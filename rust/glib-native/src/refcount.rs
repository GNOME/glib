//! Reference counting types matching `grefcount.h` / `grefcount.c`.

use std::sync::atomic::{AtomicI32, Ordering};

/// Initial value for [`RefCount`] (`G_REF_COUNT_INIT`).
pub const REF_COUNT_INIT: i32 = -1;

/// Initial value for [`AtomicRefCount`] (`G_ATOMIC_REF_COUNT_INIT`).
pub const ATOMIC_REF_COUNT_INIT: i32 = 1;

/// Non-atomic reference count for single-threaded use (`grefcount`).
///
/// Uses GLib's negative-range encoding: initialized to `-1` (one reference),
/// `inc` moves toward `i32::MIN`, `dec` moves toward `0`.
#[derive(Debug, Clone, Copy)]
pub struct RefCount(i32);

impl RefCount {
    /// Initialize to one reference (`g_ref_count_init`).
    #[inline]
    pub fn new() -> Self {
        Self(REF_COUNT_INIT)
    }

    /// Increase the reference count (`g_ref_count_inc`).
    pub fn inc(&mut self) {
        debug_assert!(self.0 < 0, "refcount not initialized");
        if self.0 == i32::MIN {
            eprintln!("glib-native: reference count has reached saturation");
            return;
        }
        self.0 -= 1;
    }

    /// Decrease the reference count (`g_ref_count_dec`).
    ///
    /// Returns `true` when the count reaches zero (object may be freed).
    pub fn dec(&mut self) -> bool {
        debug_assert!(self.0 < 0, "refcount not initialized");
        let next = self.0 + 1;
        if next == 0 {
            return true;
        }
        self.0 = next;
        false
    }

    /// Compare the logical reference count to `val` (`g_ref_count_compare`).
    pub fn compare(&self, val: i32) -> bool {
        debug_assert!(val >= 0);
        if val == i32::MAX {
            self.0 == i32::MIN
        } else {
            self.0 == -val
        }
    }

    /// Raw internal value (for tests mirroring C introspection).
    #[doc(hidden)]
    pub fn raw(&self) -> i32 {
        self.0
    }
}

impl Default for RefCount {
    fn default() -> Self {
        Self::new()
    }
}

/// Atomic reference count for multi-threaded use (`gatomicrefcount`).
#[derive(Debug)]
pub struct AtomicRefCount(AtomicI32);

impl AtomicRefCount {
    /// Initialize to one reference (`g_atomic_ref_count_init`).
    #[inline]
    pub fn new() -> Self {
        Self(AtomicI32::new(ATOMIC_REF_COUNT_INIT))
    }

    /// Atomically increase the reference count (`g_atomic_ref_count_inc`).
    pub fn inc(&self) {
        loop {
            let old = self.0.load(Ordering::Relaxed);
            if old <= 0 {
                debug_assert!(false, "refcount not initialized");
                return;
            }
            if old == i32::MAX {
                eprintln!("glib-native: reference count has reached saturation");
                return;
            }
            if self
                .0
                .compare_exchange_weak(old, old + 1, Ordering::Relaxed, Ordering::Relaxed)
                .is_ok()
            {
                return;
            }
        }
    }

    /// Atomically decrease the reference count (`g_atomic_ref_count_dec`).
    ///
    /// Returns `true` when the count reaches zero.
    pub fn dec(&self) -> bool {
        let old = self.0.fetch_sub(1, Ordering::Release);
        debug_assert!(old > 0, "refcount not initialized");
        if old == 1 {
            std::sync::atomic::fence(Ordering::Acquire);
            return true;
        }
        false
    }

    /// Atomically compare to `val` (`g_atomic_ref_count_compare`).
    pub fn compare(&self, val: i32) -> bool {
        debug_assert!(val >= 0);
        self.0.load(Ordering::Relaxed) == val
    }

    /// Raw internal value (for tests).
    #[doc(hidden)]
    pub fn raw(&self) -> i32 {
        self.0.load(Ordering::Relaxed)
    }
}

impl Default for AtomicRefCount {
    fn default() -> Self {
        Self::new()
    }
}

impl Clone for AtomicRefCount {
    fn clone(&self) -> Self {
        let v = self.0.load(Ordering::Relaxed);
        Self(AtomicI32::new(v))
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn grefcount_lifecycle() {
        let mut a = RefCount::new();
        assert!(a.compare(1));

        a.inc();
        assert!(!a.compare(1));
        assert!(!a.compare(i32::MAX));

        let mut b = a;
        a.inc();

        assert!(!b.dec());
        assert!(!a.dec());
        assert!(b.dec());
        assert!(!a.dec());
        assert!(a.dec());
    }

    #[test]
    fn grefcount_saturation() {
        let mut a = RefCount(i32::MIN + 1);
        a.inc();
        assert_eq!(a.raw(), i32::MIN);
        a.inc();
        assert_eq!(a.raw(), i32::MIN);
    }

    #[test]
    fn gatomicrefcount_lifecycle() {
        let a = AtomicRefCount::new();
        assert!(a.compare(1));

        a.inc();
        assert!(!a.compare(1));
        assert!(!a.compare(i32::MAX));

        let b = a.clone();
        a.inc();

        assert!(!b.dec());
        assert!(!a.dec());
        assert!(b.dec());
        assert!(!a.dec());
        assert!(a.dec());
    }

    #[test]
    fn gatomicrefcount_saturation() {
        let a = AtomicRefCount(AtomicI32::new(i32::MAX - 1));
        a.inc();
        assert_eq!(a.raw(), i32::MAX);
        a.inc();
        assert_eq!(a.raw(), i32::MAX);
    }
}

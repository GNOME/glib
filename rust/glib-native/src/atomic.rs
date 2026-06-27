//! Atomic operations matching `gatomic.h` / `gatomic.c`.

use std::sync::atomic::{AtomicI32, AtomicU32, AtomicUsize, Ordering};

/// GLib uses sequentially consistent atomics (`__ATOMIC_SEQ_CST`).
const ORDER: Ordering = Ordering::SeqCst;

/// Atomic signed integer (`gint` / `g_atomic_int_*`).
#[derive(Debug)]
pub struct AtomicInt(AtomicI32);

/// Atomic unsigned integer (`guint` / `g_atomic_int_*` on unsigned storage).
#[derive(Debug)]
pub struct AtomicUInt(AtomicU32);

/// Atomic pointer-sized value (`gpointer` / `g_atomic_pointer_*`).
#[derive(Debug)]
pub struct AtomicPointer(AtomicUsize);

impl AtomicInt {
    /// Create with initial value.
    #[inline]
    pub fn new(val: i32) -> Self {
        Self(AtomicI32::new(val))
    }

    /// Gets the current value (`g_atomic_int_get`).
    #[inline]
    pub fn get(&self) -> i32 {
        self.0.load(ORDER)
    }

    /// Sets the value (`g_atomic_int_set`).
    #[inline]
    pub fn set(&self, newval: i32) {
        self.0.store(newval, ORDER);
    }

    /// Increments by 1 (`g_atomic_int_inc`).
    #[inline]
    pub fn inc(&self) {
        self.0.fetch_add(1, ORDER);
    }

    /// Decrements by 1; returns `true` if the result is zero (`g_atomic_int_dec_and_test`).
    #[inline]
    pub fn dec_and_test(&self) -> bool {
        self.0.fetch_sub(1, ORDER) == 1
    }

    /// Compare-and-exchange (`g_atomic_int_compare_and_exchange`).
    #[inline]
    pub fn compare_and_exchange(&self, oldval: i32, newval: i32) -> bool {
        self.0
            .compare_exchange(oldval, newval, ORDER, ORDER)
            .is_ok()
    }

    /// Compare-and-exchange, storing the previous value (`g_atomic_int_compare_and_exchange_full`).
    #[inline]
    pub fn compare_and_exchange_full(&self, oldval: i32, newval: i32, preval: &mut i32) -> bool {
        *preval = oldval;
        match self.0.compare_exchange(oldval, newval, ORDER, ORDER) {
            Ok(prev) => {
                *preval = prev;
                true
            }
            Err(current) => {
                *preval = current;
                false
            }
        }
    }

    /// Exchange and return the previous value (`g_atomic_int_exchange`).
    #[inline]
    pub fn exchange(&self, newval: i32) -> i32 {
        self.0.swap(newval, ORDER)
    }

    /// Add and return the previous value (`g_atomic_int_add`).
    #[inline]
    pub fn add(&self, val: i32) -> i32 {
        self.0.fetch_add(val, ORDER)
    }

    /// Bitwise and; returns the previous value as unsigned (`g_atomic_int_and`).
    #[inline]
    pub fn and(&self, val: u32) -> u32 {
        self.0.fetch_and(val as i32, ORDER) as u32
    }

    /// Bitwise or; returns the previous value as unsigned (`g_atomic_int_or`).
    #[inline]
    pub fn or(&self, val: u32) -> u32 {
        self.0.fetch_or(val as i32, ORDER) as u32
    }

    /// Bitwise xor; returns the previous value as unsigned (`g_atomic_int_xor`).
    #[inline]
    pub fn xor(&self, val: u32) -> u32 {
        self.0.fetch_xor(val as i32, ORDER) as u32
    }

    /// Raw internal value (for tests).
    #[doc(hidden)]
    pub fn raw(&self) -> i32 {
        self.get()
    }
}

impl Default for AtomicInt {
    fn default() -> Self {
        Self::new(0)
    }
}

impl AtomicUInt {
    /// Create with initial value.
    #[inline]
    pub fn new(val: u32) -> Self {
        Self(AtomicU32::new(val))
    }

    /// Gets the current value (`g_atomic_int_get`).
    #[inline]
    pub fn get(&self) -> u32 {
        self.0.load(ORDER)
    }

    /// Sets the value (`g_atomic_int_set`).
    #[inline]
    pub fn set(&self, newval: u32) {
        self.0.store(newval, ORDER);
    }

    /// Increments by 1 (`g_atomic_int_inc`).
    #[inline]
    pub fn inc(&self) {
        self.0.fetch_add(1, ORDER);
    }

    /// Decrements by 1; returns `true` if the result is zero (`g_atomic_int_dec_and_test`).
    #[inline]
    pub fn dec_and_test(&self) -> bool {
        self.0.fetch_sub(1, ORDER) == 1
    }

    /// Compare-and-exchange (`g_atomic_int_compare_and_exchange`).
    #[inline]
    pub fn compare_and_exchange(&self, oldval: u32, newval: u32) -> bool {
        self.0
            .compare_exchange(oldval, newval, ORDER, ORDER)
            .is_ok()
    }

    /// Compare-and-exchange, storing the previous value (`g_atomic_int_compare_and_exchange_full`).
    #[inline]
    pub fn compare_and_exchange_full(&self, oldval: u32, newval: u32, preval: &mut u32) -> bool {
        *preval = oldval;
        match self.0.compare_exchange(oldval, newval, ORDER, ORDER) {
            Ok(prev) => {
                *preval = prev;
                true
            }
            Err(current) => {
                *preval = current;
                false
            }
        }
    }

    /// Exchange and return the previous value (`g_atomic_int_exchange`).
    #[inline]
    pub fn exchange(&self, newval: u32) -> u32 {
        self.0.swap(newval, ORDER)
    }

    /// Add and return the previous value (`g_atomic_int_add`).
    #[inline]
    pub fn add(&self, val: i32) -> u32 {
        self.0.fetch_add(val as u32, ORDER)
    }

    /// Bitwise and; returns the previous value (`g_atomic_int_and`).
    #[inline]
    pub fn and(&self, val: u32) -> u32 {
        self.0.fetch_and(val, ORDER)
    }

    /// Bitwise or; returns the previous value (`g_atomic_int_or`).
    #[inline]
    pub fn or(&self, val: u32) -> u32 {
        self.0.fetch_or(val, ORDER)
    }

    /// Bitwise xor; returns the previous value (`g_atomic_int_xor`).
    #[inline]
    pub fn xor(&self, val: u32) -> u32 {
        self.0.fetch_xor(val, ORDER)
    }

    /// Raw internal value (for tests).
    #[doc(hidden)]
    pub fn raw(&self) -> u32 {
        self.get()
    }
}

impl Default for AtomicUInt {
    fn default() -> Self {
        Self::new(0)
    }
}

impl AtomicPointer {
    /// Create with initial null pointer.
    #[inline]
    pub fn new() -> Self {
        Self(AtomicUsize::new(0))
    }

    /// Create from a raw pointer value.
    #[inline]
    pub fn from_ptr<T>(ptr: *mut T) -> Self {
        Self(AtomicUsize::new(ptr as usize))
    }

    /// Gets the current pointer (`g_atomic_pointer_get`).
    #[inline]
    pub fn get<T>(&self) -> *mut T {
        self.0.load(ORDER) as *mut T
    }

    /// Sets the pointer (`g_atomic_pointer_set`).
    #[inline]
    pub fn set<T>(&self, newval: *mut T) {
        self.0.store(newval as usize, ORDER);
    }

    /// Compare-and-exchange (`g_atomic_pointer_compare_and_exchange`).
    #[inline]
    pub fn compare_and_exchange<T>(&self, oldval: *mut T, newval: *mut T) -> bool {
        self.0
            .compare_exchange(oldval as usize, newval as usize, ORDER, ORDER)
            .is_ok()
    }

    /// Compare-and-exchange, storing the previous pointer (`g_atomic_pointer_compare_and_exchange_full`).
    #[inline]
    pub fn compare_and_exchange_full<T>(
        &self,
        oldval: *mut T,
        newval: *mut T,
        preval: &mut *mut T,
    ) -> bool {
        *preval = oldval;
        match self
            .0
            .compare_exchange(oldval as usize, newval as usize, ORDER, ORDER)
        {
            Ok(prev) => {
                *preval = prev as *mut T;
                true
            }
            Err(current) => {
                *preval = current as *mut T;
                false
            }
        }
    }

    /// Exchange and return the previous pointer (`g_atomic_pointer_exchange`).
    #[inline]
    pub fn exchange<T>(&self, newval: *mut T) -> *mut T {
        self.0.swap(newval as usize, ORDER) as *mut T
    }

    /// Add a signed offset to the pointer-as-integer (`g_atomic_pointer_add`).
    #[inline]
    pub fn add(&self, val: isize) -> isize {
        let old = if val >= 0 {
            self.0.fetch_add(val as usize, ORDER)
        } else {
            self.0.fetch_sub((-val) as usize, ORDER)
        };
        old as isize
    }

    /// Bitwise and; returns the previous value (`g_atomic_pointer_and`).
    #[inline]
    pub fn and(&self, val: usize) -> usize {
        self.0.fetch_and(val, ORDER)
    }

    /// Bitwise or; returns the previous value (`g_atomic_pointer_or`).
    #[inline]
    pub fn or(&self, val: usize) -> usize {
        self.0.fetch_or(val, ORDER)
    }

    /// Bitwise xor; returns the previous value (`g_atomic_pointer_xor`).
    #[inline]
    pub fn xor(&self, val: usize) -> usize {
        self.0.fetch_xor(val, ORDER)
    }

    /// Raw internal value (for tests).
    #[doc(hidden)]
    pub fn raw(&self) -> usize {
        self.0.load(ORDER)
    }
}

impl Default for AtomicPointer {
    fn default() -> Self {
        Self::new()
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::ffi::CStr;
    use std::sync::Arc;
    use std::thread;

    static HELLO: &[u8] = b"Hello\0";

    const THREADS: usize = 10;
    const ROUNDS: i32 = 10_000;

    /// Mirrors `test_types` for unsigned `guint` storage (`glib/tests/atomic.c`).
    #[test]
    fn atomic_types_unsigned() {
        let u = AtomicUInt::new(0);
        let mut u2;

        u.set(5);
        u2 = u.get();
        assert_eq!(u2, 5);

        assert!(!u.compare_and_exchange(6, 7));
        assert_eq!(u.raw(), 5);

        u2 = 0;
        assert!(!u.compare_and_exchange_full(6, 7, &mut u2));
        assert_eq!(u.raw(), 5);
        assert_eq!(u2, 5);

        u.add(1);
        assert_eq!(u.raw(), 6);
        u.inc();
        assert_eq!(u.raw(), 7);
        assert!(!u.dec_and_test());
        assert_eq!(u.raw(), 6);

        u2 = u.and(5);
        assert_eq!(u2, 6);
        assert_eq!(u.raw(), 4);
        u2 = u.or(8);
        assert_eq!(u2, 4);
        assert_eq!(u.raw(), 12);
        u2 = u.xor(4);
        assert_eq!(u2, 12);
        assert_eq!(u.raw(), 8);
        u2 = u.exchange(55);
        assert_eq!(u2, 8);
        assert_eq!(u.raw(), 55);
    }

    /// Mirrors `test_types` for signed `gint` storage.
    #[test]
    fn atomic_types_signed() {
        let s = AtomicInt::new(0);
        let mut s2;

        s.set(5);
        s2 = s.get();
        assert_eq!(s2, 5);

        assert!(!s.compare_and_exchange(6, 7));
        assert_eq!(s.raw(), 5);

        s2 = 0;
        assert!(!s.compare_and_exchange_full(6, 7, &mut s2));
        assert_eq!(s.raw(), 5);
        assert_eq!(s2, 5);

        s.add(1);
        assert_eq!(s.raw(), 6);
        s.inc();
        assert_eq!(s.raw(), 7);
        assert!(!s.dec_and_test());
        assert_eq!(s.raw(), 6);

        s2 = s.and(5) as i32;
        assert_eq!(s2, 6);
        assert_eq!(s.raw(), 4);
        s2 = s.or(8) as i32;
        assert_eq!(s2, 4);
        assert_eq!(s.raw(), 12);
        s2 = s.xor(4) as i32;
        assert_eq!(s2, 12);
        assert_eq!(s.raw(), 8);
        s2 = s.exchange(55);
        assert_eq!(s2, 8);
        assert_eq!(s.raw(), 55);
    }

    /// Mirrors pointer operations in `test_types`.
    #[test]
    fn atomic_types_pointer() {
        let mut s = 0i32;
        let vp = AtomicPointer::new();
        let mut cp: *mut i32 = std::ptr::null_mut();

        vp.set::<()>(std::ptr::null_mut());
        assert!(vp.get::<()>().is_null());

        assert!(!vp.compare_and_exchange(&mut s, &mut s));
        assert!(vp.get::<i32>().is_null());

        assert!(!vp.compare_and_exchange_full(&mut s, &mut s, &mut cp));
        assert!(cp.is_null());
        assert!(vp.get::<i32>().is_null());

        assert!(vp.compare_and_exchange::<()>(std::ptr::null_mut(), std::ptr::null_mut()));
        assert!(vp.get::<()>().is_null());

        assert!(vp.exchange(&mut s).is_null());
        assert_eq!(vp.get::<i32>(), &mut s as *mut i32);

        assert!(vp.compare_and_exchange_full(&mut s, std::ptr::null_mut(), &mut cp));
        assert_eq!(cp, &mut s as *mut i32);
        assert!(vp.get::<i32>().is_null());

        let str_ptr = HELLO.as_ptr().cast_mut();
        let vp_str = AtomicPointer::new();
        vp_str.set::<u8>(std::ptr::null_mut());
        assert!(vp_str.compare_and_exchange::<u8>(std::ptr::null_mut(), str_ptr));
        assert_eq!(
            unsafe { CStr::from_ptr(vp_str.exchange::<u8>(std::ptr::null_mut()).cast()) },
            CStr::from_bytes_with_nul(HELLO).unwrap()
        );
        assert!(vp_str.get::<()>().is_null());

        let mut vp_str2: *mut u8 = std::ptr::null_mut();
        assert!(vp_str.compare_and_exchange_full(std::ptr::null_mut(), str_ptr, &mut vp_str2));
        assert_eq!(
            unsafe { CStr::from_ptr(vp_str.get::<i8>()) },
            CStr::from_bytes_with_nul(HELLO).unwrap()
        );
        assert!(vp_str2.is_null());

        assert!(vp_str.compare_and_exchange_full(str_ptr, std::ptr::null_mut(), &mut vp_str2));
        assert!(vp_str.get::<()>().is_null());
        assert_eq!(vp_str2, str_ptr);

        let ip_atomic = AtomicPointer::new();
        ip_atomic.set::<()>(std::ptr::null_mut());
        assert!(ip_atomic.get::<i32>().is_null());
        assert!(ip_atomic.compare_and_exchange::<()>(std::ptr::null_mut(), std::ptr::null_mut()));

        let mut ip2: *mut i32 = std::ptr::null_mut();
        assert!(ip_atomic.compare_and_exchange_full(std::ptr::null_mut(), &mut s, &mut ip2));
        assert_eq!(ip_atomic.get::<i32>(), &mut s as *mut i32);
        assert!(ip2.is_null());

        assert!(!ip_atomic.compare_and_exchange_full(
            std::ptr::null_mut(),
            std::ptr::null_mut(),
            &mut ip2
        ));
        assert_eq!(ip_atomic.get::<i32>(), &mut s as *mut i32);
        assert_eq!(ip2, &mut s as *mut i32);
    }

    /// Mirrors `guintptr` pointer operations in `test_types`.
    #[test]
    fn atomic_types_uintptr() {
        let gu = AtomicPointer::new();
        let mut gu2;
        let vp_str = AtomicPointer::new();

        gu.set::<()>(std::ptr::null_mut());
        gu2 = gu.raw();
        assert_eq!(gu2, 0);

        assert!(gu.compare_and_exchange::<()>(std::ptr::null_mut(), std::ptr::null_mut()));
        assert_eq!(gu.raw(), 0);

        let mut cp: *mut () = std::ptr::null_mut();
        assert!(gu.compare_and_exchange_full(std::ptr::null_mut(), std::ptr::null_mut(), &mut cp));
        assert_eq!(gu.raw(), 0);
        assert!(cp.is_null());

        gu2 = gu.add(5) as usize;
        assert_eq!(gu2, 0);
        assert_eq!(gu.raw(), 5);
        gu2 = gu.and(6);
        assert_eq!(gu2, 5);
        assert_eq!(gu.raw(), 4);
        gu2 = gu.or(8);
        assert_eq!(gu2, 4);
        assert_eq!(gu.raw(), 12);
        gu2 = gu.xor(4);
        assert_eq!(gu2, 12);
        assert_eq!(gu.raw(), 8);

        vp_str.set(HELLO.as_ptr().cast_mut());
        let prev: *mut u8 = vp_str.exchange::<u8>(std::ptr::null_mut());
        assert!(vp_str.get::<()>().is_null());
        assert_eq!(
            unsafe { CStr::from_ptr(prev.cast()) },
            CStr::from_bytes_with_nul(HELLO).unwrap()
        );
    }

    /// Mirrors const-pointer reads at the end of `test_types`.
    #[test]
    fn atomic_types_const_reads() {
        let s = AtomicInt::new(55);
        let csp: &AtomicInt = &s;
        let cspp: &&AtomicInt = &csp;

        assert_eq!(csp.get(), 55);
        assert_eq!(*cspp as *const AtomicInt, csp as *const AtomicInt);
    }

    /// Mirrors `test_threaded` from `glib/tests/atomic.c`.
    #[test]
    fn atomic_threaded() {
        let atomic = Arc::new(AtomicInt::new(0));
        let mut bucket = [0i32; THREADS];

        let handles: Vec<_> = (0..THREADS)
            .map(|idx| {
                let atomic = Arc::clone(&atomic);
                thread::spawn(move || {
                    let mut local_sum = 0i32;
                    for i in 0..ROUNDS {
                        // Deterministic stand-in for g_random_int_range(-10, 100).
                        let d = (idx as i32 * 17 + i * 31) % 110 - 10;
                        local_sum += d;
                        atomic.add(d);
                        thread::yield_now();
                    }
                    (idx, local_sum)
                })
            })
            .collect();

        for handle in handles {
            let (idx, local_sum) = handle.join().unwrap();
            bucket[idx] = local_sum;
        }

        let sum: i32 = bucket.iter().sum();
        assert_eq!(sum, atomic.get());
    }
}

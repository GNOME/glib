//! Overflow-checked unsigned integer arithmetic matching `gtypes.h`.

use crate::Size;

/// Checked `u32` addition. Returns `Some(sum)` on success, `None` on overflow.
#[inline]
pub fn checked_add_u32(a: u32, b: u32) -> Option<u32> {
    a.checked_add(b)
}

/// Checked `u32` multiplication.
#[inline]
pub fn checked_mul_u32(a: u32, b: u32) -> Option<u32> {
    a.checked_mul(b)
}

/// Checked `usize` addition (`g_size_checked_add`).
#[inline]
pub fn checked_add_size(a: Size, b: Size) -> Option<Size> {
    a.checked_add(b)
}

/// Checked `usize` multiplication (`g_size_checked_mul`).
#[inline]
pub fn checked_mul_size(a: Size, b: Size) -> Option<Size> {
    a.checked_mul(b)
}

/// Checked `u64` addition.
#[inline]
pub fn checked_add_u64(a: u64, b: u64) -> Option<u64> {
    a.checked_add(b)
}

/// Checked `u64` multiplication.
#[inline]
pub fn checked_mul_u64(a: u64, b: u64) -> Option<u64> {
    a.checked_mul(b)
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn add_overflow() {
        assert_eq!(checked_add_u32(1, 2), Some(3));
        assert_eq!(checked_add_u32(u32::MAX, 1), None);
    }

    #[test]
    fn mul_overflow() {
        assert_eq!(checked_mul_u32(3, 4), Some(12));
        assert_eq!(checked_mul_u32(u32::MAX, 2), None);
    }

    #[test]
    fn size_checked() {
        assert!(checked_add_size(usize::MAX, 1).is_none());
        assert!(checked_mul_size(usize::MAX, 2).is_none());
    }
}

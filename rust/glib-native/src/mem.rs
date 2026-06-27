//! Memory allocation helpers matching `gmem.h` / `gmem.c`.
//!
//! GLib's `g_malloc` family returns raw pointers freed with `g_free`. In this
//! crate the same roles are filled by [`Vec<u8>`] (and [`AlignedBuffer`] for
//! aligned allocations): dropping the value is equivalent to `g_free`.

use crate::checked::checked_mul_size;
use crate::Size;
use std::alloc::{alloc, alloc_zeroed, dealloc, Layout};
use std::ops::{Deref, DerefMut};
use std::ptr::NonNull;

/// Default memory alignment (`G_MEM_ALIGN`, `sizeof(void*)` on typical targets).
pub const MEM_ALIGN: Size = std::mem::size_of::<usize>();

/// Allocate `n_bytes` uninitialized bytes (`g_malloc`).
///
/// Returns an empty vector when `n_bytes` is 0 (GLib returns `NULL`).
/// Panics on overflow or out-of-memory, matching GLib's abort-on-failure behavior.
pub fn malloc(n_bytes: Size) -> Vec<u8> {
    try_malloc(n_bytes).unwrap_or_else(|| alloc_failed(n_bytes))
}

/// Allocate `n_bytes` zero-initialized bytes (`g_malloc0`).
pub fn malloc0(n_bytes: Size) -> Vec<u8> {
    try_malloc0(n_bytes).unwrap_or_else(|| alloc_failed(n_bytes))
}

/// Reallocate `buf` to `n_bytes` (`g_realloc`).
///
/// When `n_bytes` is 0 the buffer is freed and an empty vector is returned.
pub fn realloc(buf: Vec<u8>, n_bytes: Size) -> Vec<u8> {
    try_realloc(buf, n_bytes).unwrap_or_else(|| alloc_failed(n_bytes))
}

/// Fallible allocation (`g_try_malloc`).
pub fn try_malloc(n_bytes: Size) -> Option<Vec<u8>> {
    if n_bytes == 0 {
        return Some(Vec::new());
    }
    try_alloc_uninit(n_bytes)
}

/// Fallible zero-filled allocation (`g_try_malloc0`).
pub fn try_malloc0(n_bytes: Size) -> Option<Vec<u8>> {
    if n_bytes == 0 {
        return Some(Vec::new());
    }
    try_alloc_zeroed(n_bytes)
}

/// Fallible reallocation (`g_try_realloc`).
pub fn try_realloc(buf: Vec<u8>, n_bytes: Size) -> Option<Vec<u8>> {
    if n_bytes == 0 {
        return Some(Vec::new());
    }
    let old_len = buf.len();
    if buf.capacity() >= n_bytes {
        let mut buf = buf;
        unsafe {
            buf.set_len(n_bytes);
        }
        return Some(buf);
    }
    let mut new = try_alloc_uninit(n_bytes)?;
    let copy = old_len.min(n_bytes);
    if copy > 0 {
        new[..copy].copy_from_slice(&buf[..copy]);
    }
    Some(new)
}

/// Allocate `n_blocks * n_block_bytes` bytes with overflow checking (`g_malloc_n`).
pub fn malloc_n(n_blocks: Size, n_block_bytes: Size) -> Vec<u8> {
    let len = checked_mul_size(n_blocks, n_block_bytes).unwrap_or_else(|| {
        alloc_overflow(n_blocks, n_block_bytes);
    });
    malloc(len)
}

/// Allocate zero-filled `n_blocks * n_block_bytes` bytes (`g_malloc0_n`).
pub fn malloc0_n(n_blocks: Size, n_block_bytes: Size) -> Vec<u8> {
    let len = checked_mul_size(n_blocks, n_block_bytes).unwrap_or_else(|| {
        alloc_overflow(n_blocks, n_block_bytes);
    });
    malloc0(len)
}

/// Reallocate to `n_blocks * n_block_bytes` bytes (`g_realloc_n`).
pub fn realloc_n(buf: Vec<u8>, n_blocks: Size, n_block_bytes: Size) -> Vec<u8> {
    let len = checked_mul_size(n_blocks, n_block_bytes).unwrap_or_else(|| {
        alloc_overflow(n_blocks, n_block_bytes);
    });
    realloc(buf, len)
}

/// Fallible `n_blocks * n_block_bytes` allocation (`g_try_malloc_n`).
pub fn try_malloc_n(n_blocks: Size, n_block_bytes: Size) -> Option<Vec<u8>> {
    let len = checked_mul_size(n_blocks, n_block_bytes)?;
    try_malloc(len)
}

/// Fallible zero-filled `n_blocks * n_block_bytes` allocation (`g_try_malloc0_n`).
pub fn try_malloc0_n(n_blocks: Size, n_block_bytes: Size) -> Option<Vec<u8>> {
    let len = checked_mul_size(n_blocks, n_block_bytes)?;
    try_malloc0(len)
}

/// Fallible `n_blocks * n_block_bytes` reallocation (`g_try_realloc_n`).
pub fn try_realloc_n(buf: Vec<u8>, n_blocks: Size, n_block_bytes: Size) -> Option<Vec<u8>> {
    let len = checked_mul_size(n_blocks, n_block_bytes)?;
    try_realloc(buf, len)
}

/// Copy `byte_size` bytes from `mem` (`g_memdup2`).
///
/// Returns `None` when `mem` is `None` or `byte_size` is 0, matching GLib's
/// `NULL` return for those cases.
pub fn memdup2(mem: Option<&[u8]>, byte_size: Size) -> Option<Vec<u8>> {
    if mem.is_none() || byte_size == 0 {
        return None;
    }
    let mem = mem.unwrap();
    debug_assert!(mem.len() >= byte_size);
    let mut buf = malloc(byte_size);
    buf.copy_from_slice(&mem[..byte_size]);
    Some(buf)
}

/// Copy `byte_size` bytes from `mem` (`g_memdup`, deprecated in GLib 2.68).
///
/// Uses `u32` for `byte_size` to mirror the C `guint` parameter.
pub fn memdup(mem: Option<&[u8]>, byte_size: u32) -> Option<Vec<u8>> {
    memdup2(mem, byte_size as Size)
}

/// Aligned allocation (`g_aligned_alloc`).
pub fn aligned_alloc(n_blocks: Size, n_block_bytes: Size, alignment: Size) -> AlignedBuffer {
    try_aligned_alloc(n_blocks, n_block_bytes, alignment).unwrap_or_else(|| {
        let size = n_blocks
            .checked_mul(n_block_bytes)
            .unwrap_or_else(|| alloc_overflow(n_blocks, n_block_bytes));
        alloc_failed(size)
    })
}

/// Aligned zero-filled allocation (`g_aligned_alloc0`).
pub fn aligned_alloc0(n_blocks: Size, n_block_bytes: Size, alignment: Size) -> AlignedBuffer {
    let mut buf = aligned_alloc(n_blocks, n_block_bytes, alignment);
    let clear_len = n_blocks
        .checked_mul(n_block_bytes)
        .unwrap_or_else(|| alloc_overflow(n_blocks, n_block_bytes));
    let fill_len = clear_len.min(buf.len());
    buf[..fill_len].fill(0);
    buf
}

/// Fallible aligned allocation.
pub fn try_aligned_alloc(
    n_blocks: Size,
    n_block_bytes: Size,
    alignment: Size,
) -> Option<AlignedBuffer> {
    validate_alignment(alignment);

    let logical_size = checked_mul_size(n_blocks, n_block_bytes)?;
    if logical_size == 0 {
        return Some(AlignedBuffer::empty());
    }

    let alloc_size = round_up_to_alignment(logical_size, alignment)?;
    let layout = Layout::from_size_align(alloc_size, alignment).ok()?;
    let ptr = unsafe { alloc(layout) };
    if ptr.is_null() {
        return None;
    }
    Some(AlignedBuffer {
        ptr: NonNull::new(ptr).unwrap(),
        len: alloc_size,
        layout,
    })
}

/// Release a [`Vec`] allocated by this module (`g_free`).
pub fn free(buf: Vec<u8>) {
    drop(buf);
}

/// Clear an optional owned value (`g_clear_pointer` without a custom destroy).
pub fn clear<T>(opt: &mut Option<T>) {
    opt.take();
}

/// Clear an optional value using a custom destructor (`g_clear_pointer`).
pub fn clear_with<T, F>(opt: &mut Option<T>, destroy: F)
where
    F: FnOnce(T),
{
    if let Some(value) = opt.take() {
        destroy(value);
    }
}

/// Take ownership out of an optional pointer (`g_steal_pointer`).
pub fn steal<T>(opt: &mut Option<T>) -> Option<T> {
    opt.take()
}

/// Buffer with explicit alignment, freed via `g_aligned_free` semantics.
pub struct AlignedBuffer {
    ptr: NonNull<u8>,
    len: Size,
    layout: Layout,
}

impl AlignedBuffer {
    fn empty() -> Self {
        Self {
            ptr: NonNull::dangling(),
            len: 0,
            layout: Layout::from_size_align(1, 1).unwrap(),
        }
    }

    /// Release aligned memory (`g_aligned_free`).
    pub fn aligned_free(self) {}
}

impl Drop for AlignedBuffer {
    fn drop(&mut self) {
        if self.len > 0 {
            unsafe {
                dealloc(self.ptr.as_ptr(), self.layout);
            }
        }
    }
}

impl Deref for AlignedBuffer {
    type Target = [u8];

    fn deref(&self) -> &[u8] {
        if self.len == 0 {
            &[]
        } else {
            unsafe { std::slice::from_raw_parts(self.ptr.as_ptr(), self.len) }
        }
    }
}

impl DerefMut for AlignedBuffer {
    fn deref_mut(&mut self) -> &mut [u8] {
        if self.len == 0 {
            &mut []
        } else {
            unsafe { std::slice::from_raw_parts_mut(self.ptr.as_ptr(), self.len) }
        }
    }
}

fn try_alloc_uninit(n_bytes: Size) -> Option<Vec<u8>> {
    let layout = Layout::array::<u8>(n_bytes).ok()?;
    let ptr = unsafe { alloc(layout) };
    if ptr.is_null() {
        return None;
    }
    Some(unsafe { Vec::from_raw_parts(ptr, n_bytes, n_bytes) })
}

fn try_alloc_zeroed(n_bytes: Size) -> Option<Vec<u8>> {
    let layout = Layout::array::<u8>(n_bytes).ok()?;
    let ptr = unsafe { alloc_zeroed(layout) };
    if ptr.is_null() {
        return None;
    }
    Some(unsafe { Vec::from_raw_parts(ptr, n_bytes, n_bytes) })
}

fn validate_alignment(alignment: Size) {
    if alignment == 0 || !alignment.is_power_of_two() {
        panic!("alignment {alignment} must be a positive power of two");
    }
    if alignment % std::mem::size_of::<usize>() != 0 {
        panic!(
            "alignment {alignment} must be a multiple of {}",
            std::mem::size_of::<usize>()
        );
    }
}

fn round_up_to_alignment(size: Size, alignment: Size) -> Option<Size> {
    if size == 0 {
        return Some(0);
    }
    let rem = size % alignment;
    if rem == 0 {
        return Some(size);
    }
    size.checked_add(alignment - rem)
}

#[cold]
#[inline(never)]
fn alloc_overflow(n_blocks: Size, n_block_bytes: Size) -> ! {
    panic!("overflow allocating {n_blocks}*{n_block_bytes} bytes");
}

#[cold]
#[inline(never)]
fn alloc_failed(n_bytes: Size) -> ! {
    panic!("failed to allocate {n_bytes} bytes");
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::sync::atomic::{AtomicUsize, Ordering};

    #[test]
    fn malloc_zero_returns_empty() {
        let buf = malloc(0);
        assert!(buf.is_empty());
        assert_eq!(try_malloc(0), Some(Vec::new()));
    }

    #[test]
    fn malloc0_zeroes() {
        let buf = malloc0(8);
        assert_eq!(buf, vec![0u8; 8]);
    }

    #[test]
    fn realloc_zero_frees() {
        let buf = malloc(10);
        assert_eq!(buf.len(), 10);
        let buf = realloc(buf, 0);
        assert!(buf.is_empty());
    }

    #[test]
    fn realloc_grow_and_shrink() {
        let buf = malloc(4);
        let buf = realloc(buf, 8);
        assert_eq!(buf.len(), 8);
        let buf = realloc(buf, 2);
        assert_eq!(buf.len(), 2);
    }

    #[test]
    fn try_realloc_null_grows() {
        let buf = try_realloc(Vec::new(), 4).unwrap();
        assert_eq!(buf.len(), 4);
    }

    #[test]
    fn malloc_n_overflow_try() {
        let a = Size::MAX / 10 + 10;
        let b = 10usize;
        assert!(try_malloc_n(a, a).is_none());
        assert!(try_malloc_n(a, b).is_none());
        assert!(try_malloc_n(b, a).is_none());
        let buf = try_malloc_n(b, b).unwrap();
        assert_eq!(buf.len(), 100);
    }

    #[test]
    fn malloc_n_overflow_panics() {
        let a = Size::MAX / 10 + 10;
        let result = std::panic::catch_unwind(|| {
            let _ = malloc_n(a, a);
        });
        assert!(result.is_err());
    }

    #[test]
    fn try_malloc0_n_and_realloc_n() {
        let a = Size::MAX / 10 + 10;
        let b = 10usize;
        assert!(try_malloc0_n(a, a).is_none());
        assert!(try_malloc0_n(a, b).is_none());
        assert!(try_malloc0_n(b, a).is_none());
        let buf = try_malloc0_n(b, b).unwrap();
        assert_eq!(buf, vec![0u8; 100]);

        let seed = malloc(1);
        assert!(try_realloc_n(seed, a, a).is_none());
        assert!(try_realloc_n(malloc(1), a, b).is_none());
        assert!(try_realloc_n(malloc(1), b, a).is_none());
        let buf = try_realloc_n(malloc(1), b, b).unwrap();
        assert_eq!(buf.len(), 100);
    }

    #[test]
    fn memdup2_cases() {
        let s = b"The quick brown fox jumps over the lazy dog";
        assert!(memdup2(None, 1024).is_none());
        assert!(memdup2(Some(s), 0).is_none());
        assert!(memdup2(None, 0).is_none());

        let dup = memdup2(Some(s), s.len()).unwrap();
        assert_eq!(dup.as_slice(), s);
    }

    #[test]
    fn memdup_cases() {
        let s = b"The quick brown fox jumps over the lazy dog";
        assert!(memdup(None, 1024).is_none());
        assert!(memdup(Some(s), 0).is_none());
        let dup = memdup(Some(s), s.len() as u32).unwrap();
        assert_eq!(dup.as_slice(), s);
    }

    #[test]
    fn clear_and_steal() {
        let mut opt = Some(String::from("x"));
        clear(&mut opt);
        assert!(opt.is_none());

        let destroyed = AtomicUsize::new(0);
        let mut opt = Some(42usize);
        clear_with(&mut opt, |_| {
            destroyed.fetch_add(1, Ordering::SeqCst);
        });
        assert!(opt.is_none());
        assert_eq!(destroyed.load(Ordering::SeqCst), 1);

        let mut opt = Some(7u8);
        assert_eq!(steal(&mut opt), Some(7));
        assert!(opt.is_none());
    }

    #[test]
    fn aligned_alloc_valid() {
        let b = 10usize;
        let buf = aligned_alloc(b, std::mem::size_of::<u64>(), 16);
        assert!(buf.len() >= b * std::mem::size_of::<u64>());
        assert_eq!(buf.as_ptr() as usize % 16, 0);

        let buf0 = aligned_alloc0(b, std::mem::size_of::<u64>(), 16);
        let logical = b * std::mem::size_of::<u64>();
        assert_eq!(&buf0[..logical], vec![0u8; logical].as_slice());
    }

    #[test]
    fn aligned_alloc_overflow_panics() {
        let a = Size::MAX / 10 + 10;
        let result = std::panic::catch_unwind(|| {
            let _ = aligned_alloc(a, std::mem::size_of::<u8>(), 16);
        });
        assert!(result.is_err());
    }

    #[test]
    fn aligned_alloc_zero_size() {
        let buf = aligned_alloc(0, 16, 16);
        assert!(buf.is_empty());
    }

    #[test]
    fn free_drops() {
        free(malloc(4));
    }
}

//! Immutable reference-counted byte buffer matching `GBytes`.

use crate::checked::{checked_add_size, checked_mul_size};
use crate::Size;
use std::fmt;
use std::ops::Deref;
use std::sync::Arc;

/// Maximum payload size stored inline (matches `G_BYTES_MAX_INLINE` on 64-bit).
const BYTES_MAX_INLINE: usize = if cfg!(target_pointer_width = "64") {
    128 - 6 * std::mem::size_of::<usize>()
} else {
    64 - 6 * std::mem::size_of::<usize>()
};

enum BytesStorage {
    /// Owned or copied data.
    Owned(Vec<u8>),
    /// Owned buffer with a caller-supplied destructor.
    OwnedWithFree(OwnedWithFree),
    /// Static data that outlives the process (`g_bytes_new_static`).
    Static(&'static [u8]),
    /// Subslice referencing another [`Bytes`] (`g_bytes_new_from_bytes`).
    Slice {
        parent: Arc<BytesInner>,
        offset: Size,
        len: Size,
    },
}

struct OwnedWithFree {
    data: Vec<u8>,
    free_fn: Option<Box<dyn FnOnce() + Send + Sync>>,
}

impl Drop for OwnedWithFree {
    fn drop(&mut self) {
        if let Some(f) = self.free_fn.take() {
            f();
        }
    }
}

struct BytesInner {
    storage: BytesStorage,
}

/// Immutable reference-counted byte sequence (`GBytes`).
#[derive(Clone)]
pub struct Bytes {
    inner: Arc<BytesInner>,
}

impl Bytes {
    /// Create from a copied buffer (`g_bytes_new`).
    pub fn new(data: impl AsRef<[u8]>) -> Self {
        let data = data.as_ref();
        if data.is_empty() {
            return Self::from_owned(Vec::new());
        }
        if data.len() <= BYTES_MAX_INLINE {
            return Self::from_owned(data.to_vec());
        }
        Self::from_owned(data.to_vec())
    }

    /// Take ownership of a `Vec` (`g_bytes_new_take` with `g_free`).
    pub fn from_vec(data: Vec<u8>) -> Self {
        Self::from_owned(data)
    }

    /// Reference static data (`g_bytes_new_static`).
    pub fn from_static(data: &'static [u8]) -> Self {
        Self {
            inner: Arc::new(BytesInner {
                storage: BytesStorage::Static(data),
            }),
        }
    }

    /// Create with a custom drop callback (`g_bytes_new_with_free_func`).
    ///
    /// Phase 1 copies into an owned buffer; the callback runs when the last
    /// reference is dropped.
    pub fn new_with_free_fn(data: &[u8], free_fn: impl FnOnce() + Send + Sync + 'static) -> Self {
        Self {
            inner: Arc::new(BytesInner {
                storage: BytesStorage::OwnedWithFree(OwnedWithFree {
                    data: data.to_vec(),
                    free_fn: Some(Box::new(free_fn)),
                }),
            }),
        }
    }

    /// Create a subslice (`g_bytes_new_from_bytes`).
    pub fn slice(&self, offset: Size, length: Size) -> Self {
        assert!(offset <= self.len());
        assert!(length <= self.len() - offset);

        if offset == 0 && length == self.len() {
            return self.clone();
        }

        let root = self.root_inner();
        let abs_offset = self.offset_in_root() + offset;

        Self {
            inner: Arc::new(BytesInner {
                storage: BytesStorage::Slice {
                    parent: root,
                    offset: abs_offset,
                    len: length,
                },
            }),
        }
    }

    /// Pointer to data (`g_bytes_get_data`).
    pub fn data(&self) -> &[u8] {
        self.as_ref()
    }

    /// Size in bytes (`g_bytes_get_size`).
    pub fn len(&self) -> Size {
        match &self.inner.storage {
            BytesStorage::Owned(v) => v.len(),
            BytesStorage::OwnedWithFree(o) => o.data.len(),
            BytesStorage::Static(s) => s.len(),
            BytesStorage::Slice { len, .. } => *len,
        }
    }

    /// Whether the buffer is empty.
    pub fn is_empty(&self) -> bool {
        self.len() == 0
    }

    /// Increase reference count (`g_bytes_ref`).
    #[must_use]
    pub fn ref_count(&self) -> Self {
        self.clone()
    }

    /// Drop a reference (`g_bytes_unref`).
    pub fn unref(self) {}

    /// Equality (`g_bytes_equal`).
    pub fn equal(&self, other: &Self) -> bool {
        self.as_ref() == other.as_ref()
    }

    /// djb2 hash (`g_bytes_hash`).
    pub fn hash(&self) -> u32 {
        let mut h: u32 = 5381;
        for byte in self.as_ref() {
            h = h
                .wrapping_shl(5)
                .wrapping_add(h)
                .wrapping_add(u32::from(*byte));
        }
        h
    }

    /// Lexicographic compare (`g_bytes_compare`).
    pub fn compare(&self, other: &Self) -> i32 {
        self.as_ref().cmp(other.as_ref()) as i32
    }

    /// Return owned copy of data (`g_bytes_unref_to_data`).
    pub fn into_vec(self) -> Vec<u8> {
        self.as_ref().to_vec()
    }

    /// Typed region access with overflow checks (`g_bytes_get_region`).
    pub fn get_region(&self, element_size: Size, offset: Size, n_elements: Size) -> Option<&[u8]> {
        if element_size == 0 {
            return None;
        }
        let total = checked_mul_size(element_size, n_elements)?;
        let end = checked_add_size(offset, total)?;
        if end > self.len() {
            return None;
        }
        Some(&self.as_ref()[offset..end])
    }

    fn from_owned(data: Vec<u8>) -> Self {
        Self {
            inner: Arc::new(BytesInner {
                storage: BytesStorage::Owned(data),
            }),
        }
    }

    fn root_inner(&self) -> Arc<BytesInner> {
        let mut current = self.inner.clone();
        loop {
            match &current.storage {
                BytesStorage::Slice { parent, .. } => current = parent.clone(),
                _ => return current,
            }
        }
    }

    fn offset_in_root(&self) -> Size {
        let mut offset = 0usize;
        let mut current = self.inner.clone();
        loop {
            match &current.storage {
                BytesStorage::Slice {
                    parent,
                    offset: off,
                    ..
                } => {
                    offset += off;
                    current = parent.clone();
                }
                _ => return offset,
            }
        }
    }
}

impl Deref for Bytes {
    type Target = [u8];

    fn deref(&self) -> &[u8] {
        match &self.inner.storage {
            BytesStorage::Owned(v) => v.as_slice(),
            BytesStorage::OwnedWithFree(o) => o.data.as_slice(),
            BytesStorage::Static(s) => s,
            BytesStorage::Slice {
                parent,
                offset,
                len,
            } => Self::slice_from_root(parent, *offset, *len),
        }
    }
}

impl Bytes {
    fn slice_from_root(parent: &Arc<BytesInner>, offset: Size, len: Size) -> &[u8] {
        match &parent.storage {
            BytesStorage::Owned(v) => &v[offset..offset + len],
            BytesStorage::OwnedWithFree(o) => &o.data[offset..offset + len],
            BytesStorage::Static(s) => &s[offset..offset + len],
            BytesStorage::Slice { .. } => {
                unreachable!("slice parent must reference root storage")
            }
        }
    }
}

impl fmt::Debug for Bytes {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        f.debug_tuple("Bytes").field(&self.as_ref()).finish()
    }
}

impl PartialEq for Bytes {
    fn eq(&self, other: &Self) -> bool {
        self.equal(other)
    }
}

impl Eq for Bytes {}

impl PartialOrd for Bytes {
    fn partial_cmp(&self, other: &Self) -> Option<std::cmp::Ordering> {
        Some(self.cmp(other))
    }
}

impl Ord for Bytes {
    fn cmp(&self, other: &Self) -> std::cmp::Ordering {
        self.as_ref().cmp(other.as_ref())
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::sync::atomic::{AtomicUsize, Ordering};

    static NYAN: [u8; 127] = {
        let mut a = [0u8; 127];
        let mut i = 0usize;
        while i < 127 {
            a[i] = (i + 1) as u8;
            i += 1;
        }
        a
    };

    #[test]
    fn new_copies() {
        let data = b"test";
        let bytes = Bytes::new(data);
        assert_ne!(bytes.as_ptr(), data.as_ptr());
        assert_eq!(bytes.len(), 4);
        assert_eq!(bytes.as_ref(), b"test");
    }

    #[test]
    fn from_vec_takes() {
        let data = b"test".to_vec();
        let ptr = data.as_ptr();
        let bytes = Bytes::from_vec(data);
        assert_eq!(bytes.as_ptr(), ptr);
        assert_eq!(bytes.len(), 4);
    }

    #[test]
    fn from_static() {
        let data = b"test";
        let bytes = Bytes::from_static(data);
        assert_eq!(bytes.as_ptr(), data.as_ptr());
        assert_eq!(bytes.len(), 4);
    }

    #[test]
    fn slice_from_bytes() {
        let bytes = Bytes::new(b"smile and wave");
        let sub = bytes.slice(10, 4);
        assert_eq!(sub.as_ref(), b"wave");
    }

    #[test]
    fn full_slice_reuses_handle() {
        let bytes = Bytes::new(b"hello");
        let sub = bytes.slice(0, 5);
        assert_eq!(Arc::strong_count(&bytes.inner), 2);
        assert_eq!(Arc::strong_count(&sub.inner), 2);
        drop(sub);
        assert_eq!(Arc::strong_count(&bytes.inner), 1);
    }

    #[test]
    fn nested_slice_references_root() {
        let bytes = Bytes::new(&NYAN);
        let sub1 = bytes.slice(10, 20);
        let sub2 = sub1.slice(5, 10);
        assert_eq!(sub2.as_ref(), &NYAN[15..25]);
    }

    #[test]
    fn equal_and_hash() {
        let a = Bytes::new(b"abc");
        let b = Bytes::new(b"abc");
        let c = Bytes::new(b"abd");
        assert!(a.equal(&b));
        assert!(!a.equal(&c));
        assert_eq!(a.hash(), b.hash());
        assert_ne!(a.hash(), c.hash());
    }

    #[test]
    fn compare_lexicographic() {
        let a = Bytes::new(b"abc");
        let b = Bytes::new(b"abd");
        assert!(a.compare(&b) < 0);
        let prefix = Bytes::new(b"ab");
        assert!(prefix.compare(&a) < 0);
    }

    #[test]
    fn get_region_bounds() {
        let bytes = Bytes::new(&[1u8, 2, 3, 4, 5, 6]);
        assert_eq!(bytes.get_region(2, 0, 0), Some(&[][..]));
        assert_eq!(bytes.get_region(2, 4, 1), Some(&[5u8, 6][..]));
        assert!(bytes.get_region(2, 5, 1).is_none());
        assert!(bytes.get_region(0, 0, 1).is_none());
        assert!(bytes.get_region(2, usize::MAX, 2).is_none());
    }

    #[test]
    fn empty_new() {
        let bytes = Bytes::new(&[]);
        assert_eq!(bytes.len(), 0);
        assert!(bytes.is_empty());
    }

    #[test]
    fn into_vec() {
        let bytes = Bytes::new(b"data");
        assert_eq!(bytes.clone().into_vec(), b"data".to_vec());
    }

    #[test]
    fn free_fn_runs_on_drop() {
        let count = Arc::new(AtomicUsize::new(0));
        let count2 = count.clone();
        let bytes = Bytes::new_with_free_fn(b"x", move || {
            count2.fetch_add(1, Ordering::SeqCst);
        });
        drop(bytes);
        assert_eq!(count.load(Ordering::SeqCst), 1);
    }
}

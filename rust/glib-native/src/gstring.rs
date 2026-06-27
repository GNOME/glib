//! Growable string buffer matching `GString`.

use crate::Size;

/// Growable byte/string buffer with explicit length (`GString`).
#[derive(Clone, Debug, PartialEq, Eq)]
pub struct GString {
    /// Buffer including a trailing nul at index `len`.
    buf: Vec<u8>,
    /// Logical length excluding the trailing nul.
    len: Size,
}

impl GString {
    /// Create from optional initial text (`g_string_new`).
    pub fn new(init: Option<&str>) -> Self {
        match init {
            None | Some("") => Self::sized_new(2),
            Some(s) => {
                let mut string = Self::sized_new(s.len() + 2);
                string.append(s);
                string
            }
        }
    }

    /// Take ownership of a nul-terminated buffer (`g_string_new_take`).
    pub fn new_take(init: Option<String>) -> Self {
        match init {
            None => Self::new(None),
            Some(s) => {
                let len = s.len();
                let mut buf = s.into_bytes();
                if buf.len() == len {
                    buf.push(0);
                } else {
                    buf.truncate(len);
                    buf.push(0);
                }
                Self { buf, len }
            }
        }
    }

    /// Create with explicit length, allowing embedded nuls (`g_string_new_len`).
    pub fn new_len(init: Option<&[u8]>, len: isize) -> Self {
        if len < 0 {
            return Self::new(init.and_then(|b| std::str::from_utf8(b).ok()));
        }
        let mut string = Self::sized_new(len as Size);
        if let Some(bytes) = init {
            string.append_len(bytes, len as Size);
        }
        string
    }

    /// Pre-allocate at least `dfl_size` bytes (`g_string_sized_new`).
    pub fn sized_new(dfl_size: Size) -> Self {
        let want = dfl_size.max(64);
        let allocated_len = nearest_pow(want + 1);
        let mut buf = Vec::with_capacity(allocated_len);
        buf.push(0);
        Self { buf, len: 0 }
    }

    /// Pointer to buffer (`str` field).
    pub fn as_ptr(&self) -> *const u8 {
        self.buf.as_ptr()
    }

    /// Contents as UTF-8 when valid.
    pub fn as_str(&self) -> &str {
        std::str::from_utf8(&self.buf[..self.len]).expect("GString UTF-8")
    }

    /// Raw contents including possible embedded nuls.
    pub fn as_bytes(&self) -> &[u8] {
        &self.buf[..self.len]
    }

    /// Logical length (`len` field).
    pub fn len(&self) -> Size {
        self.len
    }

    /// Whether the string is empty.
    pub fn is_empty(&self) -> bool {
        self.len == 0
    }

    /// Allocated buffer size (`allocated_len` field).
    pub fn allocated_len(&self) -> Size {
        self.buf.capacity()
    }

    /// Append a nul-terminated string (`g_string_append`).
    pub fn append(&mut self, val: &str) -> &mut Self {
        self.append_len(val.as_bytes(), val.len());
        self
    }

    /// Append bytes (`g_string_append_len`). `len == isize::MAX` means use all of `val`.
    pub fn append_len(&mut self, val: &[u8], len: Size) -> &mut Self {
        self.insert_len(-1, val, len)
    }

    /// Append one byte (`g_string_append_c`).
    pub fn append_c(&mut self, c: u8) -> &mut Self {
        if self.len + 2 <= self.buf.capacity() {
            if self.buf.len() < self.len + 2 {
                self.buf.resize(self.len + 2, 0);
            }
            self.buf[self.len] = c;
            self.len += 1;
            self.buf[self.len] = 0;
        } else {
            self.insert_len(-1, &[c], 1);
        }
        self
    }

    /// Truncate to at most `len` bytes (`g_string_truncate`).
    pub fn truncate(&mut self, len: Size) -> &mut Self {
        self.len = len.min(self.len);
        if self.buf.len() <= self.len {
            self.buf.resize(self.len + 1, 0);
        }
        self.buf[self.len] = 0;
        self
    }

    /// Set length, extending with undefined bytes when growing (`g_string_set_size`).
    pub fn set_size(&mut self, len: Size) -> &mut Self {
        if len + 1 > self.buf.capacity() {
            self.expand(len.saturating_sub(self.len));
        }
        if self.buf.len() < len + 1 {
            let new_len = len + 1;
            self.buf.reserve(new_len - self.buf.len());
            // SAFETY: GLib leaves grown bytes undefined; only the nul terminator is set below.
            #[allow(clippy::uninit_vec)]
            unsafe {
                self.buf.set_len(new_len);
            }
        }
        self.len = len;
        self.buf[len] = 0;
        self
    }

    /// Release buffer without freeing wrapper (`g_string_free_and_steal`).
    pub fn into_inner(mut self) -> Vec<u8> {
        self.buf.truncate(self.len);
        std::mem::take(&mut self.buf)
    }

    /// Free wrapper, optionally returning the buffer (`g_string_free`).
    pub fn free(self, free_segment: bool) -> Option<Vec<u8>> {
        if free_segment {
            None
        } else {
            Some(self.into_inner())
        }
    }

    /// Equality for hash tables (`g_string_equal`).
    pub fn equal(&self, other: &Self) -> bool {
        self.as_bytes() == other.as_bytes()
    }

    /// 31-bit hash (`g_string_hash`).
    pub fn hash(&self) -> u32 {
        let mut h: u32 = 0;
        for &byte in self.as_bytes() {
            h = h
                .wrapping_shl(5)
                .wrapping_sub(h)
                .wrapping_add(u32::from(byte));
        }
        h
    }

    fn insert_len(&mut self, pos: isize, val: &[u8], len: Size) -> &mut Self {
        if len == 0 {
            return self;
        }

        let pos = if pos < 0 { self.len } else { pos as Size };
        assert!(pos <= self.len, "insert position out of bounds");

        if self.len + len + 1 > self.buf.capacity() {
            self.expand(len);
        }

        let val_start = val.as_ptr() as usize;
        let buf_start = self.buf.as_ptr() as usize;
        let buf_end = buf_start + self.len;

        if val_start >= buf_start && val_start <= buf_end {
            let offset = val_start - buf_start;
            self.buf.resize(self.len + len + 1, 0);
            if pos < self.len {
                self.buf.copy_within(pos..self.len, pos + len);
            }
            let val_ptr = self.buf.as_ptr() as usize + offset;
            let precount = len.min(pos.saturating_sub(offset));
            if precount > 0 {
                self.buf.copy_within(offset..offset + precount, pos);
            }
            if len > precount {
                let src = val_ptr + precount + len;
                let dst = pos + precount;
                let count = len - precount;
                unsafe {
                    std::ptr::copy(src as *const u8, self.buf.as_mut_ptr().add(dst), count);
                }
            }
        } else {
            self.buf.resize(self.len + len + 1, 0);
            if pos < self.len {
                self.buf.copy_within(pos..self.len, pos + len);
            }
            self.buf[pos..pos + len].copy_from_slice(&val[..len]);
        }

        self.len += len;
        self.buf[self.len] = 0;
        self
    }

    fn expand(&mut self, extra: Size) {
        let need = self
            .len
            .checked_add(extra)
            .and_then(|n| n.checked_add(1))
            .expect("GString expansion overflow");
        let mut new_cap = nearest_pow(need);
        if new_cap == 0 {
            new_cap = need;
        }
        self.buf
            .reserve(new_cap.saturating_sub(self.buf.capacity()));
        if self.buf.len() <= self.len {
            self.buf.resize(self.len + 1, 0);
        }
    }
}

fn nearest_pow(num: Size) -> Size {
    assert!(num > 0 && num <= Size::MAX / 2);
    let mut n = num - 1;
    n |= n >> 1;
    n |= n >> 2;
    n |= n >> 4;
    n |= n >> 8;
    n |= n >> 16;
    #[cfg(target_pointer_width = "64")]
    {
        n |= n >> 32;
    }
    n + 1
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn new_and_new_len() {
        let s1 = GString::new(Some("hi pete!"));
        let s2 = GString::new(None);
        assert_eq!(s1.len(), 8);
        assert_eq!(s2.len(), 0);
        assert_eq!(s1.as_str(), "hi pete!");
        assert_eq!(s2.as_str(), "");

        let s3 = GString::new_len(Some(b"foo"), -1);
        let s4 = GString::new_len(Some(b"foobar"), 3);
        assert_eq!(s3.as_str(), "foo");
        assert_eq!(s4.as_str(), "foo");
        assert_eq!(s3.len(), 3);
        assert_eq!(s4.len(), 3);
    }

    #[test]
    fn append_and_append_c() {
        let mut s = GString::new(Some("firsthalf"));
        s.append("last");
        s.append("half");
        assert_eq!(s.as_str(), "firsthalflasthalf");

        let mut s = GString::new(Some("firsthalf"));
        s.append_len(b"lasthalfjunkjunk", 4);
        s.append_len(b"halfjunkjunk", 4);
        s.append_len(b"more", 4);
        s.append_len(b"ore", 3);
        assert_eq!(s.as_str(), "firsthalflasthalfmoreore");

        let mut s = GString::new(Some("hi pete!"));
        for i in 0..10_000u32 {
            s.append_c(b'a' + (i % 26) as u8);
        }
        assert_eq!(s.len(), 8 + 10_000);
    }

    #[test]
    fn equal() {
        let s1 = GString::new(Some("test"));
        let mut s2 = GString::new(Some("te"));
        assert!(!s1.equal(&s2));
        s2.append("st");
        assert!(s1.equal(&s2));
    }

    #[test]
    fn truncate() {
        let mut s = GString::new(Some("testing"));
        s.truncate(1000);
        assert_eq!(s.as_str(), "testing");
        s.truncate(4);
        assert_eq!(s.as_str(), "test");
        s.truncate(0);
        assert_eq!(s.as_str(), "");
    }

    #[test]
    fn set_size() {
        let mut s = GString::new(Some("foo"));
        s.set_size(30);
        assert_eq!(&s.as_bytes()[..3], b"foo");
        assert_eq!(s.len(), 30);
    }

    #[test]
    fn nul_handling() {
        let mut s1 = GString::new(Some("fiddle"));
        let mut s2 = GString::new(Some("fiddle"));
        assert!(s1.equal(&s2));
        s1.append_c(0);
        assert!(!s1.equal(&s2));
        s2.append_c(0);
        assert!(s1.equal(&s2));
        s1.append_c(b'x');
        s2.append_c(b'y');
        assert!(!s1.equal(&s2));
        assert_eq!(s1.len(), 8);
    }

    #[test]
    fn steal_and_new_take() {
        let mut s = GString::new(Some("One"));
        s.append(", two");
        s.append(", three");
        s.append_c(b'.');
        let stolen = s.free(false).unwrap();
        assert_eq!(stolen, b"One, two, three.".to_vec());

        let taken = GString::new_take(Some("test_test".to_owned()));
        assert_eq!(taken.as_str(), "test_test");

        let empty = GString::new_take(None);
        assert_eq!(empty.as_str(), "");
    }

    #[test]
    fn hash_consistent() {
        let a = GString::new(Some("abc"));
        let b = GString::new(Some("abc"));
        assert_eq!(a.hash(), b.hash());
    }

    #[test]
    fn sized_new_minimum() {
        let s = GString::sized_new(0);
        assert!(s.allocated_len() >= 64);
    }
}

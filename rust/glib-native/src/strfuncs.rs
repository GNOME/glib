//! String utility functions from `gstrfuncs.h` (non-printf subset).

use crate::checked::{checked_add_size, checked_mul_size};
use crate::Size;

/// Returns the length of `s` in bytes (`strlen`).
#[inline]
pub fn strlen(s: &str) -> Size {
    s.len()
}

/// Lexicographic compare (`strcmp`).
#[inline]
pub fn strcmp(s1: &str, s2: &str) -> i32 {
    match s1.cmp(s2) {
        std::cmp::Ordering::Less => -1,
        std::cmp::Ordering::Equal => 0,
        std::cmp::Ordering::Greater => 1,
    }
}

/// ASCII case-insensitive compare (`g_ascii_strcasecmp`).
pub fn ascii_strcasecmp(s1: &str, s2: &str) -> i32 {
    let mut i1 = s1.bytes();
    let mut i2 = s2.bytes();

    loop {
        match (i1.next(), i2.next()) {
            (Some(c1), Some(c2)) => {
                let c1 = c1.to_ascii_lowercase();
                let c2 = c2.to_ascii_lowercase();
                if c1 != c2 {
                    return i32::from(c1) - i32::from(c2);
                }
            }
            (Some(c1), None) => return i32::from(c1),
            (None, Some(c2)) => return -i32::from(c2),
            (None, None) => return 0,
        }
    }
}

/// ASCII case-insensitive compare (`g_strcasecmp` without locale).
#[inline]
pub fn strcasecmp(s1: &str, s2: &str) -> i32 {
    ascii_strcasecmp(s1, s2)
}

/// Whether `str` begins with `prefix` (`g_str_has_prefix`).
pub fn str_has_prefix(str: &str, prefix: &str) -> bool {
    str.as_bytes().starts_with(prefix.as_bytes())
}

/// Whether `str` ends with `suffix` (`g_str_has_suffix`).
pub fn str_has_suffix(str: &str, suffix: &str) -> bool {
    str.as_bytes().ends_with(suffix.as_bytes())
}

/// Duplicate a string (`g_strdup`).
pub fn strdup(str: Option<&str>) -> Option<String> {
    str.map(str::to_owned)
}

/// Duplicate up to `n` bytes (`g_strndup`).
///
/// Returns a buffer of `n + 1` bytes, always nul-terminated at index `n`.
/// The first `n` bytes follow `strncpy` semantics (padded with nuls when `str`
/// is shorter than `n`).
pub fn strndup(str: Option<&str>, n: Size) -> Option<Vec<u8>> {
    let s = str?;
    if n == Size::MAX {
        return None;
    }
    let mut buf = vec![0u8; n + 1];
    let copy_len = n.min(s.len());
    buf[..copy_len].copy_from_slice(&s.as_bytes()[..copy_len]);
    Some(buf)
}

/// Concatenate strings (`g_strconcat`). Returns `None` when `parts` is empty.
pub fn strconcat(parts: &[&str]) -> Option<String> {
    if parts.is_empty() {
        return None;
    }
    let mut total = 0usize;
    for part in parts {
        total = checked_add_size(total, part.len()).expect("strconcat overflow");
    }
    let mut out = String::with_capacity(total);
    for part in parts {
        out.push_str(part);
    }
    Some(out)
}

/// Join strings with an optional separator (`g_strjoin` / `g_strjoinv`).
pub fn strjoin(separator: Option<&str>, parts: &[&str]) -> String {
    strjoinv(separator, parts)
}

/// Join a slice of strings (`g_strjoinv`).
pub fn strjoinv(separator: Option<&str>, parts: &[&str]) -> String {
    let separator = separator.unwrap_or("");
    match parts.len() {
        0 => String::new(),
        1 => parts[0].to_owned(),
        _ => {
            let sep_len = separator.len();
            let separators =
                checked_mul_size(sep_len, parts.len() - 1).expect("join separator overflow");
            let mut total = separators;
            for part in parts {
                total = checked_add_size(total, part.len()).expect("join overflow");
            }
            let mut out = String::with_capacity(total);
            out.push_str(parts[0]);
            for part in &parts[1..] {
                out.push_str(separator);
                out.push_str(part);
            }
            out
        }
    }
}

/// Whether `c` is ASCII whitespace per GLib's `g_ascii_isspace`.
fn ascii_isspace(c: u8) -> bool {
    matches!(c, b' ' | b'\t' | b'\n' | b'\r' | b'\x0c')
}

/// Remove leading whitespace in place (`g_strchug`).
pub fn strchug(s: &mut String) {
    let start = s.bytes().position(|c| !ascii_isspace(c)).unwrap_or(s.len());
    if start > 0 {
        s.drain(..start);
    }
}

/// Remove trailing whitespace in place (`g_strchomp`).
pub fn strchomp(s: &mut String) {
    let len = s.len();
    let mut end = len;
    while end > 0 && ascii_isspace(s.as_bytes()[end - 1]) {
        end -= 1;
    }
    if end < len {
        s.truncate(end);
    }
}

/// Remove leading and trailing whitespace (`g_strstrip`).
pub fn strstrip(s: &mut String) {
    strchug(s);
    strchomp(s);
}

#[cfg(test)]
mod tests {
    use super::*;

    const GLIB_TEST_STRING: &str = "el dorado ";

    #[test]
    fn strdup_null_and_empty() {
        assert_eq!(strdup(None), None);

        let s = strdup(Some(GLIB_TEST_STRING)).unwrap();
        assert_eq!(s, GLIB_TEST_STRING);

        let s = strdup(Some("")).unwrap();
        assert_eq!(s, "");
    }

    #[test]
    fn strndup_cases() {
        assert_eq!(strndup(None, 3), None);
        let padded = strndup(Some("aaaa"), 5).unwrap();
        assert_eq!(padded.len(), 6);
        assert_eq!(&padded[0..5], b"aaaa\0");
        assert_eq!(padded[5], 0);
        let short = strndup(Some("aaaa"), 2).unwrap();
        assert_eq!(&short[0..2], b"aa");
        assert_eq!(short[2], 0);
        assert_eq!(strndup(Some("aaaa"), Size::MAX), None);
    }

    #[test]
    fn strconcat_cases() {
        assert_eq!(
            strconcat(&[GLIB_TEST_STRING]).as_deref(),
            Some(GLIB_TEST_STRING)
        );
        assert_eq!(
            strconcat(&[GLIB_TEST_STRING, GLIB_TEST_STRING, GLIB_TEST_STRING]).as_deref(),
            Some("el dorado el dorado el dorado ")
        );
        assert_eq!(strconcat(&[]), None);
    }

    #[test]
    fn strjoinv_cases() {
        let strings = ["string1", "string2"];
        assert_eq!(strjoinv(Some(":"), &strings), "string1:string2");
        assert_eq!(strjoinv(None, &strings), "string1string2");
        assert_eq!(strjoinv(None, &[] as &[&str]), "");
    }

    #[test]
    fn strjoin_cases() {
        assert_eq!(strjoin(None, &[] as &[&str]), "");
        assert_eq!(strjoin(Some(":"), &[] as &[&str]), "");
        assert_eq!(strjoin(None, &[GLIB_TEST_STRING]), GLIB_TEST_STRING);
        assert_eq!(
            strjoin(
                None,
                &[GLIB_TEST_STRING, GLIB_TEST_STRING, GLIB_TEST_STRING]
            ),
            "el dorado el dorado el dorado "
        );
        assert_eq!(
            strjoin(
                Some(":"),
                &[GLIB_TEST_STRING, GLIB_TEST_STRING, GLIB_TEST_STRING]
            ),
            "el dorado :el dorado :el dorado "
        );
    }

    #[test]
    fn ascii_strcasecmp_cases() {
        assert_eq!(ascii_strcasecmp("FroboZZ", "frobozz"), 0);
        assert_eq!(ascii_strcasecmp("frobozz", "frobozz"), 0);
        assert_ne!(ascii_strcasecmp("FROBOZZ", "froboz"), 0);
        assert_ne!(ascii_strcasecmp("FROBOZZ", "froboz"), 0);
        assert_eq!(ascii_strcasecmp("", ""), 0);
        assert_eq!(ascii_strcasecmp("!#%&/()", "!#%&/()"), 0);
        assert!(ascii_strcasecmp("a", "b") < 0);
        assert!(ascii_strcasecmp("b", "a") > 0);
    }

    #[test]
    fn strcmp_strlen() {
        assert_eq!(strcmp("abc", "abc"), 0);
        assert!(strcmp("abc", "abd") < 0);
        assert_eq!(strlen("hello"), 5);
    }

    #[test]
    fn str_has_prefix_suffix() {
        assert!(!str_has_prefix("aa", "aaa"));
        assert!(!str_has_prefix("foo", "bar"));
        assert!(!str_has_prefix("foo", "foobar"));
        assert!(str_has_prefix("foobar", "foo"));
        assert!(str_has_prefix("foo", ""));
        assert!(str_has_prefix("", ""));

        assert!(!str_has_suffix("aa", "aaa"));
        assert!(!str_has_suffix("foo", "bar"));
        assert!(!str_has_suffix("bar", "foobar"));
        assert!(str_has_suffix("foobar", "bar"));
        assert!(str_has_suffix("foo", ""));
        assert!(str_has_suffix("", ""));
    }

    fn check_strchug(input: &str, expected: &str) {
        let mut tmp = input.to_owned();
        strchug(&mut tmp);
        assert_eq!(tmp, expected);
    }

    #[test]
    fn strchug_cases() {
        check_strchug("", "");
        check_strchug(" ", "");
        check_strchug("\t\r\n ", "");
        check_strchug(" a", "a");
        check_strchug("  a", "a");
        check_strchug("a a", "a a");
        check_strchug(" a a", "a a");
    }

    fn check_strchomp(input: &str, expected: &str) {
        let mut tmp = input.to_owned();
        strchomp(&mut tmp);
        assert_eq!(tmp, expected);
    }

    #[test]
    fn strchomp_cases() {
        check_strchomp("", "");
        check_strchomp(" ", "");
        check_strchomp(" \t\r\n", "");
        check_strchomp("a ", "a");
        check_strchomp("a  ", "a");
        check_strchomp("a a", "a a");
        check_strchomp("a a ", "a a");
    }

    #[test]
    fn strstrip_cases() {
        let mut s = "  hello  ".to_owned();
        strstrip(&mut s);
        assert_eq!(s, "hello");
    }
}

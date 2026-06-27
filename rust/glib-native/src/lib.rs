//! Native Rust reimplementation of GLib.
//!
//! See [`docs/rust-migration.md`](../../docs/rust-migration.md) for the phased
//! migration plan. Phases 0–1 cover endian helpers, checked arithmetic,
//! reference counting, and [`Bytes`]. Phase 2 adds atomics, memory, and strings.

#![deny(unsafe_op_in_unsafe_fn)]
#![warn(missing_docs)]

pub mod atomic;
pub mod bytes;
pub mod checked;
pub mod endian;
pub mod gstring;
pub mod mem;
pub mod refcount;
pub mod strfuncs;

pub use atomic::{AtomicInt, AtomicPointer, AtomicUInt};
pub use bytes::Bytes;
pub use checked::{checked_add_size, checked_add_u32, checked_mul_size, checked_mul_u32};
pub use endian::{
    g_htonl, g_htons, g_ntohl, g_ntohs, swap_u16_le_be, swap_u32_le_be, swap_u64_le_be,
};
pub use gstring::GString;
pub use mem::{
    aligned_alloc, aligned_alloc0, clear, clear_with, free, malloc, malloc0, malloc0_n, malloc_n,
    memdup, memdup2, realloc, realloc_n, steal, try_aligned_alloc, try_malloc, try_malloc0,
    try_malloc0_n, try_malloc_n, try_realloc, try_realloc_n, AlignedBuffer, MEM_ALIGN,
};
pub use refcount::{AtomicRefCount, RefCount};
pub use strfuncs::{
    ascii_strcasecmp, str_has_prefix, str_has_suffix, strcasecmp, strchomp, strchug, strcmp,
    strconcat, strdup, strjoin, strjoinv, strlen, strndup, strstrip,
};

/// Alias matching GLib's `gboolean`: `true` or `false`.
pub type Bool = bool;

/// Alias matching GLib's `gsize`.
pub type Size = usize;

/// Alias matching GLib's `guint`.
pub type UInt = u32;

/// Mathematical constants from `gtypes.h`.
pub mod constants {
    #![allow(clippy::excessive_precision)]
    /// Euler's number.
    pub const E: f64 = 2.7182818284590452353602874713526624977572470937000;
    /// Natural log of 2.
    pub const LN_2: f64 = 0.69314718055994530941723212145817656807550013436026;
    /// Natural log of 10.
    pub const LN_10: f64 = 2.3025850929940456840179914546843642076011014886288;
    /// Pi.
    pub const PI: f64 = 3.1415926535897932384626433832795028841971693993751;
    /// Pi / 2.
    pub const PI_2: f64 = 1.5707963267948966192313216916397514420985846996876;
    /// Pi / 4.
    pub const PI_4: f64 = 0.78539816339744830961566084581987572104929234984378;
    /// Square root of 2.
    pub const SQRT_2: f64 = 1.4142135623730950488016887242096980785696718753769;
}

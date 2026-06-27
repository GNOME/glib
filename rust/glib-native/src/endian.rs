//! Byte-order helpers from `gtypes.h`.

/// Constant-time 16-bit endian swap (`GUINT16_SWAP_LE_BE_CONSTANT`).
#[inline]
pub const fn swap_u16_le_be_constant(val: u16) -> u16 {
    val.rotate_right(8)
}

/// Constant-time 32-bit endian swap (`GUINT32_SWAP_LE_BE_CONSTANT`).
#[inline]
pub const fn swap_u32_le_be_constant(val: u32) -> u32 {
    val.rotate_right(8) & 0x00ff_00ff | val.rotate_left(8) & 0xff00_ff00
}

/// Constant-time 64-bit endian swap (`GUINT64_SWAP_LE_BE_CONSTANT`).
#[inline]
pub const fn swap_u64_le_be_constant(val: u64) -> u64 {
    swap_u32_le_be_constant((val & 0xffff_ffff) as u32) as u64
        | (swap_u32_le_be_constant((val >> 32) as u32) as u64) << 32
}

/// 16-bit endian swap (`GUINT16_SWAP_LE_BE`).
#[inline]
pub fn swap_u16_le_be(val: u16) -> u16 {
    swap_u16_le_be_constant(val)
}

/// 32-bit endian swap (`GUINT32_SWAP_LE_BE`).
#[inline]
pub fn swap_u32_le_be(val: u32) -> u32 {
    val.swap_bytes()
}

/// 64-bit endian swap (`GUINT64_SWAP_LE_BE`).
#[inline]
pub fn swap_u64_le_be(val: u64) -> u64 {
    val.swap_bytes()
}

/// Convert 32-bit value from network to host byte order (`g_ntohl`).
#[inline]
pub fn g_ntohl(val: u32) -> u32 {
    u32::from_be(val)
}

/// Convert 16-bit value from network to host byte order (`g_ntohs`).
#[inline]
pub fn g_ntohs(val: u16) -> u16 {
    u16::from_be(val)
}

/// Convert 32-bit value from host to network byte order (`g_htonl`).
#[inline]
pub fn g_htonl(val: u32) -> u32 {
    val.to_be()
}

/// Convert 16-bit value from host to network byte order (`g_htons`).
#[inline]
pub fn g_htons(val: u16) -> u16 {
    val.to_be()
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn roundtrip_swap() {
        let v16: u16 = 0x1234;
        assert_eq!(swap_u16_le_be(swap_u16_le_be(v16)), v16);

        let v32: u32 = 0x1234_5678;
        assert_eq!(swap_u32_le_be(swap_u32_le_be(v32)), v32);

        let v64: u64 = 0x1234_5678_9abc_def0;
        assert_eq!(swap_u64_le_be(swap_u64_le_be(v64)), v64);
    }

    #[test]
    fn network_order() {
        assert_eq!(g_htonl(0x0102_0304), 0x0102_0304u32.to_be());
        assert_eq!(g_ntohl(g_htonl(42)), 42);
        assert_eq!(g_ntohs(g_htons(0xabcd)), 0xabcd);
    }
}

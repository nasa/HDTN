#ifdef BPV6_SDNV_CGOTO

#include <assert.h>
#include <printf.h>

#include "codec/bpv6.h"

// get the highest bit set for a 64-bit value
static inline uint8_t get_msb64 (register uint64_t val)
{

    // return (val == 0) ? 1 : 64 - _lzcnt_u64(val);

    return (val == 0) ? 1 : 64 - __builtin_clzll(val);
}

/*
 * This works by first computing the highest active bit in a value - this is going to tell us how long the SDNV is
 * going to be.  To avoid iteration and branches, a lookup table is used to call various functions - each individual
 * function is tuned to encode an SDNV of the appropriate length.
 */

uint8_t bpv6_sdnv_encode(const uint64_t target, char* buffer, const size_t offset, const size_t bufsz) {
    static void* _clz_dispatch[] = {
        &&bpv6_encode_1,
        &&bpv6_encode_1,
        &&bpv6_encode_1,
        &&bpv6_encode_1,
        &&bpv6_encode_1,
        &&bpv6_encode_1,
        &&bpv6_encode_1,
        // 7 bits into 1 byte
        &&bpv6_encode_2,
        &&bpv6_encode_2,
        &&bpv6_encode_2,
        &&bpv6_encode_2,
        &&bpv6_encode_2,
        &&bpv6_encode_2,
        &&bpv6_encode_2,
        // 14 bits into 2 bytes
        &&bpv6_encode_3,
        &&bpv6_encode_3,
        &&bpv6_encode_3,
        &&bpv6_encode_3,
        &&bpv6_encode_3,
        &&bpv6_encode_3,
        &&bpv6_encode_3,
        // 21 bits into 3 bytes
        &&bpv6_encode_4,
        &&bpv6_encode_4,
        &&bpv6_encode_4,
        &&bpv6_encode_4,
        &&bpv6_encode_4,
        &&bpv6_encode_4,
        &&bpv6_encode_4,
        // 28 bits into 4 bytes
        &&bpv6_encode_5,
        &&bpv6_encode_5,
        &&bpv6_encode_5,
        &&bpv6_encode_5,
        &&bpv6_encode_5,
        &&bpv6_encode_5,
        &&bpv6_encode_5,
        // 35 bits into 5 bytes
        &&bpv6_encode_6,
        &&bpv6_encode_6,
        &&bpv6_encode_6,
        &&bpv6_encode_6,
        &&bpv6_encode_6,
        &&bpv6_encode_6,
        &&bpv6_encode_6,
        // 42 bits into 6 bytes
        &&bpv6_encode_7,
        &&bpv6_encode_7,
        &&bpv6_encode_7,
        &&bpv6_encode_7,
        &&bpv6_encode_7,
        &&bpv6_encode_7,
        &&bpv6_encode_7,
        // 49 bits into 7 bytes
        &&bpv6_encode_8,
        &&bpv6_encode_8,
        &&bpv6_encode_8,
        &&bpv6_encode_8,
        &&bpv6_encode_8,
        &&bpv6_encode_8,
        &&bpv6_encode_8,
        // 56 bits into 8 bytes
        &&bpv6_encode_9,
        &&bpv6_encode_9,
        &&bpv6_encode_9,
        &&bpv6_encode_9,
        &&bpv6_encode_9,
        &&bpv6_encode_9,
        &&bpv6_encode_9,
        // 64 bits into 10 bytes
        &&bpv6_encode_10
    };

    goto *_clz_dispatch[get_msb64(target)];

    bpv6_encode_1:
    buffer[offset] = target & 0x7fLL;
    return 1;

    bpv6_encode_2:
    buffer[offset + 1] = target & 0x7FLL;
    buffer[offset] = ((target & (0x7fLL << 7)) >> 7) | 0x80;
    return 2;

    bpv6_encode_3:
    buffer[offset + 2] = target & 0x7FLL;
    buffer[offset + 1] = ((target & (0x7fLL << 7)) >> 7) | 0x80;
    buffer[offset] = ((target & (0x7fLL << 14)) >> 14) | 0x80;
    return 3;

    bpv6_encode_4:
    buffer[offset + 3] = target & 0x7FLL;
    buffer[offset + 2] = ((target & (0x7FLL << 7)) >> 7) | 0x80;
    buffer[offset + 1] = ((target & (0x7FLL << 14)) >> 14) | 0x80;
    buffer[offset] = ((target & (0x7FLL << 21)) >> 21) | 0x80;
    return 4;

    bpv6_encode_5:
    buffer[offset + 4] = target & 0x7FLL;
    buffer[offset + 3] = ((target & (0x7FLL << 7)) >> 7) | 0x80;
    buffer[offset + 2] = ((target & (0x7FLL << 14)) >> 14) | 0x80;
    buffer[offset + 1] = ((target & (0x7FLL << 21)) >> 21) | 0x80;
    buffer[offset] = ((target & (0x7FLL << 28)) >> 28) | 0x80;
    return 5;

    bpv6_encode_6:
    buffer[offset + 5] = target & 0x7FLL;
    buffer[offset + 4] = ((target & (0x7FLL << 7)) >> 7) | 0x80;
    buffer[offset + 3] = ((target & (0x7FLL << 14)) >> 14) | 0x80;
    buffer[offset + 2] = ((target & (0x7FLL << 21)) >> 21) | 0x80;
    buffer[offset + 1] = ((target & (0x7FLL << 28)) >> 28) | 0x80;
    buffer[offset] = ((target & (0x7FLL << 35)) >> 35) | 0x80;
    return 6;

    bpv6_encode_7:
    buffer[offset + 6] = target & 0x7FLL;
    buffer[offset + 5] = ((target & (0x7FLL << 7)) >> 7) | 0x80;
    buffer[offset + 4] = ((target & (0x7FLL << 14)) >> 14) | 0x80;
    buffer[offset + 3] = ((target & (0x7FLL << 21)) >> 21) | 0x80;
    buffer[offset + 2] = ((target & (0x7FLL << 28)) >> 28) | 0x80;
    buffer[offset + 1] = ((target & (0x7FLL << 35)) >> 35) | 0x80;
    buffer[offset] = ((target & (0x7FLL << 42)) >> 42) | 0x80;
    return 7;

    bpv6_encode_8:
    buffer[offset + 7] = target & 0x7FLL;
    buffer[offset + 6] = ((target & (0x7FLL << 7)) >> 7) | 0x80;
    buffer[offset + 5] = ((target & (0x7FLL << 14)) >> 14) | 0x80;
    buffer[offset + 4] = ((target & (0x7FLL << 21)) >> 21) | 0x80;
    buffer[offset + 3] = ((target & (0x7FLL << 28)) >> 28) | 0x80;
    buffer[offset + 2] = ((target & (0x7FLL << 35)) >> 35) | 0x80;
    buffer[offset + 1] = ((target & (0x7FLL << 42)) >> 42) | 0x80;
    buffer[offset] = ((target & (0x7FLL << 49)) >> 49) | 0x80;
    return 8;

    bpv6_encode_9:
    buffer[offset + 8] = target & 0x7FLL;
    buffer[offset + 7] = ((target & (0x7FLL << 7)) >> 7) | 0x80;
    buffer[offset + 6] = ((target & (0x7FLL << 14)) >> 14) | 0x80;
    buffer[offset + 5] = ((target & (0x7FLL << 21)) >> 21) | 0x80;
    buffer[offset + 4] = ((target & (0x7FLL << 28)) >> 28) | 0x80;
    buffer[offset + 3] = ((target & (0x7FLL << 35)) >> 35) | 0x80;
    buffer[offset + 2] = ((target & (0x7FLL << 42)) >> 42) | 0x80;
    buffer[offset + 1] = ((target & (0x7FLL << 49)) >> 49) | 0x80;
    buffer[offset] = ((target & (0x7FLL << 56)) >> 56) | 0x80;
    return 9;

    bpv6_encode_10:
    buffer[offset + 9] = target & 0x7FLL;
    buffer[offset + 8] = ((target & (0x7FLL << 7)) >> 7) | 0x80;
    buffer[offset + 7] = ((target & (0x7FLL << 14)) >> 14) | 0x80;
    buffer[offset + 6] = ((target & (0x7FLL << 21)) >> 21) | 0x80;
    buffer[offset + 5] = ((target & (0x7FLL << 28)) >> 28) | 0x80;
    buffer[offset + 4] = ((target & (0x7FLL << 35)) >> 35) | 0x80;
    buffer[offset + 3] = ((target & (0x7FLL << 42)) >> 42) | 0x80;
    buffer[offset + 2] = ((target & (0x7FLL << 49)) >> 49) | 0x80;
    buffer[offset + 1] = ((target & (0x7FLL << 56)) >> 56) | 0x80;
    buffer[offset] = ((target & (0x7FLL << 63)) >> 63) | 0x80;
    return 10;
    
}

#endif
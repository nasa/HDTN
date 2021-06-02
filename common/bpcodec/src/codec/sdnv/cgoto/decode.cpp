/***************************************************************************
 * NASA Glenn Research Center, Cleveland, OH
 * Released under the NASA Open Source Agreement (NOSA)
 * May  2021
 *
 ****************************************************************************
 */

#ifdef BPV6_SDNV_CGOTO

#include <stdio.h>
#include <string.h>
#include <assert.h>

#include "codec/bpv6.h"

// get the highest bit set for a 64-bit value
static inline uint8_t get_msb64 (register uint64_t val)
{
    // return (val == 0) ? 1 : 64 - _lzcnt_u64(val);

    return (val == 0) ? 1 : 64 - __builtin_clzll(val);
}

// get the highest bit set for a 64-bit value
//static inline uint8_t get_msb64 (register uint64_t val)
//{
//    return 64 - __builtin_clzll(val);
//}

static inline uint8_t get_zero(uint64_t val)
{
    uint64_t inv = ~val;
    uint64_t complement = val + (uint8_t)1;
    return get_msb64(inv & complement);
}

uint8_t  bpv6_sdnv_decode(uint64_t* target, const char* buffer, const size_t offset, const size_t bufsz) {
    static void* _dispatch[] = {
        &&bpv6_decode_1,
        &&bpv6_decode_2,
        &&bpv6_decode_3,
        &&bpv6_decode_4,
        &&bpv6_decode_5,
        &&bpv6_decode_6,
        &&bpv6_decode_7,
        &&bpv6_decode_8
    };

    char tbuf[16];
    uint64_t raw_value;
    uint8_t  continue_bits;

    // first: read 8-byte data chunk
    if(offset >= bufsz - 8) {
        // we can't do this the fast way 'cause we'll read off the end of the buffer
        memset(tbuf, 0, 16);
        memcpy(tbuf, (buffer + offset), bufsz - offset);
        memcpy(&raw_value, tbuf, 8);
    }
    else {
        memcpy(&raw_value, (buffer + offset), 8);
    }

    // next: extract continue bits to wind up with a happy, 8-bit value that describes our SDNV
    continue_bits = (uint8_t)(
            ((raw_value & (0x80LL << 56)) >> 56) |
            ((raw_value & (0x80LL << 48)) >> 49) |
            ((raw_value & (0x80LL << 40)) >> 42) |
            ((raw_value & (0x80LL << 32)) >> 35) |
            ((raw_value & (0x80LL << 24)) >> 28) |
            ((raw_value & (0x80LL << 16)) >> 21) |
            ((raw_value & (0x80LL << 8)) >> 14) |
            ((raw_value & (0x80LL)) >> 7));

    uint8_t zero_index = get_zero(continue_bits);

    goto *_dispatch[zero_index - 1];

    bpv6_decode_1:
    *target = (uint64_t)(
            (uint64_t)buffer[offset] & 0x7F
    );
    return 1;

    bpv6_decode_2:
    *target = (uint64_t)(
            (((uint64_t)buffer[offset] << 7) & (0x7FLL << 7)) |
            (((uint64_t)buffer[offset + 1]) & 0x7F)
    );
    return 2;

    bpv6_decode_3:
    *target = (uint64_t)(
            (((uint64_t)buffer[offset] << 14) & (0x7FLL << 14)) |
            (((uint64_t)buffer[offset + 1] << 7) & (0x7FLL << 7)) |
            (((uint64_t)buffer[offset + 2]) & 0x7F)
    );
    return 3;

    bpv6_decode_4:
    *target = (uint64_t)(
            (((uint64_t)buffer[offset] << 21) & (0x7FLL << 21)) |
            (((uint64_t)buffer[offset + 1] << 14) & (0x7FLL << 14)) |
            (((uint64_t)buffer[offset + 2] << 7) & (0x7FLL << 7)) |
            (((uint64_t)buffer[offset + 3]) & 0x7F)
    );
    return 4;

    bpv6_decode_5:
    *target = (uint64_t)(
            (((uint64_t)buffer[offset] << 28) & (0x7FLL << 28)) |
            (((uint64_t)buffer[offset + 1] << 21) & (0x7FLL << 21)) |
            (((uint64_t)buffer[offset + 2] << 14) & (0x7FLL << 14)) |
            (((uint64_t)buffer[offset + 3] << 7) & (0x7FLL << 7)) |
            (((uint64_t)buffer[offset + 4]) & 0x7F)
    );
    return 5;

    bpv6_decode_6:
    *target = (uint64_t)(
            (((uint64_t)buffer[offset] << 35) & (0x7FLL << 35)) |
            (((uint64_t)buffer[offset + 1] << 28) & (0x7FLL << 28)) |
            (((uint64_t)buffer[offset + 2] << 21) & (0x7FLL << 21)) |
            (((uint64_t)buffer[offset + 3] << 14) & (0x7FLL << 14)) |
            (((uint64_t)buffer[offset + 4] << 7) & (0x7FLL << 7)) |
            (((uint64_t)buffer[offset + 5]) & 0x7F)
    );
    return 6;

    bpv6_decode_7:
    *target = (uint64_t)(
            (((uint64_t)buffer[offset] << 42) & (0x7FLL << 42)) |
            (((uint64_t)buffer[offset + 1] << 35) & (0x7FLL << 35)) |
            (((uint64_t)buffer[offset + 2] << 28) & (0x7FLL << 28)) |
            (((uint64_t)buffer[offset + 3] << 21) & (0x7FLL << 21)) |
            (((uint64_t)buffer[offset + 4] << 14) & (0x7FLL << 14)) |
            (((uint64_t)buffer[offset + 5] << 7) & (0x7FLL << 7)) |
            (((uint64_t)buffer[offset + 6]) & 0x7F)
    );
    return 7;

    bpv6_decode_8:
    *target = (uint64_t)(
            (((uint64_t)buffer[offset] << 49) & (0x7FLL << 49)) |
            (((uint64_t)buffer[offset + 1] << 42) & (0x7FLL << 42)) |
            (((uint64_t)buffer[offset + 2] << 35) & (0x7FLL << 35)) |
            (((uint64_t)buffer[offset + 3] << 28) & (0x7FLL << 28)) |
            (((uint64_t)buffer[offset + 4] << 21) & (0x7FLL << 21)) |
            (((uint64_t)buffer[offset + 5] << 14) & (0x7FLL << 14)) |
            (((uint64_t)buffer[offset + 6] << 7) & (0x7FLL << 7)) |
            (((uint64_t)buffer[offset + 7]) & 0x7F)
    );
    return 8;

}

#endif

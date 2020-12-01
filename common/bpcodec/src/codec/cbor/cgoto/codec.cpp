#ifdef BPV7_CBOR_CGOTO

#include <stdint.h>

#include "codec/bpv7.h"

#define CBOR_UINT8_TYPE   (24)
#define CBOR_UINT16_TYPE  (25)
#define CBOR_UINT32_TYPE  (26)
#define CBOR_UINT64_TYPE  (27)

namespace hdtn {
    void cbor_init() { }

    uint8_t cbor_encode_uint(uint8_t* dst, const uint64_t src, const uint64_t offset, const uint64_t bufsz) {
        dst += offset;

        // count the number of leading zeroes, which is going to tell us how large our encoded value will be
        if(src < 24) {
            if(bufsz - offset < 1) {
                return 0;
            }
            dst[0] = (uint8_t)src;
            return 1;
        }
        else if(src < UINT8_MAX) {
            if(bufsz - offset < 2) {
                return 0;
            }
            dst[0] = CBOR_UINT8_TYPE;
            dst[1] = (uint8_t)src;
            return 2;
        }
        else if(src < UINT16_MAX) {
            if(bufsz - offset < 3) {
                return 0;
            }
            dst[0] = CBOR_UINT16_TYPE;
            dst[1] = (uint8_t)((src & 0xff00) >> 8);
            dst[2] = (uint8_t) (src & 0x00ff);
            return 3;
        }
        if(src < UINT32_MAX) {
            if(bufsz - offset < 5) {
                return 0;
            }
            dst[0] = CBOR_UINT32_TYPE;
            dst[1] = (uint8_t)((src & 0xff000000) >> 24);
            dst[2] = (uint8_t)((src & 0x00ff0000) >> 16);
            dst[3] = (uint8_t)((src & 0x0000ff00) >> 8);
            dst[4] = (uint8_t) (src & 0x000000ff);
            return 5;
        }
        if(src < UINT64_MAX) {
            if(bufsz - offset < 9) {
                return 0;
            }
            dst[0] = CBOR_UINT64_TYPE;
            dst[1] = (uint8_t)((src & 0xff00000000000000) >> 56);
            dst[2] = (uint8_t)((src & 0x00ff000000000000) >> 48);
            dst[3] = (uint8_t)((src & 0x0000ff0000000000) >> 40);
            dst[4] = (uint8_t)((src & 0x000000ff00000000) >> 32);
            dst[5] = (uint8_t)((src & 0x00000000ff000000) >> 24);
            dst[6] = (uint8_t)((src & 0x0000000000ff0000) >> 16);
            dst[7] = (uint8_t)((src & 0x000000000000ff00) >> 8);
            dst[8] = (uint8_t) (src & 0x00000000000000ff);
            return 9;
        }
        return 0;
    }
    
    uint8_t cbor_decode_uint(uint64_t *dst, uint8_t *src, const uint64_t offset, const uint64_t bufsz) {
        src += offset;

        static void *_dispatch[] = {
                &&dec_uint_single,
                &&dec_uint_single,
                &&dec_uint_single,
                &&dec_uint_single,
                &&dec_uint_single,
                &&dec_uint_single,
                &&dec_uint_single,
                &&dec_uint_single,
                &&dec_uint_single,
                &&dec_uint_single,
                &&dec_uint_single,
                &&dec_uint_single,
                &&dec_uint_single,
                &&dec_uint_single,
                &&dec_uint_single,
                &&dec_uint_single,
                &&dec_uint_single,
                &&dec_uint_single,
                &&dec_uint_single,
                &&dec_uint_single,
                &&dec_uint_single,
                &&dec_uint_single,
                &&dec_uint_single,
                &&dec_uint_single,
                &&dec_uint_24,
                &&dec_uint_25,
                &&dec_uint_26,
                &&dec_uint_27,
                &&dec_uint_invalid,  // 28
                &&dec_uint_invalid,  // 29
                &&dec_uint_invalid,  // 30
                &&dec_uint_invalid   // 31
        };

        goto *_dispatch[(src[0] & 0x1f)];

        dec_uint_single:
        if(bufsz - offset < 1) {
            return 0;
        }
        *dst = src[0] & 0x1f;
        return 1;
        dec_uint_24:
        if(bufsz - offset < 2) {
            return 0;
        }
        *dst = src[1];
        return 2;
        dec_uint_25:
        if(bufsz - offset < 3) {
            return 0;
        }
        *dst = (src[1] << 8) | src[2];
        return 3;
        dec_uint_26:
        if(bufsz - offset < 5) {
            return 0;
        }
        *dst = ((uint64_t) src[1] << 24) | ((uint64_t) src[2] << 16)
                | ((uint64_t) src[3] << 8) | ((uint64_t) src[4]);
        return 5;
        dec_uint_27:
        if(bufsz - offset < 9) {
            return 0;
        }
        *dst = ((uint64_t) src[1] << 56) | ((uint64_t) src[2] << 48) | ((uint64_t) src[3] << 40)
                | ((uint64_t) src[4] << 32) | ((uint64_t) src[5] << 24) | ((uint64_t) src[6] << 16)
                | ((uint64_t) src[7] << 8) | ((uint64_t) src[8]);
        return 9;
        dec_uint_invalid:
        return 0;
    }
}    

#endif

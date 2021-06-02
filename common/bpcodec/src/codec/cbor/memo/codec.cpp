/***************************************************************************
 * NASA Glenn Research Center, Cleveland, OH
 * Released under the NASA Open Source Agreement (NOSA)
 * May  2021
 *
 ****************************************************************************
 */

#ifdef  BPV7_CBOR_MEMOIZE

#include <stdio.h>
#include <stdint.h>

#include "codec/bpv7.h"
#include <string.h>
#include <endian.h>

#define CBOR_UINT8_TYPE   (24)
#define CBOR_UINT16_TYPE  (25)
#define CBOR_UINT32_TYPE  (26)
#define CBOR_UINT64_TYPE  (27)

#define CBOR_MEMOIZE_THRESHOLD  (65536)
#define CBOR_MEMOIZE_TABLESZ    (CBOR_MEMOIZE_THRESHOLD)  // need to address first three floating bits for "type" specification

namespace hdtn {
    static uint8_t _cbor_decode_uint_full(uint64_t *dst, uint8_t *src, const uint64_t offset, const uint64_t bufsz);
    static uint8_t _cbor_encode_uint_full(uint8_t* dst, const uint64_t src, const uint64_t offset, const uint64_t bufsz);

    static uint8_t* _encode_mval = nullptr;

    void cbor_init() {
        _encode_mval = new uint8_t[CBOR_MEMOIZE_TABLESZ * sizeof(uint32_t)];
        uint8_t* encode_raw = (uint8_t*) _encode_mval;

        for(int i = 0; i < CBOR_MEMOIZE_THRESHOLD; ++i) {
            // encode memoization is pretty straightforward ...
            _cbor_encode_uint_full(encode_raw + i * sizeof(uint32_t), i, 0, sizeof(uint64_t));
        }
    }

    uint8_t cbor_encode_uint(uint8_t* dst, const uint64_t src, const uint64_t offset, const uint64_t bufsz) {
        dst += offset;

        if(src <= UINT16_MAX) {
            uint8_t mlen = (src < CBOR_UINT8_TYPE) ? 1 : (
                (src <= UINT8_MAX) ? 2 : 3  // max memoization tops out at 16 bits
            );
            if(bufsz - offset < mlen) {
                return 0;
            }
            memcpy(dst, &_encode_mval[src * sizeof(uint32_t)], mlen);
            return mlen;
        }
        else if(src <= UINT32_MAX) {
            if(bufsz - offset < 5) {
                return 0;
            }
            dst[0] = CBOR_UINT32_TYPE;
            const uint32_t nbsrc32 = be32toh((uint32_t)(src));
            memcpy(&dst[1], &nbsrc32, 4);            
            return 5;
        }
        else if(src <= UINT64_MAX) {
            if(bufsz - offset < 9) {
                return 0;
            }
            dst[0] = CBOR_UINT64_TYPE;
            const uint64_t nbsrc64 = be64toh((uint64_t)(src));
            memcpy(&dst[1], &nbsrc64, 8);
            return 9;
        }
        return 0;
    }

    uint8_t cbor_decode_uint(uint64_t *dst, uint8_t *src, const uint64_t offset, const uint64_t bufsz) {
        src += offset;
        uint8_t type = src[0] & 0x1f;
        if(type < CBOR_UINT8_TYPE) {
            *dst = type;
            return 1;
        }
        else if(type == CBOR_UINT8_TYPE) {
            *dst = src[1];
            return 2;
        }
        else if(type == CBOR_UINT16_TYPE) {
            uint16_t tval;
            memcpy(&tval, &src[1], 2);
            tval = be16toh(tval);
            *dst = tval;
            return 3;
        }
        else if(type == CBOR_UINT32_TYPE) {
            uint32_t tval;
            memcpy(&tval, &src[1], 4);
            tval = be32toh(tval);
            *dst = tval;
            return 5;
        }
        else if(type == CBOR_UINT64_TYPE) {
            uint64_t tval;
            memcpy(&tval, &src[1], 8);
            tval = be64toh(tval);
            *dst = tval;
            return 9;
        }

        return 0;
    }

    static uint8_t _cbor_decode_uint_full(uint64_t *dst, uint8_t *src, const uint64_t offset, const uint64_t bufsz) {
        src += offset;
        uint8_t type = src[0] & 0x1f;
        if(type < CBOR_UINT8_TYPE) {
            *dst = type;
            return 1;
        }
        else if(type == CBOR_UINT8_TYPE) {
            *dst = src[1];
            return 2;
        }
        else if(type == CBOR_UINT16_TYPE) {
            uint16_t tval;
            memcpy(&tval, &src[1], 2);
            tval = be16toh(tval);
            *dst = tval;
            return 3;
        }
        else if(type == CBOR_UINT32_TYPE) {
            uint32_t tval;
            memcpy(&tval, &src[1], 4);
            tval = be32toh(tval);
            *dst = tval;
            return 5;
        }
        else if(type == CBOR_UINT64_TYPE) {
            uint64_t tval;
            memcpy(&tval, &src[1], 8);
            tval = be64toh(tval);
            *dst = tval;
            return 9;
        }

        return 0;
    }

    static uint8_t _cbor_encode_uint_full(uint8_t* dst, const uint64_t src, const uint64_t offset, const uint64_t bufsz) {
        dst += offset;
        
        if(src < CBOR_UINT8_TYPE) {
            if(bufsz - offset < 1) {
                return 0;
            }
            dst[0] = (uint8_t)src;
            return 1;
        }
        else if(src <= UINT8_MAX) {
            if(bufsz - offset < 2) {
                return 0;
            }

            dst[0] = CBOR_UINT8_TYPE;
            dst[1] = (uint8_t)src;
            return 2;
        }
        else if(src <= UINT16_MAX) {
            if(bufsz - offset < 3) {
                return 0;
            }
            dst[0] = CBOR_UINT16_TYPE;
            const uint16_t nbsrc16 = be16toh((uint16_t)(src));
            memcpy(&dst[1], &nbsrc16, 2);
            return 3;
        }
        if(src <= UINT32_MAX) {
            if(bufsz - offset < 5) {
                return 0;
            }
            dst[0] = CBOR_UINT32_TYPE;
            const uint32_t nbsrc32 = be32toh((uint32_t)(src));
            memcpy(&dst[1], &nbsrc32, 4);            
            return 5;
        }
        if(src <= UINT64_MAX) {
            if(bufsz - offset < 9) {
                return 0;
            }
            dst[0] = CBOR_UINT64_TYPE;
            const uint64_t nbsrc64 = be64toh((uint64_t)(src));
            memcpy(&dst[1], &nbsrc64, 8);
            return 9;
        }
        return 0;
    }
}    

#endif

#ifdef BPV7_CBOR_STD

#include <stdio.h>
#include <stdint.h>

#include "codec/bpv7.h"
#include <string.h>
//#include <endian.h>
#include <boost/endian/conversion.hpp>

#define CBOR_UINT8_TYPE   (24)
#define CBOR_UINT16_TYPE  (25)
#define CBOR_UINT32_TYPE  (26)
#define CBOR_UINT64_TYPE  (27)

namespace hdtn {
    void cbor_init() { }

    uint8_t cbor_encode_uint(uint8_t* dst, const uint64_t src, const uint64_t offset, const uint64_t bufsz) {
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
            const uint16_t nbsrc16 = boost::endian::big_to_native((uint16_t)(src));//be16toh((uint16_t)(src));
            memcpy(&dst[1], &nbsrc16, 2);
            return 3;
        }
        else if(src <= UINT32_MAX) {
            if(bufsz - offset < 5) {
                return 0;
            }
            dst[0] = CBOR_UINT32_TYPE;
            const uint32_t nbsrc32 = boost::endian::big_to_native((uint32_t)(src));//be32toh((uint32_t)(src));
            memcpy(&dst[1], &nbsrc32, 4);            
            return 5;
        }
        else if(src <= UINT64_MAX) {
            if(bufsz - offset < 9) {
                return 0;
            }
            dst[0] = CBOR_UINT64_TYPE;
            const uint64_t nbsrc64 = boost::endian::big_to_native((uint64_t)(src));//be64toh((uint64_t)(src));
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
            tval = boost::endian::big_to_native(tval);//be16toh(tval);
            *dst = tval;
            return 3;
        }
        else if(type == CBOR_UINT32_TYPE) {
            uint32_t tval;
            memcpy(&tval, &src[1], 4);
            tval = boost::endian::big_to_native(tval);//be32toh(tval);
            *dst = tval;
            return 5;
        }
        else if(type == CBOR_UINT64_TYPE) {
            uint64_t tval;
            memcpy(&tval, &src[1], 8);
            tval = boost::endian::big_to_native(tval);//be64toh(tval);
            *dst = tval;
            return 9;
        }

        return 0;
    }
}    

#endif

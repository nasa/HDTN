#include <stdio.h>
#include "codec/bpv6.h"

#ifdef BPV6_SDNV_LOOP

uint8_t  bpv6_sdnv_decode(uint64_t* target, const char* buffer, const size_t offset, const size_t bufsz) {
    uint64_t tval = 0;
    int i = 0;
    tval = buffer[offset] & 0x7F;
    if(buffer[offset] & 0x80) {
        do {
            ++i;
            tval <<= 7;
            tval |= buffer[offset + i] & 0x7F;

        } while(buffer[offset + i] & 0x80);
    }
    *target = tval;
    return i + 1;
}

uint8_t  bpv6_sdnv_encode(const uint64_t target, char* buffer, const size_t offset, const size_t bufsz) {
    if(target == 0) {
        buffer[offset] = 0;
        return 1;
    }

    uint64_t tval = target;
    int len = 0;
    int i = 0;
    while(tval > 0) {
        tval >>= 7;
        ++len;
    }
    len -= 1;
    if(offset + len >= bufsz) {
        return 0;
    }

    tval = target;
    while(tval > 0) {
        if(i == 0) {
            buffer[offset + (len - i)] = tval & 0x7F;
        }
        else {
            buffer[offset + (len - i)] = 0x80 | (tval & 0x7F);
        }
        tval >>= 7;
        ++i;
    }
    return len + 1;
}

#endif

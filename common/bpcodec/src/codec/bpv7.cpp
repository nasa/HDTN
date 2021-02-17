#include "codec/bpv7.h"
#include <cstdio>
#include <cstring>
//#include <netinet/in.h> //for htons not declared
#include <boost/endian/conversion.hpp>

// BPBIS_10 enables compatibility for version 10 of the bpbis draft.  This was required to achieve interoperability testing.
#define BPV7_BPBIS_10     (1)

// FIXME: this assumes we need to byte swap, which might not always be true.

namespace hdtn {
    static uint8_t _bpv7_eid_decode(bpv7_ipn_eid* eid, char* buffer, size_t offset, size_t bufsz) {
        uint64_t len = 0;
        buffer += offset;
        bufsz -= offset;
        uint8_t index = 1;  // set to 1 to skip the array indicator byte - it's always an array of length "2"
        // now read our EID scheme type and mask everything but the last two bits (to keep our table size down)
        uint8_t type = buffer[index] & 0x03;
        if(type == BPV7_EID_SCHEME_DTN) {
            eid->type = BPV7_EID_SCHEME_DTN;
            index ++;
            index += cbor_decode_uint(&len, (uint8_t *)(&buffer[index]), 0, bufsz - index);
            if(len > (bufsz - index)) {
                printf("Bad string length: %llu\n", len);
                return 0;  // make sure we're not reading off the end of our assigned buffer here ...
            }
            else if(len > 0) {
                memcpy(eid->path, &buffer[index], len);
                eid->path[len] = 0;
            }
            else {
                eid->path[0] = 0;  // dtn:none is the empty string in this case.
            }
            index += len;
            return index;
        }
        else if(type == BPV7_EID_SCHEME_IPN) {
            eid->type = BPV7_EID_SCHEME_IPN;
            // jump past array header byte
            index += 2;
            index += cbor_decode_uint(&eid->node, (uint8_t*)(buffer + index), 0, bufsz);
            index += cbor_decode_uint(&eid->service, (uint8_t*)(buffer + index), 0, bufsz);
            return index;
        }
        return 0;
    }

    uint32_t bpv7_canonical_block_encode(bpv7_canonical_block* block, char* buffer, size_t offset, size_t bufsz) {
        return 0;
    }

    uint32_t bpv7_canonical_block_decode(bpv7_canonical_block* block, char* buffer, size_t offset, size_t bufsz) {
        uint32_t index = 1;  // skip the array information byte
        uint64_t tmp;
        uint8_t res = cbor_decode_uint(&tmp, (uint8_t *)buffer, offset + index, bufsz);
        index += res;
        if(0 == res) {
            return 0;
        }
        block->block_type = tmp;
        res = cbor_decode_uint(&tmp, (uint8_t *)buffer, offset + index, bufsz);
        index += res;
        if(0 == res) {
            return 0;
        }
        block->block_id = tmp;
        res = cbor_decode_uint(&tmp, (uint8_t *)buffer, offset + index, bufsz);
        index += res;
        if(0 == res) {
            return 0;
        }
        block->flags = tmp;
        res = cbor_decode_uint(&tmp, (uint8_t *)buffer, offset + index, bufsz);
        index += res;
        if(0 == res) {
            return 0;
        }
        block->crc_type = tmp;
        res = cbor_decode_uint(&tmp, (uint8_t *)buffer, offset + index, bufsz);
        index += res;
        if(0 == res) {
            return 0;
        }
        block->len = tmp;
        block->offset = offset + index;
        index += block->len;
        if(block->crc_type) {
            res = cbor_decode_uint(&tmp, (uint8_t *)buffer, offset + index, bufsz);
            index += res;
            if(0 == res) {
                return 0;
            }
            memcpy(block->crc_data, buffer + offset + index, tmp);
            index += tmp;
        }
        return index;
    }

    uint32_t bpv7_primary_block_encode(bpv7_primary_block* primary, char* buffer, size_t offset, size_t bufsz) {
        return 0;
    }

    uint32_t bpv7_primary_block_decode(bpv7_primary_block* primary, char* buffer, size_t offset, size_t bufsz) {
        uint64_t crclen;
        uint32_t index = 0;
        uint8_t  res;

        if(bufsz - offset < sizeof(bpv7_hdr)) {
            return 0;  // not enough available bytes to do our read ...
        }

        buffer += offset;
        bufsz -= offset;

        auto base = (bpv7_hdr *)buffer;
        if(base->hdr.magic != BPV7_MAGIC_FRAGMENT && base->hdr.magic != BPV7_MAGIC_NOFRAGMENT) {
            return 0;  // something is wrong in the beginning of our primary block header.
        }
        primary->version = base->hdr.bytes[BPV7_VERSION_OFFSET];
		primary->flags = boost::endian::big_to_native(base->flags);//ntohs(base->flags);
        primary->crc_type = base->crc;
        index += sizeof(bpv7_hdr);
        res = _bpv7_eid_decode(&primary->dst, buffer, index, bufsz);
        if(res == 0) {
            printf("Decoding EID (dst) failed.\n");
            return 0;
        }
        index += res;
        res = _bpv7_eid_decode(&primary->src, buffer, index, bufsz);
        if(res == 0) {
            printf("Decoding EID (src) failed.\n");
            return 0;
        }
        index += res;
        res = _bpv7_eid_decode(&primary->report, buffer, index, bufsz);
        if(res == 0) {
            printf("Decoding EID (report) failed.\n");
            return 0;
        }
        index += res;
        ++index;  // skip two-element array indicator
        index += cbor_decode_uint(&primary->creation, (uint8_t *)buffer, index, bufsz);
        index += cbor_decode_uint(&primary->sequence, (uint8_t *)buffer, index, bufsz);
        index += cbor_decode_uint(&primary->lifetime, (uint8_t *)buffer, index, bufsz);
        if(base->hdr.magic == BPV7_MAGIC_FRAGMENT) {
            index += cbor_decode_uint(&primary->offset, (uint8_t *)buffer, index, bufsz);
            index += cbor_decode_uint(&primary->app_length, (uint8_t *)buffer, index, bufsz);
        }
        index += cbor_decode_uint(&crclen, (uint8_t *)buffer, index, bufsz);
        memcpy(primary->crc_data, buffer + index, crclen);
        index += crclen;

        return index;
    }

}

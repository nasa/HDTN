#include <iostream>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <random>
#include <cassert>

#include "codec/bpv7.h"
#include "codec/bpv6.h"
#include "util/tsc.h"
#include <inttypes.h>

using namespace hdtn;
using namespace std;

#define BP_ENCODE_COUNT  (1 << 22)
#define BP_ENCODE_RANGE  (0xFFFF)
#define BP_SANITY_COUNT  (6)

void print_usage(char* argv[]) {
    printf("usage: %s [-6] [-7] [-s] [-h] [-c <count>] [-r <range_upper>]\n", argv[0]);
    printf("-6: test BPv6 encode speed\n");
    printf("-7: test BPv7 encode speed\n");
    printf("-h: display information on usage (read: this blurb)\n");
    printf("-s: run a sanity check of BPv6 / BPv7 encoding\n");
}

int main(int argc, char* argv[]) {
    cbor_init();
    uint8_t src[16];
    memset(src, 0, 16);
    uint64_t dst = 0;

    uint64_t count = BP_ENCODE_COUNT;
    uint64_t range = BP_ENCODE_RANGE;
    
    bool sanity = false;
    bool test_v7 = false;
    bool test_v6 = false;

    int index;
    int c;
    opterr = 0;


    while ((c = getopt (argc, argv, "67c:hr:s")) != -1)
        switch (c)
        {
        case '6':
            test_v6 = true;
        break;
        case '7':
            test_v7 = true;
        break;
        case 'c':
            count = atoll(optarg);
        break;
        case 'h':
            print_usage(argv);
            return 0;
        break;
        case 'r':
            range = atoll(optarg);
        break;
        case 's':
            sanity = true;
        break;
        case '?':
        default:
            print_usage(argv);
            abort ();
    }

    if(!sanity && !test_v6 && !test_v7) {
        fprintf(stderr, "A mode of operation must be specified (e.g. '-6')\n");
        print_usage(argv);
        return -1;
    }

    if(sanity) {
        std::cout << "Running integer codec sanity check ..." << std::endl;
        uint64_t sanity_encode[BP_SANITY_COUNT] = {0, 127, 255, 0xABC, 0x123456, 0x100000000000};
        for(int i = 0; i < BP_SANITY_COUNT; ++i) {
            uint64_t sanity_tmp = 0;
            uint8_t  sanity_adv[2] = {0, 0};
            sanity_adv[0] = bpv6_sdnv_encode(sanity_encode[i], (char *)src, 0, 16);
            sanity_adv[1] = bpv6_sdnv_decode(&sanity_tmp, (char *)src, 0, 16);
            if(sanity_tmp != sanity_encode[i]) {
                std::cerr << "SDNV Sanity check failed ..." << std::endl;
                std::cerr << "Encoded `" << sanity_encode[i] << "` into ";
                int j = 0;
                printf("0x%02x ", (uint8_t)src[j]);
                if(src[j] & 0x80) {
                    j++;
                }
                while(src[j] & 0x80) {
                    printf("0x%02x ", (uint8_t)src[j]);
                    ++j;
                }
                std::cerr << "and then back into `" << sanity_tmp << "`" << std::endl;
                return -1;
            }
            if(sanity_adv[0] != sanity_adv[1]) {
                std::cerr << "SDNV Sanity check failed ..." << std::endl;
                std::cerr << "Length mismatch: " << int(sanity_adv[0]) << " bytes to encode and " << int(sanity_adv[1]) << " to decode." << std::endl;
                return -1;
            }
        }
        
        for(int i = 0; i < BP_SANITY_COUNT; ++i) {
            uint64_t sanity_tmp = 0;
            uint8_t  sanity_adv[2] = {0, 0};
            sanity_adv[0] = cbor_encode_uint((uint8_t *)src, sanity_encode[i], 0, 16);
            sanity_adv[1] = cbor_decode_uint(&sanity_tmp, (uint8_t *)src, 0, 16);
            if(sanity_tmp != sanity_encode[i]) {
                std::cerr << "CBOR sanity check failed ..." << std::endl;
                std::cerr << "Encoded `" << sanity_encode[i] << "` into ";
                for(int j = 0; j < 9; ++j) {
                    printf("0x%02x ", src[j]);
                }
                std::cerr << "and then back into `" << sanity_tmp << "`" << std::endl;
                return -1;
            }
            if(sanity_adv[0] != sanity_adv[1]) {
                std::cerr << "CBOR sanity check failed ..." << std::endl;
                std::cerr << "Length mismatch: " << int(sanity_adv[0]) << " bytes to encode and " << int(sanity_adv[1]) << " to decode." << std::endl;
                return -1;
            }
        }

        memset(src, 0, 16);
        dst = 0;
        std::cout << "Integer codec seems as sane as it can be ..." << std::endl;
    }

    if(!test_v6 && !test_v7) {
        printf("Exiting immediately upon completion of sanity check.\n");
        return 0;
    }

    std::random_device rd;
    std::default_random_engine generator(rd());
    std::uniform_int_distribution<uint64_t> rand64(0, range);
    std::cout << "Generating values to encode ..." << std::endl;
    uint64_t * to_encode = new uint64_t[count];
    for(uint64_t i = 0; i < count; ++i) {
        to_encode[i] = rand64(generator);
    }
    std::cout << "Generated " << count << " values between 0 and " << range << std::endl;
    std::cout << "Generation complete." << std::endl;
    
    uint64_t cbor_bytes_total = 0;
    uint64_t sdnv_bytes_total = 0;

    if(test_v7) {
        for(uint64_t i = 0; i < count; ++i) {
            uint8_t adv;
            adv = cbor_encode_uint(src, to_encode[i], 0, 16);
            cbor_bytes_total += adv;
            adv = cbor_decode_uint(&dst, src, 0, 16);
            if(dst != to_encode[i]) {
                printf("[sanity] encode / decode for value `%" PRIu64 "` failed (got %" PRIu64 ").\n", to_encode[i], dst);
            }
            assert(dst == to_encode[i]);
        }
    }

    if(test_v6) {
        for(uint64_t i = 0; i < count; ++i) {
            uint64_t tmp;
            uint8_t adv;
            adv = bpv6_sdnv_encode(to_encode[i], (char *)src, 0, 16);
            sdnv_bytes_total += adv;
            adv = bpv6_sdnv_decode(&tmp, (char *)src, 0, 16);
            assert(tmp == to_encode[i]);
        }
    }

    if(test_v7) {
        printf("[CBOR statistics]\n");
        std::cout << "Total encoded bytes: " << cbor_bytes_total << std::endl;
        std::cout << "Values completed: " << count << std::endl;
        printf("Average bytes / value: %0.6f\n", cbor_bytes_total / (double)count);
        printf("\n");
    }

    if(test_v6) {
        printf("[SDNV statistics]\n");
        std::cout << "Total encoded bytes: " << sdnv_bytes_total << std::endl;
        std::cout << "Values completed: " << count << std::endl;
        printf("Average bytes / value: %0.6f\n\n", sdnv_bytes_total / (double)count);
    }

    if(test_v6 && test_v7) {
        printf("[Relative Efficiency]\n");
        printf("SDNVs were %0.2f%% the size of equivalent CBOR\n", 100.0 * (sdnv_bytes_total / (double)cbor_bytes_total));
    }

    delete[] to_encode;
    return 0;
}

/**
 * CRC support for bplib bundle integrity block.
 * This code is from GSFC bplib so that CRCs will be calculated in the same manner
 * https://github.com/nasa/bplib
 */
#ifndef CRC_H
#define CRC_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdlib.h>
#include <stdint.h>
#include "codec/bpv6-ext-block.h"

/*Bundle integrity types for bplib BIB*/
#define BPLIB_BIB_NONE                     0
#define BPLIB_BIB_CRC16_X25                1
#define BPLIB_BIB_CRC32_CASTAGNOLI         2

#define BYTE_COMBOS 256 /* Number of different possible bytes. */
/* Precalculated byte reflection table from bplib https://github.com/nasa/bplib */
static const uint8_t byte_reflections_table[BYTE_COMBOS] =
{
    0x00, 0x80, 0x40, 0xc0, 0x20, 0xa0, 0x60, 0xe0, 0x10, 0x90, 0x50, 0xd0, 0x30, 0xb0, 0x70, 0xf0,
    0x08, 0x88, 0x48, 0xc8, 0x28, 0xa8, 0x68, 0xe8, 0x18, 0x98, 0x58, 0xd8, 0x38, 0xb8, 0x78, 0xf8,
    0x04, 0x84, 0x44, 0xc4, 0x24, 0xa4, 0x64, 0xe4, 0x14, 0x94, 0x54, 0xd4, 0x34, 0xb4, 0x74, 0xf4,
    0x0c, 0x8c, 0x4c, 0xcc, 0x2c, 0xac, 0x6c, 0xec, 0x1c, 0x9c, 0x5c, 0xdc, 0x3c, 0xbc, 0x7c, 0xfc,
    0x02, 0x82, 0x42, 0xc2, 0x22, 0xa2, 0x62, 0xe2, 0x12, 0x92, 0x52, 0xd2, 0x32, 0xb2, 0x72, 0xf2,
    0x0a, 0x8a, 0x4a, 0xca, 0x2a, 0xaa, 0x6a, 0xea, 0x1a, 0x9a, 0x5a, 0xda, 0x3a, 0xba, 0x7a, 0xfa,
    0x06, 0x86, 0x46, 0xc6, 0x26, 0xa6, 0x66, 0xe6, 0x16, 0x96, 0x56, 0xd6, 0x36, 0xb6, 0x76, 0xf6,
    0x0e, 0x8e, 0x4e, 0xce, 0x2e, 0xae, 0x6e, 0xee, 0x1e, 0x9e, 0x5e, 0xde, 0x3e, 0xbe, 0x7e, 0xfe,
    0x01, 0x81, 0x41, 0xc1, 0x21, 0xa1, 0x61, 0xe1, 0x11, 0x91, 0x51, 0xd1, 0x31, 0xb1, 0x71, 0xf1,
    0x09, 0x89, 0x49, 0xc9, 0x29, 0xa9, 0x69, 0xe9, 0x19, 0x99, 0x59, 0xd9, 0x39, 0xb9, 0x79, 0xf9,
    0x05, 0x85, 0x45, 0xc5, 0x25, 0xa5, 0x65, 0xe5, 0x15, 0x95, 0x55, 0xd5, 0x35, 0xb5, 0x75, 0xf5,
    0x0d, 0x8d, 0x4d, 0xcd, 0x2d, 0xad, 0x6d, 0xed, 0x1d, 0x9d, 0x5d, 0xdd, 0x3d, 0xbd, 0x7d, 0xfd,
    0x03, 0x83, 0x43, 0xc3, 0x23, 0xa3, 0x63, 0xe3, 0x13, 0x93, 0x53, 0xd3, 0x33, 0xb3, 0x73, 0xf3,
    0x0b, 0x8b, 0x4b, 0xcb, 0x2b, 0xab, 0x6b, 0xeb, 0x1b, 0x9b, 0x5b, 0xdb, 0x3b, 0xbb, 0x7b, 0xfb,
    0x07, 0x87, 0x47, 0xc7, 0x27, 0xa7, 0x67, 0xe7, 0x17, 0x97, 0x57, 0xd7, 0x37, 0xb7, 0x77, 0xf7,
    0x0f, 0x8f, 0x4f, 0xcf, 0x2f, 0xaf, 0x6f, 0xef, 0x1f, 0x9f, 0x5f, 0xdf, 0x3f, 0xbf, 0x7f, 0xff
};


/* Parameters specific to calculating 16 bit crc parameters. */
typedef struct crc16_parameters
{
    uint16_t generator_polynomial;   /* The generator polynomial used to compute the CRC. */
    uint16_t initial_value;          /* The value used to initialize a CRC. */
    uint16_t final_xor;              /* The final value to xor with the crc before returning. */
    uint16_t check_value;            /* The crc resulting from the input string "123456789". */
    uint16_t xor_table[BYTE_COMBOS]; /* A ptr to a table with the precomputed XOR values. */
} crc16_parameters_t;

/* Parameters specific to calculating 32 bit crcs. */
typedef struct crc32_parameters
{
    uint32_t generator_polynomial;   /* The generator polynomial used to compute the CRC. */
    uint32_t initial_value;          /* The value used to initialize a CRC. */
    uint32_t final_xor;              /* The final value to xor with the crc before returning. */
    uint32_t check_value;            /* The crc resulting from the input string "123456789". */
    uint32_t xor_table[BYTE_COMBOS]; /* A ptr to a table with the precomputed XOR values. */
} crc32_parameters_t;

/* Standard parameters for calculating a CRC. */
typedef struct crc_parameters
{
    const char* name;                 /* Name of the CRC. */
    int length;                 /* The number of bits in the CRC. */
    bool should_reflect_input;  /* Whether to reflect the bits of the input bytes. */
    bool should_reflect_output; /* Whether to reflect the bits of the output crc. */
    /* Parameters specific to crc implementations of various lengths. The field that is populated
       within this union should directly coincide with the length member.
       Ex: If length == 16 crc16 should be popualted in this union below. */
    union {
        crc16_parameters_t crc16;
        crc32_parameters_t crc32;
    } n_bit_params;
} crc_parameters_t;

/**
 * Reflects the bits of a uint8_t.
 *
 * @param num A uint8_t to reflect.
 * @return reflection
 */

static inline uint8_t reflect8(uint8_t num)
{
    return byte_reflections_table[num];
}

/**
 * Reflects the bits of a uint16_t.
 *
 * @param num A uint16_t to reflect. 
 * @return reflection
 */
static uint16_t reflect16(uint16_t num)
{
    uint16_t reflected_num = 0;
    uint8_t* num_bytes = (uint8_t *) &num;
    uint8_t* reflected_bytes = (uint8_t *) &reflected_num;
    reflected_bytes[0] = reflect8(num_bytes[1]);
    reflected_bytes[1] = reflect8(num_bytes[0]);
    return reflected_num;
}

/**
 * Populates a crc16_table with the different combinations of bytes
 * XORed with the generator polynomial for a given CRC.
 *
 * @param params: A ptr to a crc_parameters_t to populate with a lookup table. 
 */
static void init_crc16_table(crc_parameters_t* params);

/**
 * Calculates the CRC from a byte array of a given length using a
 * 16 bit CRC lookup table.
 *
 * @param data A ptr to a byte array containing data to calculate a CRC over. 
 * @param length The length of the provided data in bytes. 
 * @param params A ptr to a crc_parameters_t struct defining how to calculate the crc and has
 *      an XOR lookup table. 
 * @return A crc remainder of the provided data.
 */
static uint16_t get_crc16(const uint8_t* data, uint32_t length, const crc_parameters_t* params);

/**
 * Verify a payload against the CRC of a bundle integrity block.
 * Only CRC16 is included currently.
 * @param payload  pointer to payload memory buffer 
 * @param payload_size  number of bytes to crc over 
 * @param bib - pointer to a bundle integrity block structure used to write the block
 * @return return 1 for success, 0 for failure
 */
int bib_verify (void* payload, int payload_size, bpv6_bplib_bib_block* bib);

#ifdef __cplusplus
}
#endif

#endif //CRC_H

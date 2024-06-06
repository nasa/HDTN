//https://github.com/madler/zlib/blob/2fa463bacfff79181df1a5270fb67cc679a53e71/crc32.c#L361
/* crc32.c -- compute the CRC-32 of a data stream
 * Copyright (C) 1995-2006, 2010, 2011, 2012, 2016 Mark Adler
 * For conditions of distribution and use, see copyright notice in zlib.h
 */


#include "codec/Bpv7Crc.h"


#define GF2_DIM 32      /* dimension of GF(2) vectors (length of CRC) */

/* ========================================================================= */
static uint32_t gf2_matrix_times(const uint32_t* mat, uint32_t vec) {
    uint32_t sum = 0;
    while (vec) {
        if (vec & 1)
            sum ^= *mat;
        vec >>= 1;
        mat++;
    }
    return sum;
}

/* ========================================================================= */
static void gf2_matrix_square(uint32_t* square, const uint32_t* mat) {
    for (int n = 0; n < GF2_DIM; n++)
        square[n] = gf2_matrix_times(mat, mat[n]);
}

//#define CRC_COMBINE_USE_ORIGINAL_ALGORITHM 1
#ifdef CRC_COMBINE_USE_ORIGINAL_ALGORITHM
static uint32_t crc32_combine_(uint32_t crc1, uint32_t crc2, std::size_t len2) {
    int n;
    uint32_t row;
    uint32_t even[GF2_DIM];    /* even-power-of-two zeros operator */
    uint32_t odd[GF2_DIM];     /* odd-power-of-two zeros operator */

    /* degenerate case (also disallow negative lengths) */
    if (len2 <= 0)
        return crc1;

    /* put operator for one zero bit in odd */
    //odd[0] = 0xedb88320UL;          /* CRC-32 polynomial */
    odd[0] = 0x82f63b78;//CRC32C polynomial (result of reflect_unsigned)
    row = 1;
    for (n = 1; n < GF2_DIM; n++) {
        odd[n] = row;
        row <<= 1;
    }

    /* put operator for two zero bits in even */
    gf2_matrix_square(even, odd);

    /* put operator for four zero bits in odd */
    gf2_matrix_square(odd, even);

    /* apply len2 zeros to crc1 (first square will put the operator for one
       zero byte, eight zero bits, in even) */
    do {
        /* apply zeros operator for this bit of len2 */
        gf2_matrix_square(even, odd);
        if (len2 & 1)
            crc1 = gf2_matrix_times(even, crc1);
        len2 >>= 1;

        /* if no more bits set, then done */
        if (len2 == 0)
            break;

        /* another iteration of the loop with odd and even swapped */
        gf2_matrix_square(odd, even);
        if (len2 & 1)
            crc1 = gf2_matrix_times(odd, crc1);
        len2 >>= 1;

        /* if no more bits set, then done */
    } while (len2 != 0);

    /* return combined crc */
    crc1 ^= crc2;
    return crc1;
}
#else //not using original algorithm

/**
 * @file ZlibCrc32cCombined.cpp
 * @author  Brian Tomko <brian.j.tomko@nasa.gov>
 *
 * @copyright Copyright (c) 2021 United States Government as represented by
 * the National Aeronautics and Space Administration.
 * No copyright is claimed in the United States under Title 17, U.S.Code.
 * All Other Rights Reserved.
 *
 * @section LICENSE
 * Released under the NASA Open Source Agreement (NOSA)
 * See LICENSE.md in the source root directory for more information.
 *
 * @section DESCRIPTION
 *
 * The remainder of this file contains optimizations to
 * the original zlib crc32_combine_ function, using lookup tables.
 */

struct EvenAndOdd {
    uint32_t even[GF2_DIM];    /* even-power-of-two zeros operator */
    uint32_t odd[GF2_DIM];     /* odd-power-of-two zeros operator */
};
struct CrcCombineInit {
    EvenAndOdd zerosOperator;

    CrcCombineInit() {
        /* put operator for one zero bit in odd */
        //odd[0] = 0xedb88320UL;          /* CRC-32 polynomial */
        // 
        //https://stackoverflow.com/questions/41878655/why-are-crc-polynomials-given-as-normal-reversed-etc#comment110168877_41879464
        //So how do you reverse it? If I have 0x04C11DB7 polynomial how do I reverse it into 0xEDB88320? 
        //Just write it in binary and reverse the 32 bits.
        //00000100110000010001110110110111 which reversed is 11101101101110001000001100100000, equal to 0xedb88320. - Mark Adler Jun 9, 2020 at 21:02
        // 
        //odd[0] = boost::detail::reflect_unsigned<uint32_t>(0x1EDC6F41);//CRC32C polynomial (must #include <boost/crc.hpp>)
        zerosOperator.odd[0] = 0x82f63b78;//CRC32C polynomial (result of reflect_unsigned above)
        uint32_t row = 1;
        for (int n = 1; n < GF2_DIM; n++) {
            zerosOperator.odd[n] = row;
            row <<= 1;
        }

        /* put operator for two zero bits in even */
        gf2_matrix_square(zerosOperator.even, zerosOperator.odd);

        /* put operator for four zero bits in odd */
        gf2_matrix_square(zerosOperator.odd, zerosOperator.even);
    }
};

#define CRC_COMBINE_USE_LUT 1
#ifdef CRC_COMBINE_USE_LUT
struct CrcCombineLut {
    EvenAndOdd zerosOperators[32];

    CrcCombineLut() {
        static const CrcCombineInit CRC_COMBINED_INIT;
        CrcCombineInit vals = CRC_COMBINED_INIT;

        /* apply len2 zeros to crc1 (first square will put the operator for one
           zero byte, eight zero bits, in even) */
        for(unsigned int i=0; i<=31; ++i) {
            /* apply zeros operator for this bit of len2 */
            gf2_matrix_square(vals.zerosOperator.even, vals.zerosOperator.odd);

            /* another iteration of the loop with odd and even swapped */
            gf2_matrix_square(vals.zerosOperator.odd, vals.zerosOperator.even);

            zerosOperators[i] = vals.zerosOperator;
        }
    }
};

static uint32_t crc32_combine_(uint32_t crc1, uint32_t crc2, std::size_t len2) {
    static const CrcCombineLut LUT;

    /* degenerate case (also disallow negative lengths) */
    if (len2 <= 0)
        return crc1;


    /* apply len2 zeros to crc1 (first square will put the operator for one
       zero byte, eight zero bits, in even) */
    unsigned int i = 0;
    do {
        /* apply zeros operator for this bit of len2 */
        //gf2_matrix_square(vals.zerosOperator.even, vals.zerosOperator.odd);
        if (len2 & 1)
            crc1 = gf2_matrix_times(LUT.zerosOperators[i].even, crc1);
        len2 >>= 1;

        /* if no more bits set, then done */
        if (len2 == 0)
            break;

        /* another iteration of the loop with odd and even swapped */
        //gf2_matrix_square(vals.zerosOperator.odd, vals.zerosOperator.even);
        if (len2 & 1)
            crc1 = gf2_matrix_times(LUT.zerosOperators[i].odd, crc1);
        len2 >>= 1;

        ++i;

        /* if no more bits set, then done */
    } while (len2 != 0);

    /* return combined crc */
    crc1 ^= crc2;
    return crc1;
}
#else //not CRC_COMBINE_USE_LUT (using some initialization optimization)

/* ========================================================================= */
static uint32_t crc32_combine_(uint32_t crc1, uint32_t crc2, std::size_t len2) {
    static const CrcCombineInit CRC_COMBINED_INIT;

    /* degenerate case (also disallow negative lengths) */
    if (len2 <= 0)
        return crc1;

    CrcCombineInit vals = CRC_COMBINED_INIT;

    /* apply len2 zeros to crc1 (first square will put the operator for one
       zero byte, eight zero bits, in even) */
    do {
        /* apply zeros operator for this bit of len2 */
        gf2_matrix_square(vals.zerosOperator.even, vals.zerosOperator.odd);
        if (len2 & 1)
            crc1 = gf2_matrix_times(vals.zerosOperator.even, crc1);
        len2 >>= 1;

        /* if no more bits set, then done */
        if (len2 == 0)
            break;

        /* another iteration of the loop with odd and even swapped */
        gf2_matrix_square(vals.zerosOperator.odd, vals.zerosOperator.even);
        if (len2 & 1)
            crc1 = gf2_matrix_times(vals.zerosOperator.odd, crc1);
        len2 >>= 1;

        /* if no more bits set, then done */
    } while (len2 != 0);

    /* return combined crc */
    crc1 ^= crc2;
    return crc1;
}


#endif //CRC_COMBINE_USE_LUT

#endif //CRC_COMBINE_USE_ORIGINAL_ALGORITHM

uint32_t Bpv7Crc::Crc32cCombine(uint32_t crc1, uint32_t crc2, std::size_t len2) {
    return crc32_combine_(crc1, crc2, len2);
}

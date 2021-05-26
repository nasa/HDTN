/***************************************************************************
 * NASA Glenn Research Center, Cleveland, OH
 * Released under the NASA Open Source Agreement (NOSA)
 * May  2021
 *
 ****************************************************************************
 */

#include "codec/crc.h"

static crc_parameters_t crc16_x25 = {
    /*.name                            =*/ "CRC-16 X25",
    /*.length                          =*/ 16,
    /*.should_reflect_input            =*/ true,
    /*.should_reflect_output           =*/ true,
    /*.n_bit_params =*/ {
        /*.crc16 =*/ {
            /*.generator_polynomial    =*/ 0x1021,
            /*.initial_value           =*/ 0xFFFF,
            /*.final_xor               =*/ 0xF0B8,
            /*.check_value             =*/ 0x906E
        }
    }
};


static void init_crc16_table(crc_parameters_t* params)
{
    uint16_t generator = params->n_bit_params.crc16.generator_polynomial;
    uint16_t* table = params->n_bit_params.crc16.xor_table;
    uint16_t i;
    int j;

    for (i = 0; i < BYTE_COMBOS; i++)
    {
        table[i] = i << 8;

        for (j = 8; j > 0; j--)
        {
            table[i] = (table[i] & 0x8000) != 0 ? (table[i] << 1) ^ generator : table[i] << 1;
        }
    }
}

static uint16_t get_crc16(const uint8_t* data, uint32_t length, const crc_parameters_t* params)
{
    /* Check that we are always using a lookup table corresponding to the requested crc. */
    uint16_t crc = params->n_bit_params.crc16.initial_value;
    uint8_t current_byte;
    uint32_t i;
    for (i = 0; i < length; i++)
    {
        current_byte = params->should_reflect_input ? reflect8(data[i]) : data[i];
        crc = (crc << 8) ^ params->n_bit_params.crc16.xor_table[current_byte ^ (crc >> 8)];
    }

    if (params->should_reflect_output)
    {
        crc = reflect16(crc);
    }
    /* Perform the final XOR based on the parameters. */
    return crc ^ params->n_bit_params.crc16.final_xor;
}

int bib_verify (void* payload, int payload_size, bpv6_bplib_bib_block* bib)
{
	init_crc16_table(&crc16_x25);
    /* Calculate and Verify Payload CRC */
    if(bib->cipher_suite_id == BPLIB_BIB_CRC16_X25)
    {
        uint16_t crc = get_crc16((uint8_t*)payload, payload_size, &crc16_x25);
        if((uint16_t)bib->security_result != crc)
        {
            /* Return Failure */
        	return 0;
        }
    }

    /* Return Success */
    return 1;
}

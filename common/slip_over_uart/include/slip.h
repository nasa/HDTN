/**
 * @file slip.h
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
 * This file contains functionality for SLIP encode and decode operations.
 */

#ifndef _SLIP_H
#define _SLIP_H

#define SLIP_END     (0xC0)
#define SLIP_ESC     (0xDB)
#define SLIP_ESC_END (0xDC)
#define SLIP_ESC_ESC (0xDD)


#include <stdlib.h>
#include <stdint.h>
#include <boost/config/detail/suffix.hpp>
#include "slip_over_uart_lib_export.h"

#ifdef __cplusplus
extern "C" {
#endif

    typedef struct {
        uint8_t m_previouslyReceivedChar;
    } SlipDecodeState_t;

    //return output size
    SLIP_OVER_UART_LIB_EXPORT unsigned int SlipEncode(const uint8_t * const inputIpPacketRawData, uint8_t * const outputSlipRawData, const unsigned int sizeInput);

    SLIP_OVER_UART_LIB_EXPORT unsigned int SlipEncodeChar(const uint8_t inChar, uint8_t * const outputSlipRawData_size2);
    
    
    SLIP_OVER_UART_LIB_EXPORT void SlipDecodeInit(SlipDecodeState_t * slipDecodeState);
    SLIP_OVER_UART_LIB_EXPORT unsigned int SlipDecodeChar(SlipDecodeState_t * slipDecodeState, const uint8_t inChar, uint8_t * outChar);

    BOOST_FORCEINLINE unsigned int SlipDecodeChar_Inline(SlipDecodeState_t* slipDecodeState, const uint8_t inChar, uint8_t* outChar) {
        if (inChar == SLIP_ESC) {
            slipDecodeState->m_previouslyReceivedChar = inChar;
            return 0; //not complete
        }
        else if (inChar == SLIP_END) {
            return 2; // complete
        }
        else {
            uint8_t charToWrite = inChar;
            if (slipDecodeState->m_previouslyReceivedChar == SLIP_ESC) {
                // Previously read byte was an escape byte
                if (inChar == SLIP_ESC_END) {
                    charToWrite = SLIP_END;
                }
                else if (inChar == SLIP_ESC_ESC) {
                    charToWrite = SLIP_ESC;
                }
            }
            slipDecodeState->m_previouslyReceivedChar = inChar;
            *outChar = charToWrite;
            return 1; //new outChar
        }
    }

#ifdef __cplusplus
}           // closing brace for extern "C" 
#endif

#endif      // _SLIP_H 

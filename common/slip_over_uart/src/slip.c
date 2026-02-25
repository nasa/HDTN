/**
 * @file slip.c
 * @author  Brian Tomko <brian.j.tomko@nasa.gov>
 *
 * @copyright Copyright (c) 2026 United States Government as represented by
 * the National Aeronautics and Space Administration.
 * No copyright is claimed in the United States under Title 17, U.S.Code.
 * All Other Rights Reserved.
 *
 * @section LICENSE
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/*
     |<---------------------------------IP datagram ---------------------------------->|
     ___________________________________________________________________________________
     |                   |   |                             |   |                       |
     |                   | C0|                             |DB |                       |
     |___________________|___|_____________________________|___|_______________________|
     :                   : 1  \                            \ 1  \                       \
     :                   :     :                            :    \                       \
     :                   :     \                             \    \                       \
     :                   :      :                             :    \.                      \
  END:                   :ESC   \                             \ ESC  \                      \  END
  __ :___________________:_______:_____________________________:______\______________________\_____
 |   |                   |   |   |                             |   |   |                      |   |
 |C0 |                   |DB |DC |                             |DB |DD |                      |C0 |
 |___|___________________|___|___|_____________________________|___|___|______________________|___|
   1                       1   1                                 1   1                          1



*/

#include "slip.h"





unsigned int SlipEncode(const uint8_t * const inputIpPacketRawData, uint8_t * const outputSlipRawData, const unsigned int sizeInput) {
    const uint8_t * ptrInputIp = inputIpPacketRawData;
    uint8_t * ptrOutputSlip = outputSlipRawData;
    unsigned int i;

    *ptrOutputSlip++ = SLIP_END;
    for (i = 0; i < sizeInput; ++i) {
        const uint8_t inputC = *ptrInputIp++;
        if (inputC == SLIP_END) {
            *ptrOutputSlip++ = SLIP_ESC;
            *ptrOutputSlip++ = SLIP_ESC_END;
        }
        else if (inputC == SLIP_ESC) {
            *ptrOutputSlip++ = SLIP_ESC;
            *ptrOutputSlip++ = SLIP_ESC_ESC;
        }
        else {
            *ptrOutputSlip++ = inputC;
        }
    }
    *ptrOutputSlip++ = SLIP_END;


    //result is the distance (in array elements) between the two elements
    //since both are uint8_t this will be the size in bytes
    return (unsigned int) (ptrOutputSlip - outputSlipRawData); 
}


// Returns size (1 or 2) depending on if an escape sequence is needed
// Places encoded data for inChar into outputSlipRawData_size2
// Assumes the user will put SLIP_END's before and after using this function 
unsigned int SlipEncodeChar(const uint8_t inChar, uint8_t * const outputSlipRawData_size2) {

    if (inChar == SLIP_END) {
        outputSlipRawData_size2[0] = SLIP_ESC;
        outputSlipRawData_size2[1] = SLIP_ESC_END;
        return 2;
    }
    else if (inChar == SLIP_ESC) {
        outputSlipRawData_size2[0] = SLIP_ESC;
        outputSlipRawData_size2[1] = SLIP_ESC_ESC;
        return 2;
    }
    else {
        outputSlipRawData_size2[0] = inChar;
        return 1;
    }
}

void SlipDecodeInit(SlipDecodeState_t * slipDecodeState) {
    slipDecodeState->m_previouslyReceivedChar = 0;
}

unsigned int SlipDecodeChar(SlipDecodeState_t * slipDecodeState, const uint8_t inChar, uint8_t * outChar) {
    return SlipDecodeChar_Inline(slipDecodeState, inChar, outChar);
}

//Implementation of https://tools.ietf.org/html/rfc6256
// Using Self-Delimiting Numeric Values in Protocols
#include "sdnv.h"

#define SDNV_MAX_1_BYTE  ((1 <<  7) - 1)
#define SDNV_MAX_2_BYTE  ((1 << 14) - 1)
#define SDNV_MAX_3_BYTE  ((1 << 21) - 1)
#define SDNV_MAX_4_BYTE  ((1 << 28) - 1)
#define SDNV_MAX_5_BYTE  ((1 << 35) - 1)



//return output size
unsigned int SdnvEncodeU32(uint8_t * outputEncoded, const uint32_t valToEncodeU32) {
	if(valToEncodeU32 <= SDNV_MAX_1_BYTE) {
		outputEncoded[0] = (uint8_t)(valToEncodeU32 & 0x7f);
		return 1;
	}
	else if(valToEncodeU32 <= SDNV_MAX_2_BYTE) {
		outputEncoded[1] = (uint8_t)(valToEncodeU32 & 0x7f);
		outputEncoded[0] = (uint8_t)(((valToEncodeU32 >> 7) & 0x7f) | 0x80);
		return 2;
	}
	else if(valToEncodeU32 <= SDNV_MAX_3_BYTE) {
		outputEncoded[2] = (uint8_t)(valToEncodeU32 & 0x7f);
		outputEncoded[1] = (uint8_t)(((valToEncodeU32 >> 7) & 0x7f) | 0x80);
		outputEncoded[0] = (uint8_t)(((valToEncodeU32 >> 14) & 0x7f) | 0x80);
		return 3;
	}
	else if(valToEncodeU32 <= SDNV_MAX_4_BYTE) {
		outputEncoded[3] = (uint8_t)(valToEncodeU32 & 0x7f);
		outputEncoded[2] = (uint8_t)(((valToEncodeU32 >> 7) & 0x7f) | 0x80);
		outputEncoded[1] = (uint8_t)(((valToEncodeU32 >> 14) & 0x7f) | 0x80);
		outputEncoded[0] = (uint8_t)(((valToEncodeU32 >> 21) & 0x7f) | 0x80);
		return 4;
	}
	else { //if(valToEncodeU32 <= SDNV_MAX_5_BYTE)
		outputEncoded[4] = (uint8_t)(valToEncodeU32 & 0x7f);
		outputEncoded[3] = (uint8_t)(((valToEncodeU32 >> 7) & 0x7f) | 0x80);
		outputEncoded[2] = (uint8_t)(((valToEncodeU32 >> 14) & 0x7f) | 0x80);
		outputEncoded[1] = (uint8_t)(((valToEncodeU32 >> 21) & 0x7f) | 0x80);
		outputEncoded[0] = (uint8_t)(((valToEncodeU32 >> 28) & 0x7f) | 0x80);
		return 5;
	}
}
	
//return decoded value (0 if failure), also set parameter numBytes taken to decode
uint32_t SdnvDecodeU32(const uint8_t * inputEncoded, uint8_t * numBytes) {
	//(Initial Step) Set the result to 0.  Set an index to the first
	//  byte of the encoded SDNV.
	//(Recursion Step) Shift the result left 7 bits.  Add the low-order
	//   7 bits of the value at the index to the result.  If the high-order
	//   bit under the pointer is a 1, advance the index by one byte within
	//   the encoded SDNV and repeat the Recursion Step, otherwise return
	//   the current value of the result.
	
	uint32_t result = 0;
	uint8_t byteCount;
	for(byteCount=1; byteCount<=5; ++byteCount) {
		result <<= 7;
		const uint8_t currentByte = *inputEncoded++;
		result += (currentByte & 0x7f);
		if((currentByte & 0x80) == 0) { //if msbit is a 0 then stop
			*numBytes = byteCount;
			return result;
		}
	}
	*numBytes = 0;
	return 0;
}


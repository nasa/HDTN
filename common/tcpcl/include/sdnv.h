//Implementation of https://tools.ietf.org/html/rfc6256
// Using Self-Delimiting Numeric Values in Protocols
#ifndef _SDNV_H
#define _SDNV_H




#include <stdlib.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif


//return output size
unsigned int SdnvEncodeU32(uint8_t * outputEncoded, const uint32_t valToEncodeU32);
	
//return decoded value (0 if failure), also set parameter numBytes taken to decode
uint32_t SdnvDecodeU32(const uint8_t * inputEncoded, uint8_t * numBytes);
	

#ifdef __cplusplus
}           // closing brace for extern "C" 
#endif

#endif      // _SDNV_H 

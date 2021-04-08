//Implementation of https://tools.ietf.org/html/rfc6256
// Using Self-Delimiting Numeric Values in Protocols
#ifndef _SDNV_UTIL_H
#define _SDNV_UTIL_H 1




#include <stdlib.h>
#include <stdint.h>

//return output size
unsigned int SdnvEncodeU32(uint8_t * outputEncoded, const uint32_t valToEncodeU32);

//return output size
unsigned int SdnvEncodeU64(uint8_t * outputEncoded, const uint64_t valToEncodeU64);

//return decoded value (0 if failure), also set parameter numBytes taken to decode
uint32_t SdnvDecodeU32(const uint8_t * inputEncoded, uint8_t * numBytes);

//return decoded value (0 if failure), also set parameter numBytes taken to decode
uint64_t SdnvDecodeU64(const uint8_t * inputEncoded, uint8_t * numBytes);


//return output size
unsigned int SdnvEncodeU32Classic(uint8_t * outputEncoded, const uint32_t valToEncodeU32);

//return output size
unsigned int SdnvEncodeU64Classic(uint8_t * outputEncoded, const uint64_t valToEncodeU64);

//return decoded value (0 if failure), also set parameter numBytes taken to decode
uint32_t SdnvDecodeU32Classic(const uint8_t * inputEncoded, uint8_t * numBytes);
	
//return decoded value (0 if failure), also set parameter numBytes taken to decode
uint64_t SdnvDecodeU64Classic(const uint8_t * inputEncoded, uint8_t * numBytes);
	
//return output size
unsigned int SdnvEncodeU64Fast(uint8_t * outputEncoded, const uint64_t valToEncodeU64);

//return decoded value (0 if failure), also set parameter numBytes taken to decode
uint64_t SdnvDecodeU64Fast(const uint8_t * data, uint8_t * numBytes);

//return num values decoded this iteration
unsigned int SdnvDecodeMultipleU64Fast(const uint8_t * data, uint8_t * numBytes, uint64_t * decodedValues, unsigned int decodedRemaining);

//return num values decoded this iteration
unsigned int SdnvDecodeMultiple256BitU64Fast(const uint8_t * data, uint8_t * numBytes, uint64_t * decodedValues, unsigned int decodedRemaining);

#endif      // _SDNV_UTIL_H 
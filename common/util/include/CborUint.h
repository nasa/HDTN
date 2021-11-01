//Implementation of https://datatracker.ietf.org/doc/html/rfc8949 for CBOR unsigned integers only
#ifndef _CBOR_UINT_H
#define _CBOR_UINT_H 1




#include <stdlib.h>
#include <stdint.h>

//return output size
//unsigned int CborGetNumBytesRequiredToEncode(const uint64_t valToEncodeU64);


//return output size
unsigned int CborEncodeU64(uint8_t * const outputEncoded, const uint64_t valToEncodeU64, const uint64_t bufferSize);

//return output size
unsigned int CborEncodeU64BufSize9(uint8_t * const outputEncoded, const uint64_t valToEncodeU64);


//return decoded value (return invalid number that must be ignored on failure)
//  also sets parameter numBytes taken to decode (set to 0 on failure)
uint64_t CborDecodeU64(const uint8_t * const inputEncoded, uint8_t * numBytes, const uint64_t bufferSize);

//return decoded value (return invalid number that must be ignored on failure)
//  also sets parameter numBytes taken to decode (set to 0 on failure)
uint64_t CborDecodeU64BufSize9(const uint8_t * const inputEncoded, uint8_t * numBytes);


//return output size
unsigned int CborEncodeU64Classic(uint8_t * const outputEncoded, const uint64_t valToEncodeU64, const uint64_t bufferSize);

//return output size
unsigned int CborEncodeU64ClassicBufSize9(uint8_t * const outputEncoded, const uint64_t valToEncodeU64);

//return decoded value (return invalid number that must be ignored on failure)
//  also sets parameter numBytes taken to decode (set to 0 on failure)
uint64_t CborDecodeU64Classic(const uint8_t * const inputEncoded, uint8_t * numBytes, const uint64_t bufferSize);

//return decoded value (return invalid number that must be ignored on failure)
//  also sets parameter numBytes taken to decode (set to 0 on failure)
uint64_t CborDecodeU64ClassicBufSize9(const uint8_t * const inputEncoded, uint8_t * numBytes);
	
#ifdef USE_X86_HARDWARE_ACCELERATION
//return output size
unsigned int CborEncodeU64Fast(uint8_t * const outputEncoded, const uint64_t valToEncodeU64, const uint64_t bufferSize);

//return output size
unsigned int CborEncodeU64FastBufSize9(uint8_t * const outputEncoded, const uint64_t valToEncodeU64);

//return decoded value (return invalid number that must be ignored on failure)
//  also sets parameter numBytes taken to decode (set to 0 on failure)
uint64_t CborDecodeU64Fast(const uint8_t * const inputEncoded, uint8_t * numBytes, const uint64_t bufferSize);

//return decoded value (return invalid number that must be ignored on failure)
//  also sets parameter numBytes taken to decode (set to 0 on failure)
uint64_t CborDecodeU64FastBufSize9(const uint8_t * const inputEncoded, uint8_t * numBytes);

#endif //#ifdef USE_X86_HARDWARE_ACCELERATION

#endif      // _CBOR_UINT_H 

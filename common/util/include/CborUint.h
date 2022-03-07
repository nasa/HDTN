//Implementation of https://datatracker.ietf.org/doc/html/rfc8949 for CBOR unsigned integers only
#ifndef _CBOR_UINT_H
#define _CBOR_UINT_H 1




#include <stdlib.h>
#include <stdint.h>
#include <vector>
#include "hdtn_util_export.h"

//return output size
//unsigned int CborGetNumBytesRequiredToEncode(const uint64_t valToEncodeU64);


//return output size
HDTN_UTIL_EXPORT unsigned int CborEncodeU64(uint8_t * const outputEncoded, const uint64_t valToEncodeU64, const uint64_t bufferSize);

//return output size
HDTN_UTIL_EXPORT unsigned int CborEncodeU64BufSize9(uint8_t * const outputEncoded, const uint64_t valToEncodeU64);


//return decoded value (return invalid number that must be ignored on failure)
//  also sets parameter numBytes taken to decode (set to 0 on failure)
HDTN_UTIL_EXPORT uint64_t CborDecodeU64(const uint8_t * const inputEncoded, uint8_t * numBytes, const uint64_t bufferSize);

//return decoded value (return invalid number that must be ignored on failure)
//  also sets parameter numBytes taken to decode (set to 0 on failure)
HDTN_UTIL_EXPORT uint64_t CborDecodeU64BufSize9(const uint8_t * const inputEncoded, uint8_t * numBytes);


//return output size
HDTN_UTIL_EXPORT unsigned int CborEncodeU64Classic(uint8_t * const outputEncoded, const uint64_t valToEncodeU64, const uint64_t bufferSize);

//return output size
HDTN_UTIL_EXPORT unsigned int CborEncodeU64ClassicBufSize9(uint8_t * const outputEncoded, const uint64_t valToEncodeU64);

//return output size
HDTN_UTIL_EXPORT unsigned int CborGetEncodingSizeU64Classic(const uint64_t valToEncodeU64);

//return decoded value (return invalid number that must be ignored on failure)
//  also sets parameter numBytes taken to decode (set to 0 on failure)
HDTN_UTIL_EXPORT uint64_t CborDecodeU64Classic(const uint8_t * const inputEncoded, uint8_t * numBytes, const uint64_t bufferSize);

//return decoded value (return invalid number that must be ignored on failure)
//  also sets parameter numBytes taken to decode (set to 0 on failure)
HDTN_UTIL_EXPORT uint64_t CborDecodeU64ClassicBufSize9(const uint8_t * const inputEncoded, uint8_t * numBytes);

//return output size
#define CborGetEncodingSizeU64 CborGetNumBytesRequiredToEncode
//this version shall be used regardless of USE_CBOR_FAST
HDTN_UTIL_EXPORT unsigned int CborGetNumBytesRequiredToEncode(const uint64_t valToEncodeU64);

#ifdef USE_CBOR_FAST
//return output size
HDTN_UTIL_EXPORT unsigned int CborEncodeU64Fast(uint8_t * const outputEncoded, const uint64_t valToEncodeU64, const uint64_t bufferSize);

//return output size
HDTN_UTIL_EXPORT unsigned int CborEncodeU64FastBufSize9(uint8_t * const outputEncoded, const uint64_t valToEncodeU64);

//return output size
#define CborGetEncodingSizeU64Fast CborGetNumBytesRequiredToEncode

//return decoded value (return invalid number that must be ignored on failure)
//  also sets parameter numBytes taken to decode (set to 0 on failure)
HDTN_UTIL_EXPORT uint64_t CborDecodeU64Fast(const uint8_t * const inputEncoded, uint8_t * numBytes, const uint64_t bufferSize);

//return decoded value (return invalid number that must be ignored on failure)
//  also sets parameter numBytes taken to decode (set to 0 on failure)
HDTN_UTIL_EXPORT uint64_t CborDecodeU64FastBufSize9(const uint8_t * const inputEncoded, uint8_t * numBytes);

#endif //#ifdef USE_CBOR_FAST

//array ops
HDTN_UTIL_EXPORT uint64_t CborTwoUint64ArraySerialize(uint8_t * serialization, const uint64_t element1, const uint64_t element2);
HDTN_UTIL_EXPORT uint64_t CborTwoUint64ArraySerialize(uint8_t * serialization, const uint64_t element1, const uint64_t element2, uint64_t bufferSize);
HDTN_UTIL_EXPORT uint64_t CborTwoUint64ArraySerializationSize(const uint64_t element1, const uint64_t element2);
HDTN_UTIL_EXPORT bool CborTwoUint64ArrayDeserialize(const uint8_t * serialization, uint8_t * numBytesTakenToDecode, uint64_t bufferSize, uint64_t & element1, uint64_t & element2);

HDTN_UTIL_EXPORT uint64_t CborArbitrarySizeUint64ArraySerialize(uint8_t * serialization, const std::vector<uint64_t> & elements);
HDTN_UTIL_EXPORT uint64_t CborArbitrarySizeUint64ArraySerialize(uint8_t * serialization, const std::vector<uint64_t> & elements, uint64_t bufferSize);
HDTN_UTIL_EXPORT uint64_t CborArbitrarySizeUint64ArraySerializationSize(const std::vector<uint64_t> & elements);
HDTN_UTIL_EXPORT bool CborArbitrarySizeUint64ArrayDeserialize(uint8_t * serialization, uint64_t & numBytesTakenToDecode, uint64_t bufferSize, std::vector<uint64_t> & elements, const uint64_t maxElements);

#endif      // _CBOR_UINT_H 

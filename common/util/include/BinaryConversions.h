//Implementation of https://datatracker.ietf.org/doc/html/rfc8949 for CBOR unsigned integers only
#ifndef _BINARY_CONVERSIONS_H
#define _BINARY_CONVERSIONS_H 1
#include <stdlib.h>
#include <stdint.h>
#include <vector>
#include <string>
#include <boost/version.hpp>
#include "hdtn_util_export.h"

class HDTN_UTIL_EXPORT BinaryConversions {
public:
#if (BOOST_VERSION >= 106600)
    static void DecodeBase64(const std::string & strBase64, std::vector<uint8_t> & binaryDecodedMessage);
    static void EncodeBase64(const std::vector<uint8_t> & binaryMessage, std::string & strBase64);
#endif
    static void BytesToHexString(const std::vector<uint8_t> & bytes, std::string & hexString);
    static bool HexStringToBytes(const std::string & hexString, std::vector<uint8_t> & bytes);
};
#endif      // _BINARY_CONVERSIONS_H 

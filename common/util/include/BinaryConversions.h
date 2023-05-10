/**
 * @file BinaryConversions.h
 * @author  Brian Tomko <brian.j.tomko@nasa.gov>
 *
 * @copyright Copyright © 2021 United States Government as represented by
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
 * This BinaryConversions static class is a utility used to
 * 1.) Convert between a Base64 std::string and an std::vector<uint8_t> byte string.
 * 2.) Convert between a Hex std::string and an std::vector<uint8_t> byte string.
 */

#ifndef _BINARY_CONVERSIONS_H
#define _BINARY_CONVERSIONS_H 1
#include <stdlib.h>
#include <stdint.h>
#include <vector>
#include <string>
#include <boost/asio/buffer.hpp>
#include <boost/version.hpp>
#include "PaddedVectorUint8.h"
#include "hdtn_util_export.h"

class HDTN_UTIL_EXPORT BinaryConversions {
public:
#if (BOOST_VERSION >= 106600)
    static void DecodeBase64(const std::string & strBase64, std::vector<uint8_t> & binaryDecodedMessage);
    static void EncodeBase64(const std::vector<uint8_t> & binaryMessage, std::string & strBase64);
#endif
    static void BytesToHexString(const std::vector<uint8_t> & bytes, std::string & hexString);
    static void BytesToHexString(const padded_vector_uint8_t& bytes, std::string& hexString);
    static void BytesToHexString(const void* data, std::size_t size, std::string& hexString);
    static void BytesToHexString(const std::vector<boost::asio::const_buffer>& bytes, std::string& hexString);
    static void BytesToHexString(const boost::asio::const_buffer& bytes, std::string& hexString);
    static bool HexStringToBytes(const std::string& hexString, padded_vector_uint8_t& bytes);
    static bool HexStringToBytes(const std::string & hexString, std::vector<uint8_t> & bytes);
};
#endif      // _BINARY_CONVERSIONS_H 

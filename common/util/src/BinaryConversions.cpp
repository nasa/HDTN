#include "BinaryConversions.h"
#include <iostream>
#include <boost/algorithm/string.hpp>
#include <boost/algorithm/hex.hpp>
#if (BOOST_VERSION >= 106600)
#include <boost/beast/core/detail/base64.hpp>
void BinaryConversions::DecodeBase64(const std::string & strBase64, std::vector<uint8_t> & binaryDecodedMessage) {
    //The memory pointed to by `out` points to valid memory of at least `decoded_size(len)` bytes.
    const std::size_t decodedMinimumSize = boost::beast::detail::base64::decoded_size(strBase64.length());
    binaryDecodedMessage.resize(decodedMinimumSize + 5);
    std::pair<std::size_t, std::size_t> sizesB64 = boost::beast::detail::base64::decode(binaryDecodedMessage.data(), strBase64.data(), strBase64.length());
    binaryDecodedMessage.resize(sizesB64.first); //first is The number of octets written to `out
}

void BinaryConversions::EncodeBase64(const std::vector<uint8_t> & binaryMessage, std::string & strBase64) {
    //The memory pointed to by `out` points to valid memory of at least `encoded_size(len)` bytes.
    const std::size_t encodedMinimumSize = boost::beast::detail::base64::encoded_size(binaryMessage.size());
    //return The number of characters written to `out`. This will exclude any null termination.
    //The resulting string will not be null terminated.
    std::vector<uint8_t> dest(encodedMinimumSize + 5);
    const std::size_t b64StringEncodedSizeNoNullTermination = boost::beast::detail::base64::encode(dest.data(), binaryMessage.data(), binaryMessage.size());
    strBase64 = std::string(dest.data(), dest.data() + b64StringEncodedSizeNoNullTermination);
}
#endif //#if (BOOST_VERSION >= 106600)

void BinaryConversions::BytesToHexString(const std::vector<uint8_t> & bytes, std::string & hexString) {
    hexString.resize(0);
    hexString.reserve((bytes.size() * 2) + 2);
    boost::algorithm::hex(bytes, std::back_inserter(hexString));
}


bool BinaryConversions::HexStringToBytes(const std::string & hexString, std::vector<uint8_t> & bytes) {
    bytes.resize(0);
    bytes.reserve(hexString.size() / 2);
    try {
        boost::algorithm::unhex(hexString, std::back_inserter(bytes));
    }
    catch (const boost::algorithm::hex_decode_error &) {
        //std::cout << "decode error: " << e.what() << std::endl;
        return false;
    }
    catch (const std::exception & e) {
        std::cout << "unknown decode error: " << e.what() << std::endl;
        return false;
    }
    return true;
}

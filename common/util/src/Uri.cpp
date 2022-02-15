#include "Uri.h"
#include <boost/format.hpp>
#include <boost/tokenizer.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/multiprecision/cpp_int.hpp>
#include <boost/multiprecision/detail/bitscan.hpp>

std::string Uri::GetIpnUriString(const uint64_t eidNodeNumber, const uint64_t eidServiceNumber) {
    static const boost::format fmtTemplate("ipn:%d.%d");
    boost::format fmt(fmtTemplate);
    fmt % eidNodeNumber % eidServiceNumber;
    return fmt.str();
}
std::string Uri::GetIpnUriStringAnyServiceNumber(const uint64_t eidNodeNumber) {
    static const boost::format fmtTemplate("ipn:%d.*");
    boost::format fmt(fmtTemplate);
    fmt % eidNodeNumber;
    return fmt.str();
}

bool Uri::ParseIpnUriString(const std::string & uri, uint64_t & eidNodeNumber, uint64_t & eidServiceNumber) {
    if ((uri.length() < 7) || (uri[0] != 'i') || (uri[1] != 'p') || (uri[2] != 'n') || (uri[3] != ':')) {
        return false;
    }
    //return ParseIpnSspString(uri.cbegin() + 4, uri.cend(), eidNodeNumber, eidServiceNumber);
    return ParseIpnSspString(uri.data() + 4, uri.length() - 4, eidNodeNumber, eidServiceNumber); //more efficient (no string copies)
}
//parse just the scheme specific part
bool Uri::ParseIpnSspString(std::string::const_iterator stringSspCbegin, std::string::const_iterator stringSspCend, uint64_t & eidNodeNumber, uint64_t & eidServiceNumber) {
    typedef boost::tokenizer<boost::char_separator<char> > tokenizer;
    static const boost::char_separator<char> sep(".", "", boost::keep_empty_tokens);
    tokenizer tokens(stringSspCbegin, stringSspCend, sep);
    tokenizer::const_iterator it = tokens.begin();
    if (it == tokens.end()) {
        return false;
    }
    try {
        eidNodeNumber = boost::lexical_cast<uint64_t>(*it);
    }
    catch (boost::bad_lexical_cast &) {
        return false;
    }
    ++it;
    if (it == tokens.end()) {
        return false;
    }
    try {
        eidServiceNumber = boost::lexical_cast<uint64_t>(*it);
    }
    catch (boost::bad_lexical_cast &) {
        return false;
    }
    ++it;
    return (it == tokens.end()); //should be end at this point
}

bool Uri::ParseIpnSspString(const char * data, std::size_t length, uint64_t & eidNodeNumber, uint64_t & eidServiceNumber) {
    const char * dotPosition = NULL;
    for (std::size_t i = 0; i < length; ++i) {
        if (data[i] == '.') {
            if (dotPosition == NULL) {
                dotPosition = &data[i];
            }
            else {
                return false; //double dots
            }
        }
    }
    if (dotPosition == NULL) {
        return false; //dot not found
    }
    const char * const lastChar = &data[length-1];
    const std::size_t sizeStr1 = static_cast<std::size_t>(dotPosition - data);
    const std::size_t sizeStr2 = static_cast<std::size_t>(lastChar - dotPosition);
    if (sizeStr1 && sizeStr2) {
        try {
            eidNodeNumber = boost::lexical_cast<uint64_t>(data, sizeStr1);
            eidServiceNumber = boost::lexical_cast<uint64_t>(dotPosition + 1, sizeStr2);
        }
        catch (boost::bad_lexical_cast &) {
            return false;
        }
        return true;
    }
    else {
        return false;
    }
}

static const uint8_t bitScanIndexMinValStrLengths[64] = {
    1, //1 << 0
    1, //1 << 1
    1, //1 << 2
    1, //1 << 3
    2, //1 << 4
    2, //1 << 5
    2, //1 << 6
    3, //1 << 7
    3, //1 << 8
    3, //1 << 9
    4, //1 << 10
    4, //1 << 11
    4, //1 << 12
    4, //1 << 13
    5, //1 << 14
    5, //1 << 15
    5, //1 << 16
    6, //1 << 17
    6, //1 << 18
    6, //1 << 19
    7, //1 << 20
    7, //1 << 21
    7, //1 << 22
    7, //1 << 23
    8, //1 << 24
    8, //1 << 25
    8, //1 << 26
    9, //1 << 27
    9, //1 << 28
    9, //1 << 29
    10, //1 << 30
    10, //1 << 31
    10, //1 << 32
    10, //1 << 33
    11, //1 << 34
    11, //1 << 35
    11, //1 << 36
    12, //1 << 37
    12, //1 << 38
    12, //1 << 39
    13, //1 << 40
    13, //1 << 41
    13, //1 << 42
    13, //1 << 43
    14, //1 << 44
    14, //1 << 45
    14, //1 << 46
    15, //1 << 47
    15, //1 << 48
    15, //1 << 49
    16, //1 << 50
    16, //1 << 51
    16, //1 << 52
    16, //1 << 53
    17, //1 << 54
    17, //1 << 55
    17, //1 << 56
    18, //1 << 57
    18, //1 << 58
    18, //1 << 59
    19, //1 << 60
    19, //1 << 61
    19, //1 << 62
    19, //1 << 63
};
static const uint64_t tenToThe[20] = {
    1ui64,
    10ui64,
    100ui64,
    1000ui64,
    10000ui64,
    100000ui64,
    1000000ui64,
    10000000ui64,
    100000000ui64,
    1000000000ui64,
    10000000000ui64,
    100000000000ui64,
    1000000000000ui64,
    10000000000000ui64,
    100000000000000ui64,
    1000000000000000ui64,
    10000000000000000ui64,
    100000000000000000ui64,
    1000000000000000000ui64,
    10000000000000000000ui64
};

uint64_t Uri::GetStringLengthOfUint(const uint64_t val) {
    //idea from https://stackoverflow.com/questions/25892665/performance-of-log10-function-returning-an-int

    //bitscan seems to have undefined behavior on a value of zero
    const unsigned int msb = boost::multiprecision::detail::find_msb<uint64_t>(val + (val == 0));

    const uint64_t lowGuessStringLength = bitScanIndexMinValStrLengths[msb]; //based on base 2
    return lowGuessStringLength + (val >= tenToThe[lowGuessStringLength]);
}

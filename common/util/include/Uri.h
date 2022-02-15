#ifndef URI_H
#define URI_H 1

#include <string>
#include <cstdint>

class Uri {
private:
    Uri();
    ~Uri();

public:
    static std::string GetIpnUriString(const uint64_t eidNodeNumber, const uint64_t eidServiceNumber);
    static std::string GetIpnUriStringAnyServiceNumber(const uint64_t eidNodeNumber);
    static bool ParseIpnUriString(const std::string & uri, uint64_t & eidNodeNumber, uint64_t & eidServiceNumber);

    //parse just the scheme specific part
    static bool ParseIpnSspString(std::string::const_iterator stringSspCbegin, std::string::const_iterator stringSspCend, uint64_t & eidNodeNumber, uint64_t & eidServiceNumber);
    static bool ParseIpnSspString(const char * data, std::size_t length, uint64_t & eidNodeNumber, uint64_t & eidServiceNumber); //more efficient (no string copies)

    //more efficient method of boost::lexical_cast<std::string>(val).length();
    static uint64_t GetStringLengthOfUint(const uint64_t val);
};

#endif //URI_H


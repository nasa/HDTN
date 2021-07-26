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
    static bool ParseIpnUriString(const std::string & uri, uint64_t & eidNodeNumber, uint64_t & eidServiceNumber);
};

#endif //URI_H


#ifndef LTP_RANDOM_NUMBER_GENERATOR_H
#define LTP_RANDOM_NUMBER_GENERATOR_H 1

#include <boost/integer.hpp>
#include <boost/random.hpp>
#include <boost/random/random_device.hpp>
#include <set>
#include "ltp_lib_export.h"

class LTP_LIB_EXPORT LtpRandomNumberGenerator {
public:
    LtpRandomNumberGenerator();
    uint64_t GetRandomSession64(const uint64_t additionalRandomness = 0);
    uint64_t GetRandomSession64(boost::random_device & randomDevice);
    uint64_t GetRandomSerialNumber64(const uint64_t additionalRandomness = 0) const;
    uint64_t GetRandomSerialNumber64(boost::random_device & randomDevice) const;
    uint32_t GetRandomSession32(const uint32_t additionalRandomness32Bit = 0);
    uint32_t GetRandomSession32(boost::random_device & randomDevice);
    uint32_t GetRandomSerialNumber32(const uint32_t additionalRandomness32Bit = 0) const;
    uint32_t GetRandomSerialNumber32(boost::random_device & randomDevice) const;
    void SetEngineIndex(const uint8_t engineIndex); //must be between 1 and 255 inclusive
    uint8_t GetEngineIndex() const;
    static uint8_t GetEngineIndexFromRandomSessionNumber(uint64_t randomSessionNumber);
private:
    uint16_t m_birthdayParadoxPreventer_incrementalPart_U16;
    uint8_t m_engineIndex; //to encode into the upper portion of a session number
};

#endif // LTP_RANDOM_NUMBER_GENERATOR_H


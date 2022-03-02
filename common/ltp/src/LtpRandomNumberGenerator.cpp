#include "LtpRandomNumberGenerator.h"
#include <iostream>
#include "TimestampUtil.h"
#ifdef LTP_RNG_USE_RDSEED
#include <immintrin.h>
#if defined(__GNUC__)
#include <x86intrin.h> // rdseed for older compilers
#endif
#endif
#include <inttypes.h>

LtpRandomNumberGenerator::LtpRandomNumberGenerator() : m_birthdayParadoxPreventer_incrementalPart_U16(1) {

}


//return a hardware random generated number with:
//    - bit 63..56 (8 bits of engineIndex)
//    - bit 55 set to 0 to leave room for incrementing without rolling into the engineIndex
//    - bit 54..16 a random part (39 bits of randomness)
//    - bits 15..0 a 16-bit incremental part (never 0) (to prevent a birthday paradox) which rolls around (1,2,..,65534,65535,1,2,..)
uint64_t LtpRandomNumberGenerator::GetRandomSession64(const uint64_t additionalRandomness) {
    const uint64_t random1 = TimestampUtil::GetMicrosecondsSinceEpochRfc5050() << 16;
    uint64_t random2 = 0;
#ifdef LTP_RNG_USE_RDSEED
    if (!_rdseed64_step((unsigned long long *)&random2)) {
        std::cerr << "NOTICE: in LtpRandomNumberGenerator::GetRandom(): cannot use _rdseed64_step function" << std::endl;
    }
#endif
    uint64_t randomNumber = (random1 ^ random2) ^ additionalRandomness;
    randomNumber &= (static_cast<uint64_t>(0x007fffffffff0000u));
    randomNumber |= ((static_cast<uint64_t>(m_engineIndex)) << 56);
    //randomNumber >>= 16; //zero out 16 lsb for OR'ing with the incremental part 
    //randomNumber <<= (16+1);
    //randomNumber >>= 1; // clear out 1 msb to make room for incrementing these random numbers so that they will never roll back around to zero
    uint16_t inc = m_birthdayParadoxPreventer_incrementalPart_U16;

    randomNumber |= inc;
    ++inc;
    inc += (inc == 0); //must be non-zero when uint16 rolls back around
    m_birthdayParadoxPreventer_incrementalPart_U16 = inc;
    //printf("random1: 0x%016" PRIx64 "\nrandom2: 0x%016" PRIx64 " \nrandom3: 0x%016" PRIx64 "\nrandomF: 0x%016" PRIx64 "\n", random1, random2, additionalRandomness, randomNumber);
    return randomNumber;

}
uint64_t LtpRandomNumberGenerator::GetRandomSession64(boost::random_device & randomDevice) {
    uint64_t additionalRandomness = (static_cast<uint32_t>(randomDevice())); //should already return a 32 bit integer
    additionalRandomness <<= 32;
    additionalRandomness |= (static_cast<uint32_t>(randomDevice()));
    return GetRandomSession64(additionalRandomness);
}

//return a hardware random generated number with:
//    - bit 63 set to 0 to leave room for incrementing without rolling back around to zero
//    - bit 62..16 a random part (47 bits of randomness)
//    - bits 15..0 set to 1 for incrementing ltp serial numbers by 1 (never want a serial number to be 0)
uint64_t LtpRandomNumberGenerator::GetRandomSerialNumber64(const uint64_t additionalRandomness) const {
    const uint64_t random1 = TimestampUtil::GetMicrosecondsSinceEpochRfc5050() << 16;
    uint64_t random2 = 0;
#ifdef LTP_RNG_USE_RDSEED
    if (!_rdseed64_step((unsigned long long *)&random2)) {
        std::cerr << "NOTICE: in LtpRandomNumberGenerator::GetRandom(): cannot use _rdseed64_step function" << std::endl;
    }
#endif
    uint64_t randomNumber = (random1 ^ random2) ^ additionalRandomness;
    randomNumber &= (static_cast<uint64_t>(0x7fffffffffff0000u));
    randomNumber |= 1u;
    
    return randomNumber;
}
uint64_t LtpRandomNumberGenerator::GetRandomSerialNumber64(boost::random_device & randomDevice) const {
    uint64_t additionalRandomness = (static_cast<uint32_t>(randomDevice())); //should already return a 32 bit integer
    additionalRandomness <<= 32;
    additionalRandomness |= (static_cast<uint32_t>(randomDevice()));
    return GetRandomSerialNumber64(additionalRandomness);
}


//return a hardware random generated number with:
//    - bit 31..24 (8 bits of engineIndex)
//    - bit 23 set to 0 to leave room for incrementing without rolling into the engineIndex
//    - bit 22..16 a random part (7 bits of randomness)
//    - bits 15..0 a 16-bit incremental part (never 0) (to prevent a birthday paradox) which rolls around (1,2,..,65534,65535,1,2,..)
uint32_t LtpRandomNumberGenerator::GetRandomSession32(const uint32_t additionalRandomness32Bit) {
    const uint64_t random1 = TimestampUtil::GetMicrosecondsSinceEpochRfc5050() << 16;
    uint64_t random2 = 0;
#ifdef LTP_RNG_USE_RDSEED
    if (!_rdseed64_step((unsigned long long *)&random2)) {
        std::cerr << "NOTICE: in LtpRandomNumberGenerator::GetRandom(): cannot use _rdseed64_step function" << std::endl;
    }
#endif
    uint64_t additionalRandomness = additionalRandomness32Bit;
    additionalRandomness <<= 16;
    uint64_t randomNumber = (random1 ^ random2) ^ additionalRandomness;
    randomNumber &= (static_cast<uint64_t>(0x007f0000u));
    randomNumber |= ((static_cast<uint64_t>(m_engineIndex)) << 24);
    //randomNumber >>= 16; //zero out 16 lsb for OR'ing with the incremental part 
    //randomNumber <<= (16+1);
    //randomNumber >>= 1; // clear out 1 msb to make room for incrementing these random numbers so that they will never roll back around to zero
    uint16_t inc = m_birthdayParadoxPreventer_incrementalPart_U16;

    randomNumber |= inc;
    ++inc;
    inc += (inc == 0); //must be non-zero when uint16 rolls back around
    m_birthdayParadoxPreventer_incrementalPart_U16 = inc;
    //printf("random1: 0x%016" PRIx64 "\nrandom2: 0x%016" PRIx64 " \nrandom3: 0x%016" PRIx64 "\nrandomF: 0x%016" PRIx64 "\n", random1, random2, additionalRandomness, randomNumber);
    return static_cast<uint32_t>(randomNumber);
}
uint32_t LtpRandomNumberGenerator::GetRandomSession32(boost::random_device & randomDevice) {
    return GetRandomSession32(static_cast<uint32_t>(randomDevice())); //should already return a 32 bit integer
}

//return a hardware random generated number with:
//    - bit 31 set to 0 to leave room for incrementing without rolling back around to zero
//    - bit 30..16 a random part (15 bits of randomness)
//    - bits 15..0 set to 1 for incrementing ltp serial numbers by 1 (never want a serial number to be 0)
uint32_t LtpRandomNumberGenerator::GetRandomSerialNumber32(const uint32_t additionalRandomness32Bit) const {
    const uint64_t random1 = TimestampUtil::GetMicrosecondsSinceEpochRfc5050() << 16;
    uint64_t random2 = 0;
#ifdef LTP_RNG_USE_RDSEED
    if (!_rdseed64_step((unsigned long long *)&random2)) {
        std::cerr << "NOTICE: in LtpRandomNumberGenerator::GetRandom(): cannot use _rdseed64_step function" << std::endl;
    }
#endif
    const uint64_t additionalRandomness = additionalRandomness32Bit;
    uint64_t randomNumber = (random1 ^ random2) ^ additionalRandomness;
    randomNumber &= (static_cast<uint64_t>(0x7fff0000u));
    randomNumber |= 1u;
    
    return static_cast<uint32_t>(randomNumber);
}
uint32_t LtpRandomNumberGenerator::GetRandomSerialNumber32(boost::random_device & randomDevice) const {
    return GetRandomSerialNumber32(static_cast<uint32_t>(randomDevice())); //should already return a 32 bit integer
}

void LtpRandomNumberGenerator::SetEngineIndex(const uint8_t engineIndex) {
    m_engineIndex = engineIndex;
}
uint8_t LtpRandomNumberGenerator::GetEngineIndex() const {
    return m_engineIndex;
}
uint8_t LtpRandomNumberGenerator::GetEngineIndexFromRandomSessionNumber(uint64_t randomSessionNumber) {
    const uint8_t engineIndexIf64BitSessionNumber = static_cast<uint8_t>(randomSessionNumber >> 56);
    const bool is32BitSessionNumber = (engineIndexIf64BitSessionNumber == 0);
    const uint8_t engineIndexIf32BitSessionNumber = static_cast<uint8_t>(randomSessionNumber >> 24);
    return (engineIndexIf32BitSessionNumber * is32BitSessionNumber) + engineIndexIf64BitSessionNumber;
}

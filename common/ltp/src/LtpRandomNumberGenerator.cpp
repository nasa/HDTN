/**
 * @file LtpRandomNumberGenerator.cpp
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
 */

#include "LtpRandomNumberGenerator.h"
#include "Logger.h"
#include "TimestampUtil.h"
#ifdef LTP_RNG_USE_RDSEED
#include <immintrin.h>
#if defined(__GNUC__)
#include <x86intrin.h> // rdseed for older compilers
#endif
#endif
#include <inttypes.h>

static constexpr hdtn::Logger::SubProcess subprocess = hdtn::Logger::SubProcess::none;

//Motivation: Use a PRNG reseeded with hardware for efficiency.  Reseed the PRNG often enough to prevent predictability.
//Relative speed tests, higher is slower
//
//mt19937 (32bit) never reseed = 1.812500sec
//mt19937_64 (64bit) = 2.250000sec
//mt19937_64 re-seed every 256 times = 5.625000sec
//mt19937_64 re-seed every time = 1010.00sec
//mt19937_64 discard every 10 before obtaining = 16.828125sec
//openssl RAND_bytes = 1135.00sec
//random_device = 52.00sec
//rdseed = 984.00sec
//rdrand = 975.00sec
//
//our pick's relative speed: (mt19937_64 re-seed every 256 times) + (random_device + rdseed)/256
LtpRandomNumberGenerator::Rng::Rng() :
    m_prng(GetHardwareRandomSeed()),
    m_additionalEntropy(GetHardwareRandomSeed()),
    m_reseedPrngCount(0),
    m_reseedAdditionalEntropyCount(0),
    m_prngUseCounter(0)
{

}
uint64_t LtpRandomNumberGenerator::Rng::operator()() {
    ++m_prngUseCounter;
    if (m_prngUseCounter == 0) { 
        //reseed PRNG with hardware RNG every ~UINT8_MAX (256) times < mt19937_64's n=312 to prevent predictability
        //note: all boost::mt19937 implementations within the cmake boost minimum version have the mt19937 initialization fix 
        m_prng.seed(GetHardwareRandomSeed());
        ++m_reseedPrngCount;
    }
    else if (m_prngUseCounter == 127) {
        //reseed additional entropy with hardware RNG every ~UINT8_MAX (256) times to prevent predictability
        AddHardwareEntropy();
        ++m_reseedAdditionalEntropyCount;
    }
    const uint64_t pseudoRandomNumber = m_prng();
    return pseudoRandomNumber ^ m_additionalEntropy;
}
void LtpRandomNumberGenerator::Rng::AddHardwareEntropy() {
    m_additionalEntropy ^= GetHardwareRandomSeed();
}
void LtpRandomNumberGenerator::Rng::AddCustomEntropy(const uint64_t entropy) {
    m_additionalEntropy ^= entropy;
}
uint64_t LtpRandomNumberGenerator::Rng::GetHardwareRandomSeed() {
    const uint64_t random1 = TimestampUtil::GetMicrosecondsSinceEpochRfc5050();
    uint64_t random2 = 0;
#ifdef LTP_RNG_USE_RDSEED
    if (!_rdseed64_step((unsigned long long*)&random2)) {
        LOG_ERROR(subprocess) << "LtpRandomNumberGenerator::Rng::GetHardwareRandomSeed(): cannot use _rdseed64_step function";
    }
#endif
    uint64_t random3 = (static_cast<uint32_t>(m_randomDevice())); //should already return a 32 bit integer
    random3 <<= 32;
    random3 |= (static_cast<uint32_t>(m_randomDevice()));
    const uint64_t randomSeed = (random1 ^ random2) ^ random3;
    return randomSeed;
}
uint64_t LtpRandomNumberGenerator::Rng::GetReseedPrngCount() const noexcept {
    return m_reseedPrngCount;
}
uint64_t LtpRandomNumberGenerator::Rng::GetReseedAdditionalEntropyCount() const noexcept {
    return m_reseedAdditionalEntropyCount;
}

LtpRandomNumberGenerator::LtpRandomNumberGenerator() : m_birthdayParadoxPreventer_incrementalPart_U16(1) {

}


//return a hardware random generated number with:
//    - bit 63..56 (8 bits of engineIndex)
//    - bit 55 set to 0 to leave room for incrementing without rolling into the engineIndex
//    - bit 54..16 a random part (39 bits of randomness)
//    - bits 15..0 a 16-bit incremental part (never 0) (to prevent a birthday paradox) which rolls around (1,2,..,65534,65535,1,2,..)
uint64_t LtpRandomNumberGenerator::GetRandomSession64() {
    uint64_t randomNumber = m_rng() << 16; //only shifting 16 so that microsecond time LSB (one of the hardware random components) are fully included in the random part
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
    return randomNumber;

}

//return a ping number with:
//    - bit 63..56 (8 bits of engineIndex)
//    - bit 55..0 set to 0xffffff for reserved number denoting ping
uint64_t LtpRandomNumberGenerator::GetPingSession64() const {
    static constexpr uint64_t pingReserved = 0xffffffffffffffu;
    const uint64_t randomNumber = pingReserved | ((static_cast<uint64_t>(m_engineIndex)) << 56);
    return randomNumber;
}

//return a hardware random generated number with:
//    - bit 63 set to 0 to leave room for incrementing without rolling back around to zero
//    - bit 62..16 a random part (47 bits of randomness)
//    - bits 15..0 set to 1 for incrementing ltp serial numbers by 1 (never want a serial number to be 0)
uint64_t LtpRandomNumberGenerator::GetRandomSerialNumber64() {
    uint64_t randomNumber = m_rng() << 16; //only shifting 16 so that microsecond time LSB (one of the hardware random components) are fully included in the random part
    randomNumber &= (static_cast<uint64_t>(0x7fffffffffff0000u));
    randomNumber |= 1u;
    
    return randomNumber;
}


//return a hardware random generated number with:
//    - bit 31..24 (8 bits of engineIndex)
//    - bit 23 set to 0 to leave room for incrementing without rolling into the engineIndex
//    - bit 22..16 a random part (7 bits of randomness)
//    - bits 15..0 a 16-bit incremental part (never 0) (to prevent a birthday paradox) which rolls around (1,2,..,65534,65535,1,2,..)
uint32_t LtpRandomNumberGenerator::GetRandomSession32() {
    uint64_t randomNumber = m_rng() << 16; //only shifting 16 so that microsecond time LSB (one of the hardware random components) are fully included in the random part
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
    return static_cast<uint32_t>(randomNumber);
}


//return a ping number with:
//    - bit 31..24 (8 bits of engineIndex)
//    - bit 23..0 set to 0xffffff for reserved number denoting ping
uint32_t LtpRandomNumberGenerator::GetPingSession32() const {
    static constexpr uint64_t pingReserved = 0x00ffffffu;
    const uint64_t randomNumber = pingReserved | ((static_cast<uint64_t>(m_engineIndex)) << 24);
    return static_cast<uint32_t>(randomNumber);
}

//return a hardware random generated number with:
//    - bit 31 set to 0 to leave room for incrementing without rolling back around to zero
//    - bit 30..16 a random part (15 bits of randomness)
//    - bits 15..0 set to 1 for incrementing ltp serial numbers by 1 (never want a serial number to be 0)
uint32_t LtpRandomNumberGenerator::GetRandomSerialNumber32() {
    uint64_t randomNumber = m_rng() << 16; //only shifting 16 so that microsecond time LSB (one of the hardware random components) are fully included in the random part
    randomNumber &= (static_cast<uint64_t>(0x7fff0000u));
    randomNumber |= 1u;
    
    return static_cast<uint32_t>(randomNumber);
}

void LtpRandomNumberGenerator::SetEngineIndex(const uint8_t engineIndex) {
    m_engineIndex = engineIndex;
}
uint8_t LtpRandomNumberGenerator::GetEngineIndex() const {
    return m_engineIndex;
}
LtpRandomNumberGenerator::Rng& LtpRandomNumberGenerator::GetInternalRngRef() {
    return m_rng;
}
uint8_t LtpRandomNumberGenerator::GetEngineIndexFromRandomSessionNumber(uint64_t randomSessionNumber) {
    const uint8_t engineIndexIf64BitSessionNumber = static_cast<uint8_t>(randomSessionNumber >> 56);
    const bool is32BitSessionNumber = (engineIndexIf64BitSessionNumber == 0);
    const uint8_t engineIndexIf32BitSessionNumber = static_cast<uint8_t>(randomSessionNumber >> 24);
    return (engineIndexIf32BitSessionNumber * is32BitSessionNumber) + engineIndexIf64BitSessionNumber;
}
bool LtpRandomNumberGenerator::IsPingSession(const uint64_t sessionNumber, const bool is32Bit) {
    static constexpr uint64_t pingReserved32 = 0x00ffffffu;
    static constexpr uint64_t pingReserved64 = 0xffffffffffffffu;
    if (is32Bit) {
        return ((pingReserved32 & sessionNumber) == pingReserved32);
    }
    else {
        return ((pingReserved64 & sessionNumber) == pingReserved64);
    }
}

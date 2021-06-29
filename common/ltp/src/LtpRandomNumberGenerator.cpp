#include "LtpRandomNumberGenerator.h"
#include <iostream>
#include "TimestampUtil.h"
#include <immintrin.h>
#if defined(__GNUC__)
#include <x86intrin.h> // rdseed for older compilers
#endif
#include <inttypes.h>

LtpRandomNumberGenerator::LtpRandomNumberGenerator() : m_birthdayParadoxPreventer_incrementalPart_U16(1) {

}

//return a hardware random generated number with:
//    - bit 63 set to 0 to leave room for incrementing without rolling back around to zero
//    - bit 62..32 a random part (31 bits of randomness)
//    - bits 31..16 a 16-bit incremental part (never 0) (to prevent a birthday paradox) which rolls around (1,2,..,65534,65535,1,2,..)
//    - bits 15..0 set to 0 for incrementing ltp serial numbers by 1
uint64_t LtpRandomNumberGenerator::GetRandom(const uint32_t additionalRandomness32Bit) {
    const uint64_t random1 = TimestampUtil::GetMicrosecondsSinceEpochRfc5050() << 32;
    uint64_t random2 = 0;
#ifdef LTP_RNG_USE_RDSEED
    if (!_rdseed64_step((unsigned long long *)&random2)) {
        std::cerr << "NOTICE: in LtpRandomNumberGenerator::GetRandom(): cannot use _rdseed64_step function" << std::endl;
    }
#endif
    uint64_t additionalRandomness = additionalRandomness32Bit;
    additionalRandomness <<= 32;
    uint64_t randomNumber = (random1 ^ random2) ^ additionalRandomness;
    randomNumber &= (static_cast<uint64_t>(0x7fffffff00000000u));
    //randomNumber >>= 16; //zero out 16 lsb for OR'ing with the incremental part 
    //randomNumber <<= (16+1);
    //randomNumber >>= 1; // clear out 1 msb to make room for incrementing these random numbers so that they will never roll back around to zero
    uint16_t inc = m_birthdayParadoxPreventer_incrementalPart_U16;
    
    randomNumber |= ((static_cast<uint64_t>(inc)) << 16);
    ++inc;
    inc += (inc == 0); //must be non-zero when uint16 rolls back around
    m_birthdayParadoxPreventer_incrementalPart_U16 = inc;
    //printf("random1: 0x%016" PRIx64 "\nrandom2: 0x%016" PRIx64 " \nrandom3: 0x%016" PRIx64 "\nrandomF: 0x%016" PRIx64 "\n", random1, random2, additionalRandomness, randomNumber);
    return randomNumber;
}

uint64_t LtpRandomNumberGenerator::GetRandom(boost::random_device & randomDevice) {
    return GetRandom(static_cast<uint32_t>(randomDevice())); //should already return a 32 bit integer
}

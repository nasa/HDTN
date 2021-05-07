#include "LtpRandomNumberGenerator.h"
#include <iostream>
#include "TimestampUtil.h"
#include <immintrin.h>
#include <inttypes.h>

LtpRandomNumberGenerator::LtpRandomNumberGenerator() : m_birthdayParadoxPreventer_incrementalPart_U16(1) {

}

//return a hardware random generated number with the 16 lsb an incremental part (to prevent a birthday paradox) and the rest a random part
//(bit 63 shall be 0 to leave room for incrementing without rolling back around to zero)
uint64_t LtpRandomNumberGenerator::GetRandom(const uint64_t additionalRandomness) {
    const uint64_t random1 = TimestampUtil::GetMicrosecondsSinceEpochRfc5050() << 16;
    uint64_t random2 = 0;
    if (!_rdseed64_step(&random2)) {
        std::cerr << "NOTICE: in LtpRandomNumberGenerator::GetRandom(): cannot use _rdseed64_step function" << std::endl;
    }
    uint64_t randomNumber = (random1 ^ random2) ^ additionalRandomness;
    randomNumber &= (static_cast<uint64_t>(0x7fffffffffff0000u));
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

uint64_t LtpRandomNumberGenerator::GetRandom(boost::random_device & randomDevice) {
    uint64_t additionalRandomness = (static_cast<uint32_t>(randomDevice())); //should already return a 32 bit integer
    additionalRandomness <<= 32;
    additionalRandomness |= (static_cast<uint32_t>(randomDevice()));
    return GetRandom(additionalRandomness);
}

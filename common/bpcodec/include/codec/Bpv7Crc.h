#ifndef BPV7_CRC_H
#define BPV7_CRC_H
#include <cstdint>
#include <cstddef>
class Bpv7Crc {
private:
    Bpv7Crc();
    ~Bpv7Crc();

public:
#ifdef USE_X86_HARDWARE_ACCELERATION
    static uint32_t Crc32C_Unaligned_Hardware(const uint8_t* dataUnaligned, std::size_t length);
#endif
    static uint32_t Crc32C_Unaligned_Software(const uint8_t* dataUnaligned, std::size_t length);
    static uint32_t Crc32C_Unaligned(const uint8_t* dataUnaligned, std::size_t length);
    static uint16_t Crc16_X25_Unaligned(const uint8_t* dataUnaligned, std::size_t length);
};
#endif //BPV7_CRC_H

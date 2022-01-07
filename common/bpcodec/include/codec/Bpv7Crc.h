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

    static uint64_t SerializeCrc16ForBpv7(uint8_t * serialization, const uint16_t crc16);
    static uint64_t SerializeCrc32ForBpv7(uint8_t * serialization, const uint32_t crc32);
    static uint64_t SerializeZeroedCrc16ForBpv7(uint8_t * serialization);
    static uint64_t SerializeZeroedCrc32ForBpv7(uint8_t * serialization);
    static bool DeserializeCrc16ForBpv7(const uint8_t * serialization, uint8_t * numBytesTakenToDecode, uint16_t & crc16);
    static bool DeserializeCrc32ForBpv7(const uint8_t * serialization, uint8_t * numBytesTakenToDecode, uint32_t & crc32);
};
#endif //BPV7_CRC_H

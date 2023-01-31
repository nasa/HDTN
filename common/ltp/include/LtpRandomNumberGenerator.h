/**
 * @file LtpRandomNumberGenerator.h
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
 *
 * @section DESCRIPTION
 *
 * This LtpRandomNumberGenerator class defines methods for generating
 * either 32 or 64 bit random numbers for LTP session numbers
 * or LTP serial numbers using up to 3 sources of hardware randomness and birthday paradox prevention.
 * These sources of randomness include microsecond time, boost::random_device,
 * and optional x86 RDSEED instruction.
 */

#ifndef LTP_RANDOM_NUMBER_GENERATOR_H
#define LTP_RANDOM_NUMBER_GENERATOR_H 1

#include <boost/integer.hpp>
#include <boost/random.hpp>
#include <boost/random/random_device.hpp>
#include <set>
#include "ltp_lib_export.h"

class LTP_LIB_EXPORT LtpRandomNumberGenerator {
public:
    /// Start birthday paradox prevention value from 1.
    LtpRandomNumberGenerator();
    
    /** Generate a hardware-generated random 64-bit session number.
     *
     * - bits 63..56 (8 bits): Engine index.
     * - bit  55     (1 bit): Set to 0 to leave room for incrementing without rolling into the engine index.
     * - bits 54..16 (39 bits): Random part.
     * - bits 15..0  (16 bits): Birthday paradox prevention part, stays in range [1, std::numeric_limits<std::uint16_t>::max()].
     * @param additionalRandomness The final random component to XOR.
     * @return The random 64-bit session number.
     */
    uint64_t GetRandomSession64(const uint64_t additionalRandomness = 0);
    
    /** Generate a random 64-bit session number using given random device.
     *
     * Calls LtpRandomNumberGenerator::GetRandomSession64(uint64_t) with the final random component supplied by randomDevice.
     * @param randomDevice The random device to use.
     * @return The random 64-bit session number.
     */
    uint64_t GetRandomSession64(boost::random_device & randomDevice);
    
    /** Get 64-bit ping session number.
     *
     * - bits 63..56 (8 bits): Engine index.
     * - bits 55..0  (56 bits): Set to 0xffffff for reserved number denoting ping.
     * @return The 64-bit ping session number.
     */
    uint64_t GetPingSession64() const;
    
    /** Generate a hardware-generated random 64-bit serial number.
     *
     * - bit 63      (1 bit): Set to 0 to leave room for incrementing without rolling back around to zero.
     * - bits 62..16 (47 bits): Random part.
     * - bits 15..0  (16 bits): Set to 1 for incrementing ltp serial numbers by 1 (never want a serial number to be 0).
     * @param additionalRandomness The final random component to XOR.
     * @return The random 64-bit serial number.
     */
    uint64_t GetRandomSerialNumber64(const uint64_t additionalRandomness = 0) const;
    
    /** Generate a random 64-bit serial number using given random device.
     *
     * Calls LtpRandomNumberGenerator::GetRandomSerialNumber64(uint64_t) with the final random component supplied by randomDevice.
     * @param randomDevice The random device to use.
     * @return The random 64-bit serial number.
     */
    uint64_t GetRandomSerialNumber64(boost::random_device & randomDevice) const;
    
    /** Generate a hardware-generated random 32-bit session number.
     *
     * - bits 31..24 (8 bits): Engine index.
     * - bit 23      (1 bit): Set to 0 to leave room for incrementing without rolling into the engine index.
     * - bits 22..16 (7 bits): Random part.
     * - bits 15..0  (16 bits): Birthday paradox prevention part, stays in range [1, std::numeric_limits<std::uint16_t>::max()].
     * @param additionalRandomness32Bit The final random component to XOR.
     * @return The random 32-bit session number.
     */
    uint32_t GetRandomSession32(const uint32_t additionalRandomness32Bit = 0);
    
    /** Generate a random 32-bit session number using given random device.
     *
     * Calls LtpRandomNumberGenerator::GetRandomSession32(uint32_t) with the final random component supplied by randomDevice.
     * @param randomDevice The random device to use.
     * @return The random 32-bit session number.
     */
    uint32_t GetRandomSession32(boost::random_device & randomDevice);
    
    /** Get 32-bit ping session number.
     *
     * - bits 31..24 (8 bits): Engine index.
     * - bits 23..0  (24 bits): Set to 0xffffff for reserved number denoting ping.
     * @return The 32-bit ping session number.
     */
    uint32_t GetPingSession32() const;
    
    /** Generate a hardware-generated random 32-bit serial number.
     *
     * - bit 31 (1 bit): Set to 0 to leave room for incrementing without rolling back around to zero.
     * - bits 30..16 (15 bits): Random part.
     * - bits 15..0 (16 bits): Set to 1 for incrementing ltp serial numbers by 1 (never want a serial number to be 0).
     * @param additionalRandomness32Bit The final random component to XOR.
     * @return The random 32-bit serial number.
     */
    uint32_t GetRandomSerialNumber32(const uint32_t additionalRandomness32Bit = 0) const;
    
    /** Generate a random 32-bit serial number using given random device.
     *
     * Calls LtpRandomNumberGenerator::GetRandomSerialNumber32(uint32_t) with the final random component supplied by randomDevice.
     * @param randomDevice The random device to use.
     * @return The random 32-bit serial number.
     */
    uint32_t GetRandomSerialNumber32(boost::random_device & randomDevice) const;
    
    /** Set the engine index.
     *
     * @param engineIndex The engine index to set, must be in range [1, 255].
     */
    void SetEngineIndex(const uint8_t engineIndex);
    
    /** Get the engine index.
     *
     * @return The engine index.
     */
    uint8_t GetEngineIndex() const;
    
    /** Parse the engine index part of a random session number.
     *
     * Assumes the number engine index resides in the top 8 bits, typically generated by LtpRandomNumberGenerator::GetRandomSession64() or the 32-bit variant.
     * @param randomSessionNumber The random session number to parse.
     * @return The engine index.
     */
    static uint8_t GetEngineIndexFromRandomSessionNumber(uint64_t randomSessionNumber);
    
    /** Query whether the given session number denotes a ping session.
     *
     * @param sessionNumber The session number to check.
     * @param is32Bit Whether the session number is 32-bit.
     * @return Whether the given session number denotes a ping session.
     */
    static bool IsPingSession(const uint64_t sessionNumber, const bool is32Bit);
private:
    /// Circular birthday paradox prevention value, stays in range [1, std::numeric_limits<std::uint16_t>::max()]
    uint16_t m_birthdayParadoxPreventer_incrementalPart_U16;
    /// Engine index, encoded into the upper portion of a session number
    uint8_t m_engineIndex;
};

#endif // LTP_RANDOM_NUMBER_GENERATOR_H


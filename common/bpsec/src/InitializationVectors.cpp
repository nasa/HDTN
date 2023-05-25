/**
 * @file InitializationVectors.cpp
 * @author  Brian Tomko <brian.j.tomko@nasa.gov>
 *
 * @copyright Copyright  2021 United States Government as represented by
 * the National Aeronautics and Space Administration.
 * No copyright is claimed in the United States under Title 17, U.S.Code.
 * All Other Rights Reserved.
 *
 * @section LICENSE
 * Released under the NASA Open Source Agreement (NOSA)
 * See LICENSE.md in the source root directory for more information.
 */

#include "InitializationVectors.h"
#include "TimestampUtil.h"
#include <boost/thread.hpp>

//4.3.1.  Initialization Vector (IV)
//
//This optional parameter identifies the initialization vector (IV)
//used to initialize the AES-GCM cipher.
//
//The length of the initialization vector, prior to any CBOR encoding,
//MUST be between 8-16 bytes.  A value of 12 bytes SHOULD be used
//unless local security policy requires a different length.
//
//This value MUST be encoded as a CBOR byte string.
//
//The initialization vector can have any value, with the caveat that a
//value MUST NOT be reused for multiple encryptions using the same
//encryption key.  This value MAY be reused when encrypting with
//different keys.  For example, if each encryption operation using BCB-
//AES-GCM uses a newly generated key, then the same IV can be reused.

InitializationVector12Byte::InitializationVector12Byte() :
    InitializationVector12Byte(TimestampUtil::GetMicrosecondsSinceEpochRfc5050()) {} //good for 584,868 years
InitializationVector12Byte::InitializationVector12Byte(const uint64_t timePart) :
    m_timePart(timePart),
    m_counterPart(0) {}
InitializationVector12Byte::~InitializationVector12Byte() {}
void InitializationVector12Byte::Serialize(void* data) const {
    uint8_t* dataU8 = reinterpret_cast<uint8_t*>(data);
    memcpy(dataU8, &m_timePart, sizeof(m_timePart));
    dataU8 += sizeof(m_timePart);
    memcpy(dataU8, &m_counterPart, sizeof(m_counterPart));
}
void InitializationVector12Byte::Increment() {
    ++m_counterPart;
    m_timePart += static_cast<bool>(m_counterPart == 0);
}


InitializationVector16Byte::InitializationVector16Byte() :
    InitializationVector16Byte(TimestampUtil::GetMicrosecondsSinceEpochRfc5050()) {} //good for 584,868 years
InitializationVector16Byte::InitializationVector16Byte(const uint64_t timePart) :
    m_timePart(timePart),
    m_counterPart(0) {}
InitializationVector16Byte::~InitializationVector16Byte() {}
void InitializationVector16Byte::Serialize(void* data) const {
    memcpy(data, &m_timePart, sizeof(m_timePart) + sizeof(m_counterPart));
}
void InitializationVector16Byte::Increment() {
    ++m_counterPart;
    m_timePart += static_cast<bool>(m_counterPart == 0);
}


InitializationVectorsForOneThread::InitializationVectorsForOneThread(const uint64_t timePart) :
    m_iv12(timePart),
    m_iv16(timePart),
    m_initializationVector(16) {}
InitializationVectorsForOneThread InitializationVectorsForOneThread::Create() {
    static const boost::posix_time::time_duration maxWait = boost::posix_time::microseconds(MIN_DIFF_MICROSECONDS);
    static boost::mutex staticMutex;
    static uint64_t staticLastTimeMicroseconds = 0;
    uint64_t currentTimeMicroseconds;
    {
        boost::mutex::scoped_lock lock(staticMutex);
        while (true) {
            currentTimeMicroseconds = TimestampUtil::GetMicrosecondsSinceEpochRfc5050();
            if (currentTimeMicroseconds > staticLastTimeMicroseconds) {
                const uint64_t diff = currentTimeMicroseconds - staticLastTimeMicroseconds;
                
                if (diff >= MIN_DIFF_MICROSECONDS) {
                    staticLastTimeMicroseconds = currentTimeMicroseconds;
                    break;
                }
                else { //diff < MIN_DIFF_MICROSECONDS
                    boost::this_thread::sleep(boost::posix_time::microseconds(MIN_DIFF_MICROSECONDS - diff));
                    continue;
                }
            }
            boost::this_thread::sleep(maxWait);
        }
    }
    return InitializationVectorsForOneThread(currentTimeMicroseconds);
}

void InitializationVectorsForOneThread::SerializeAndIncrement(const bool use12ByteIv) {
    if (use12ByteIv) {
        m_initializationVector.resize(12);
        m_iv12.Serialize(m_initializationVector.data());
        m_iv12.Increment();
    }
    else {
        m_initializationVector.resize(16);
        m_iv16.Serialize(m_initializationVector.data());
        m_iv16.Increment();
    }
}

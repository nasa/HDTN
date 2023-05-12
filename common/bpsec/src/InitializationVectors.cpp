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
    m_timePart(TimestampUtil::GetMillisecondsSinceEpochRfc5050()),
    m_counterPart(0)
{
}
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
    m_timePart(TimestampUtil::GetMillisecondsSinceEpochRfc5050()),
    m_counterPart(0)
{
}
InitializationVector16Byte::~InitializationVector16Byte() {}
void InitializationVector16Byte::Serialize(void* data) const {
    memcpy(data, &m_timePart, sizeof(m_timePart) + sizeof(m_counterPart));
}
void InitializationVector16Byte::Increment() {
    ++m_counterPart;
    m_timePart += static_cast<bool>(m_counterPart == 0);
}
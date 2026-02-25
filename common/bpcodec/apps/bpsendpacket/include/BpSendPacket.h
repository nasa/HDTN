/**
 * @file BpSendFile.h
 * @author Timothy Recker University of California Berkeley
 * @author Nadia Kortas <nadia.kortas@nasa.gov>
 *
 * @copyright Copyright (c) 2026 United States Government as represented by
 * the National Aeronautics and Space Administration.
 * No copyright is claimed in the United States under Title 17, U.S.Code.
 * All Other Rights Reserved.
 *
 * @section LICENSE
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * @section DESCRIPTION
 *
 * The BpSendPacket class is a child class of BpSourcePattern.  It is an app 
 * used for extracting payload data from a (UDP) packet, wrapping it into a 
 * bundle, and sending it. It is episodic and overrides 
 * TryWaitForDataAvailable since it monitors a socket that will not always
 * have new data.
 */

#ifndef _BP_SEND_PACKET_H
#define _BP_SEND_PACKET_H 1

#include <cstdint>
#include <queue>
#include <boost/function.hpp>
#include "app_patterns/BpSourcePattern.h"

class BpSendPacket : public BpSourcePattern {
private:
    BpSendPacket();
public:
    BpSendPacket(const uint64_t maxBundleSizeBytes);
    bool Init(InductsConfig_ptr & inductsConfigPtr, const cbhe_eid_t & myEid);
    virtual ~BpSendPacket() override;
protected:
    virtual bool TryWaitForDataAvailable(const boost::posix_time::time_duration& timeout) override;
    virtual uint64_t GetNextPayloadLength_Step1() override;
    virtual bool CopyPayload_Step2(uint8_t * destinationBuffer) override;
private:
    void ProcessPacketCallback(padded_vector_uint8_t & packet);
    void NullCallback(const uint64_t remoteNodeId, Induct* thisInductPtr, void* sinkPtr);
    InductManager m_packetInductManager;
    std::queue<padded_vector_uint8_t> m_queue;
    uint64_t m_maxBundleSizeBytes;
};
#endif //_BP_SEND_PACKET_H
    

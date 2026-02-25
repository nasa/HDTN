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
 * The BpReceivePacket class is a child class of BpSinkPattern.  It is an app 
 * used for extracting payload data from a bundle, wrapping it into a 
 * UDP packet, and sending it.
 */

#ifndef _BP_RECEIVE_PACKET_H
#define _BP_RECEIVE_PACKET_H 1

#include <cstdint>
#include <queue>
#include <boost/function.hpp>
#include "app_patterns/BpSinkPattern.h"

class BpReceivePacket : public BpSinkPattern {
public:
    BpReceivePacket();
    virtual ~BpReceivePacket() override;
    bool socketInit(OutductsConfig_ptr & outductsConfigPtr, const cbhe_eid_t & myEid, 
		    const uint64_t maxBundleSizeBytes);
protected:
    virtual bool ProcessPayload(const uint8_t * data, const uint64_t size) override;
private:
    typedef std::pair<uint64_t, uint64_t> bundleid_payloadsize_pair_t;
    OutductManager m_packetOutductManager;
};
#endif //_BP_RECEIVE_PACKET_H

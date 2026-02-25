/**
 * @file BPing.h
 * @author  Brian Tomko <brian.j.tomko@nasa.gov>
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
 * The BPing class is a child class of BpSourcePattern.  It is an app used for
 * sending periodic, wait-for-a-response, bundles, to another bundle agent
 * with a running echo service.  The app copies a tiny payload to the bundle
 * payload block containg a timestamp and sequence number.
 */

#ifndef _BPING_H
#define _BPING_H 1

#include "app_patterns/BpSourcePattern.h"

class BPing : public BpSourcePattern {
public:
    BPing();
    virtual ~BPing() override;
    
protected:
    virtual uint64_t GetNextPayloadLength_Step1() override;
    virtual bool CopyPayload_Step2(uint8_t * destinationBuffer) override;
    virtual bool ProcessNonAdminRecordBundlePayload(const uint8_t * data, const uint64_t size) override;
private:
    uint64_t m_bpingSequenceNumber;
};


#endif //_BPING_H

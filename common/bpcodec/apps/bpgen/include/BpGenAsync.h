/**
 * @file BpGenAsync.h
 * @author  Brian Tomko <brian.j.tomko@nasa.gov>
 * @author  Gilbert Clark
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
 * The BpGenAsync class is a child class of BpSourcePattern.  It is an app used for
 * sending fixed-payload size bundles, either at a defined rate,
 * or as fast as possible.  The app copies a tiny payload to the beginning of the bundle
 * payload block used for counting bundles (in order or out of order).
 * The remaining data in the bundle payload block is unitialized.
 * This app is intended to be used with the BpSink app.
 */

#ifndef _BPGEN_ASYNC_H
#define _BPGEN_ASYNC_H 1

#include "app_patterns/BpSourcePattern.h"

class BpGenAsync : public BpSourcePattern {
private:
    BpGenAsync();
public:
    BpGenAsync(uint64_t bundleSizeBytes);
    virtual ~BpGenAsync() override;
    
protected:
    virtual uint64_t GetNextPayloadLength_Step1() override;
    virtual bool CopyPayload_Step2(uint8_t * destinationBuffer) override;
private:
    uint64_t m_bundleSizeBytes;
    uint64_t m_bpGenSequenceNumber;
};


#endif //_BPGEN_ASYNC_H

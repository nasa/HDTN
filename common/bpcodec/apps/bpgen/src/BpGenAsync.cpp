/**
 * @file BpGenAsync.cpp
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
 */

#include <string.h>
#include <iostream>
#include "BpGenAsync.h"

struct bpgen_hdr {
    bpgen_hdr();

    uint64_t seq;
    uint64_t tsc;
    timespec abstime;
};

bpgen_hdr::bpgen_hdr() : seq(0), tsc(0)
{
    abstime.tv_nsec = 0;
    abstime.tv_sec = 0;
}

BpGenAsync::BpGenAsync(uint64_t bundleSizeBytes) :
    BpSourcePattern(),
    m_bundleSizeBytes(bundleSizeBytes),
    m_bpGenSequenceNumber(0)
{

}

BpGenAsync::~BpGenAsync() {}


uint64_t BpGenAsync::GetNextPayloadLength_Step1() {
    return m_bundleSizeBytes;
}
bool BpGenAsync::CopyPayload_Step2(uint8_t * destinationBuffer) {
    bpgen_hdr bpGenHeader;
    bpGenHeader.seq = m_bpGenSequenceNumber++;
    memcpy(destinationBuffer, &bpGenHeader, sizeof(bpgen_hdr));
    return true;
}

/**
 * @file BpSinkAsync.cpp
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

#include <iostream>
#include "BpSinkAsync.h"
#include <boost/make_unique.hpp>

struct bpgen_hdr {
    uint64_t seq;
    uint64_t tsc;
    timespec abstime;
};

BpSinkAsync::BpSinkAsync() : 
    BpSinkPattern()
{}

BpSinkAsync::~BpSinkAsync() {}

bool BpSinkAsync::ProcessPayload(const uint8_t * data, const uint64_t size) {
    bpgen_hdr bpGenHdr;
    if (size < sizeof(bpgen_hdr)) {
        return false;
    }
    memcpy(&bpGenHdr, data, sizeof(bpgen_hdr));

    

    // offset by the first sequence number we see, so that we don't need to restart for each run ...
    if (m_FinalStatsBpSink.m_seqBase == 0) {
        m_FinalStatsBpSink.m_seqBase = bpGenHdr.seq;
        m_FinalStatsBpSink.m_seqHval = m_FinalStatsBpSink.m_seqBase;
        ++m_FinalStatsBpSink.m_receivedCount; //brian added
    }
    else if (bpGenHdr.seq > m_FinalStatsBpSink.m_seqHval) {
        m_FinalStatsBpSink.m_seqHval = bpGenHdr.seq;
        ++m_FinalStatsBpSink.m_receivedCount;
    }
    else {
        ++m_FinalStatsBpSink.m_duplicateCount;
    }

    //update with latest from base class stats
    m_FinalStatsBpSink.m_totalBundlesRx = m_totalBundlesVersion6Rx + m_totalBundlesVersion7Rx;
    m_FinalStatsBpSink.m_totalBytesRx = m_totalPayloadBytesRx;

    return true;
}

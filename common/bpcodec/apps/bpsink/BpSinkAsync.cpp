/**
 * @file BpSinkAsync.cpp
 * @author  Brian Tomko <brian.j.tomko@nasa.gov>
 * @author  Gilbert Clark
 *
 * @copyright Copyright © 2021 United States Government as represented by
 * the National Aeronautics and Space Administration.
 * No copyright is claimed in the United States under Title 17, U.S.Code.
 * All Other Rights Reserved.
 *
 * @section LICENSE
 * Released under the NASA Open Source Agreement (NOSA)
 * See LICENSE.md in the source root directory for more information.
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

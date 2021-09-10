#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <cassert>
#include <iostream>

#include "codec/bpv6-ext-block.h"
#include "codec/bpv6.h"
//#include "message.hpp"
#ifndef _WIN32
#include "util/tsc.h"
#endif
#include "BpSinkAsync.h"
#include <boost/thread.hpp>
#include <boost/bind.hpp>
#include <boost/make_unique.hpp>
#include "Uri.h"

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
    m_FinalStatsBpSink.m_totalBundlesRx = m_totalBundlesRx;
    m_FinalStatsBpSink.m_totalBytesRx = m_totalBytesRx;

    return true;
}

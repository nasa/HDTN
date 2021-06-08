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

#define BP_MSG_BUFSZ             (65536)
#define BP_BUNDLE_DEFAULT_SZ     (100)
#define BP_GEN_BUNDLE_MAXSZ      (64000)
#define BP_GEN_RATE_MAX          (1 << 30)
#define BP_GEN_TARGET_DEFAULT    "127.0.0.1"
#define BP_GEN_PORT_DEFAULT      (4556)
#define BP_GEN_SRC_NODE_DEFAULT  (1)
#define BP_GEN_DST_NODE_DEFAULT  (2)
#define BP_GEN_BATCH_DEFAULT     (1 << 18)  // write out one entry per this many bundles.
#define BP_GEN_LOGFILE           "bpsink.%lu.csv"

#define BP_MSG_NBUF   (32)


namespace hdtn {

struct bpgen_hdr {
    uint64_t seq;
    uint64_t tsc;
    timespec abstime;
};

BpSinkAsync::BpSinkAsync() {}

BpSinkAsync::~BpSinkAsync() {
    Stop();
}

void BpSinkAsync::Stop() {

    m_inductManager.Clear();

    m_FinalStatsBpSink.m_rtTotal = m_rtTotal;
    m_FinalStatsBpSink.m_seqBase = m_seqBase;
    m_FinalStatsBpSink.m_seqHval = m_seqHval;
    m_FinalStatsBpSink.m_tscTotal = m_tscTotal;
    m_FinalStatsBpSink.m_totalBytesRx = m_totalBytesRx;
    m_FinalStatsBpSink.m_receivedCount = m_receivedCount;
    m_FinalStatsBpSink.m_duplicateCount = m_duplicateCount;
}

int BpSinkAsync::Init(const InductsConfig & inductsConfig, uint32_t processingLagMs) {
    m_batch = BP_GEN_BATCH_DEFAULT;


    m_tscTotal = 0;
    m_rtTotal = 0;
    m_totalBytesRx = 0;

    m_receivedCount = 0;
    m_duplicateCount = 0;
    m_seqHval = 0;
    m_seqBase = 0;

    M_EXTRA_PROCESSING_TIME_MS = processingLagMs;
    m_inductManager.LoadInductsFromConfig(boost::bind(&BpSinkAsync::WholeBundleReadyCallback, this, boost::placeholders::_1), inductsConfig);
    
    return 0;
}



int BpSinkAsync::Process(const std::vector<uint8_t> & rxBuf, const std::size_t messageSize) {
    if (M_EXTRA_PROCESSING_TIME_MS) {
        boost::this_thread::sleep(boost::posix_time::milliseconds(M_EXTRA_PROCESSING_TIME_MS));
    }

    {
        bpv6_primary_block primary;
        bpv6_canonical_block payload;
        const char * const mbuf = (const char*)rxBuf.data();
        uint64_t curr_seq = 0;

        int32_t offset = 0;
        offset += bpv6_primary_block_decode(&primary, mbuf, offset, messageSize);
        if(0 == offset) {
            printf("Malformed bundle received - aborting.\n");
            return -2;
        }
        bool is_payload = false;
        while(!is_payload) {
            int32_t tres = bpv6_canonical_block_decode(&payload, mbuf, offset, messageSize);
            if(tres <= 0) {
                printf("Failed to parse extension block - aborting.\n");
                return -3;
            }
            offset += tres;
            if(payload.type == BPV6_BLOCKTYPE_PAYLOAD) {
                is_payload = true;
            }
        }
        m_totalBytesRx += payload.length;
        bpgen_hdr* data = (bpgen_hdr*)(mbuf + offset);
        // offset by the first sequence number we see, so that we don't need to restart for each run ...
        if(m_seqBase == 0) {
            m_seqBase = data->seq;
            m_seqHval = m_seqBase;
            ++m_receivedCount; //brian added
        }
        else if(data->seq > m_seqHval) {
            m_seqHval = data->seq;
            ++m_receivedCount;
        }
        else {
            ++m_duplicateCount;
        }
#ifndef _WIN32
        timespec tp;
        clock_gettime(CLOCK_REALTIME, &tp);
        int64_t one_way = 1000000 * (((int64_t)tp.tv_sec) - ((int64_t)data->abstime.tv_sec));
        one_way += (((int64_t)tp.tv_nsec) - ((int64_t)data->abstime.tv_nsec)) / 1000;

        m_rtTotal += one_way;
        m_tscTotal += rdtsc() - data->tsc;
#endif // !_WIN32
    }
    //gettimeofday(&tv, NULL);
    //double curr_time = ((double)tv.tv_sec) + ((double)tv.tv_usec / 1000000.0);
    //curr_time -= start;
/*
    if(m_duplicateCount + m_receivedCount >= m_batch) {
        if(m_receivedCount == 0) {
            printf("BUG: batch was entirely duplicates - this shouldn't actually be possible.\n");
        }
        else if(use_one_way) {
            fprintf(log, "%0.6f, %lu, %lu, %lu, %lu, %lu, %lu, %0.4f%%, %0.4f, one_way\n",
                    curr_time, seq_base, seq_hval, received_count, duplicate_count,
                    bytes_total, rt_total,
                    100 - (100 * (received_count / (double)(seq_hval - seq_base))),
                    1000 * ((rt_total / 1000000.0) / received_count) );

        }
        else {
            fprintf(log, "%0.6f, %lu, %lu, %lu, %lu, %lu, %lu, %0.4f%%, %0.4f, rtt\n",
                    curr_time, seq_base, seq_hval, received_count, duplicate_count,
                    bytes_total, tsc_total,
                    100 - (100 * (received_count / (double)(seq_hval - seq_base))),
                    1000 * ((tsc_total / (double)freq_base) / received_count) );

        }
        fflush(log);

        duplicate_count = 0;
        received_count = 0;
        bytes_total = 0;
        seq_hval = 0;
        seq_base = 0;
        rt_total = 0;
        tsc_total = 0;
    }
    */
    return 0;
}

void BpSinkAsync::WholeBundleReadyCallback(std::vector<uint8_t> & wholeBundleVec) {
    //if more than 1 BpSinkAsync context, must protect shared resources with mutex.  Each BpSinkAsync context has
    //its own processing thread that calls this callback
    Process(wholeBundleVec, wholeBundleVec.size());
}



}  // namespace hdtn

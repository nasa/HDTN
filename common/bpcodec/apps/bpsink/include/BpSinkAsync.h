#ifndef _BP_SINK_ASYNC_H
#define _BP_SINK_ASYNC_H

#include <stdint.h>

//#include "message.hpp"
//#include "paths.hpp"



#include "InductManager.h"

namespace hdtn {

struct FinalStatsBpSink {
    FinalStatsBpSink() : m_tscTotal(0),m_rtTotal(0),m_totalBytesRx(0),m_receivedCount(0),m_duplicateCount(0),
        m_seqHval(0),m_seqBase(0){};
    uint64_t m_tscTotal;
    int64_t m_rtTotal;
    uint64_t m_totalBytesRx;
    uint64_t m_receivedCount;
    uint64_t m_duplicateCount;
    uint64_t m_seqHval;
    uint64_t m_seqBase;
};

class BpSinkAsync {
public:
    BpSinkAsync();
    void Stop();
    ~BpSinkAsync();
    int Init(const InductsConfig & inductsConfig, uint32_t processingLagMs);

private:
    void WholeBundleReadyCallback(std::vector<uint8_t> & wholeBundleVec);
    int Process(const std::vector<uint8_t> & rxBuf, const std::size_t messageSize);

public:
    uint32_t m_batch;

    uint64_t m_tscTotal;
    int64_t m_rtTotal;
    uint64_t m_totalBytesRx;

    uint64_t m_receivedCount;
    uint64_t m_duplicateCount;
    uint64_t m_seqHval;
    uint64_t m_seqBase;

    FinalStatsBpSink m_FinalStatsBpSink;

private:
    uint32_t M_EXTRA_PROCESSING_TIME_MS;

    InductManager m_inductManager;
};


}  // namespace hdtn

#endif  //_BP_SINK_ASYNC_H

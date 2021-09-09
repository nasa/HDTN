#ifndef _BP_SINK_ASYNC_H
#define _BP_SINK_ASYNC_H

#include <stdint.h>
#include "InductManager.h"
#include "OutductManager.h"
#include "codec/bpv6.h"
#include "codec/CustodyTransferEnhancementBlock.h"
#include "codec/CustodyTransferManager.h"
#include <boost/asio.hpp>

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
    bool Init(const InductsConfig & inductsConfig, OutductsConfig_ptr & outductsConfigPtr, bool isAcsAware, const cbhe_eid_t & myEid, uint32_t processingLagMs);

private:
    void WholeBundleReadyCallback(std::vector<uint8_t> & wholeBundleVec);
    bool Process(std::vector<uint8_t> & rxBuf, const std::size_t messageSize);
    void AcsNeedToSend_TimerExpired(const boost::system::error_code& e);
    void SendAcsFromTimerThread();
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
    OutductManager m_outductManager;
    bool m_useCustodyTransfer;
    cbhe_eid_t m_myEid;
    std::string m_myEidUriString;
    std::unique_ptr<CustodyTransferManager> m_custodyTransferManagerPtr;
    uint64_t m_nextCtebCustodyId;
    std::vector<uint8_t> m_bufferSpaceForCustodySignalRfc5050SerializedBundle;
    boost::asio::io_service m_ioService;
    boost::asio::deadline_timer m_timerAcs;
    std::unique_ptr<boost::thread> m_ioServiceThreadPtr;
    boost::mutex m_mutexCtm;
    boost::mutex m_mutexForward;
};


}  // namespace hdtn

#endif  //_BP_SINK_ASYNC_H

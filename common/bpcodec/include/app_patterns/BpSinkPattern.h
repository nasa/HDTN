#ifndef _BP_SINK_PATTERN_H
#define _BP_SINK_PATTERN_H 1

#include <stdint.h>
#include "InductManager.h"
#include "OutductManager.h"
#include "codec/bpv6.h"
#include "codec/CustodyTransferEnhancementBlock.h"
#include "codec/CustodyTransferManager.h"
#include <boost/asio.hpp>



class BpSinkPattern {
public:
    BpSinkPattern();
    void Stop();
    virtual ~BpSinkPattern();
    bool Init(const InductsConfig & inductsConfig, OutductsConfig_ptr & outductsConfigPtr, bool isAcsAware, const cbhe_eid_t & myEid, uint32_t processingLagMs);
protected:
    virtual bool ProcessPayload(const uint8_t * data, const uint64_t size) = 0;
private:
    void WholeBundleReadyCallback(std::vector<uint8_t> & wholeBundleVec);
    bool Process(std::vector<uint8_t> & rxBuf, const std::size_t messageSize);
    void AcsNeedToSend_TimerExpired(const boost::system::error_code& e);
    void SendAcsFromTimerThread();
public:

    uint64_t m_totalBytesRx;
    uint64_t m_totalBundlesRx;
    

    

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


#endif  //_BP_SINK_PATTERN_H

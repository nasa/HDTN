#ifndef _BPGEN_ASYNC_H
#define _BPGEN_ASYNC_H 1

#include <string>
#include <boost/thread.hpp>
#include <boost/asio.hpp>
#include "TcpclBundleSource.h"
#include "StcpBundleSource.h"
#include "UdpBundleSource.h"
#include "LtpBundleSource.h"

struct FinalStats {
    bool useTcpcl;
    bool useStcp;

    // udp stats
    uint64_t bundleCount;
    std::size_t m_totalUdpPacketsAckedByUdpSendCallback;
    std::size_t m_totalUdpPacketsAckedByRate;
    std::size_t m_totalUdpPacketsSent;

    //stcp stats
    std::size_t m_totalDataSegmentsAckedByTcpSendCallback;
    std::size_t m_totalDataSegmentsAckedByRate;

    //tcpcl stats
    std::size_t m_totalDataSegmentsAcked;

};


class BpGenAsync {
public:
    BpGenAsync();
    ~BpGenAsync();
    void Stop();
    void Start(const std::string & hostname, const std::string & port, bool useTcpcl, bool useStcp, bool useLtp, uint32_t bundleSizeBytes, uint32_t bundleRate,
        uint32_t tcpclFragmentSize, const std::string & thisLocalEidString,
        uint64_t thisLtpEngineId, uint64_t remoteLtpEngineId, uint64_t ltpDataSegmentMtu, uint64_t oneWayLightTimeMs, uint64_t oneWayMarginTimeMs, uint64_t clientServiceId,
        unsigned int numLtpUdpRxPacketsCircularBufferSize, unsigned int maxLtpRxUdpPacketSizeBytes,
        uint64_t destFlowId = 2, uint64_t stcpRateBitsPerSec = 500000);
    uint64_t m_bundleCount;


    FinalStats m_FinalStats;
    std::size_t GetTotalBundlesAcked();


private:
    void BpGenThreadFunc(uint32_t bundleSizeBytes, uint32_t bundleRate, uint32_t tcpclFragmentSize, uint64_t destFlowId);
    void OnSuccessfulBundleAck();



    std::unique_ptr<TcpclBundleSource> m_tcpclBundleSourcePtr;
    std::unique_ptr<StcpBundleSource> m_stcpBundleSourcePtr;
    std::unique_ptr<LtpBundleSource> m_ltpBundleSourcePtr;
    std::unique_ptr<UdpBundleSource> m_udpBundleSourcePtr;
    std::unique_ptr<boost::thread> m_bpGenThreadPtr;
    boost::condition_variable m_conditionVariableAckReceived;
    volatile bool m_running;
};


#endif //_BPGEN_ASYNC_H

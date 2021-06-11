#ifndef _BPGEN_ASYNC_H
#define _BPGEN_ASYNC_H 1

#include <string>
#include <boost/thread.hpp>
#include <boost/asio.hpp>
#include "OutductManager.h"

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

    //tcpcl stats
    std::size_t m_totalDataSegmentsAcked;

};


class BpGenAsync {
public:
    BpGenAsync();
    ~BpGenAsync();
    void Stop();
    void Start(const OutductsConfig & outductsConfig, uint32_t bundleSizeBytes, uint32_t bundleRate, uint64_t destFlowId = 2);

    uint64_t m_bundleCount;

    OutductFinalStats m_outductFinalStats;
    FinalStats m_FinalStats;


private:
    void BpGenThreadFunc(uint32_t bundleSizeBytes, uint32_t bundleRate, uint64_t destFlowId);



    OutductManager m_outductManager;
    std::unique_ptr<boost::thread> m_bpGenThreadPtr;
    volatile bool m_running;
};


#endif //_BPGEN_ASYNC_H

#ifndef _BPGEN_ASYNC_H
#define _BPGEN_ASYNC_H 1

#include <string>
#include <boost/thread.hpp>
#include <boost/asio.hpp>
#include "TcpclBundleSource.h"
#include "StcpBundleSource.h"
#include "UdpBundleSource.h"

class BpGenAsync {
public:
    BpGenAsync();
    ~BpGenAsync();
    void Stop();
    void Start(const std::string & hostname, const std::string & port, bool useTcpcl, bool useStcp, uint32_t bundleSizeBytes, uint32_t bundleRate, uint32_t tcpclFragmentSize, const std::string & thisLocalEidString, uint64_t destFlowId = 2, uint64_t stcpRateBitsPerSec = 500000);
    uint64_t m_bundleCount;
private:
    void BpGenThreadFunc(uint32_t bundleSizeBytes, uint32_t bundleRate, uint32_t tcpclFragmentSize, uint64_t destFlowId);
    void OnSuccessfulBundleAck();



    boost::shared_ptr<TcpclBundleSource> m_tcpclBundleSourcePtr;
    boost::shared_ptr<StcpBundleSource> m_stcpBundleSourcePtr;
    std::unique_ptr<UdpBundleSource> m_udpBundleSourcePtr;
    boost::shared_ptr<boost::thread> m_bpGenThreadPtr;
    boost::condition_variable m_conditionVariableAckReceived;
    volatile bool m_running;
};


#endif //_BPGEN_ASYNC_H

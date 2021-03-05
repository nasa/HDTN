#ifndef _BPGEN_ASYNC_H
#define _BPGEN_ASYNC_H 1

#include <string>
#include <boost/thread.hpp>
#include <boost/asio.hpp>
#include "TcpclBundleSource.h"
#include "StcpBundleSource.h"


class BpGenAsync {
public:
    BpGenAsync();
    ~BpGenAsync();
    void Stop();
    void Start(const std::string & hostname, const std::string & port, bool useTcpcl, bool useStcp, uint32_t bundleSizeBytes, uint32_t bundleRate, uint32_t tcpclFragmentSize, const std::string & thisLocalEidString, uint64_t destFlowId = 2);
    uint64_t m_bundleCount;
private:
    void BpGenThreadFunc(uint32_t bundleSizeBytes, uint32_t bundleRate, uint32_t tcpclFragmentSize, uint64_t destFlowId);
    void HandleUdpSendBundle(boost::shared_ptr<std::vector<uint8_t> > vecPtr, const boost::system::error_code& error, std::size_t bytes_transferred);




    boost::asio::io_service m_ioService;
    boost::asio::ip::udp::socket m_udpSocket;
    boost::asio::io_service::work m_work; //keep ioservice::run from exiting when no work to do
    boost::shared_ptr<TcpclBundleSource> m_tcpclBundleSourcePtr;
    boost::shared_ptr<StcpBundleSource> m_stcpBundleSourcePtr;
    boost::shared_ptr<boost::thread> m_ioServiceThreadPtr;
    boost::shared_ptr<boost::thread> m_bpGenThreadPtr;
    boost::asio::ip::udp::endpoint m_udpDestinationEndpoint;
    volatile bool m_running;
};


#endif //_BPGEN_ASYNC_H

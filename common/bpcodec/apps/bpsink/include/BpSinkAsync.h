#ifndef _BP_SINK_ASYNC_H
#define _BP_SINK_ASYNC_H

#include <stdint.h>

//#include "message.hpp"
//#include "paths.hpp"



#include "CircularIndexBufferSingleProducerSingleConsumerConfigurable.h"
#include <boost/asio.hpp>
#include <boost/thread.hpp>
#include "TcpclBundleSink.h"
#include "StcpBundleSink.h"
#include "UdpBundleSink.h"

namespace hdtn {


class BpSinkAsync {
private:
    BpSinkAsync();
public:
    BpSinkAsync(uint16_t port, bool useTcpcl, bool useStcp, const std::string & thisLocalEidString, const uint32_t extraProcessingTimeMs = 0);  // initialize message buffers
    void Stop();
    ~BpSinkAsync();
    int Init(uint32_t type);
    int Netstart();
    //int send_telemetry();
private:
    void WholeBundleReadyCallback(std::vector<uint8_t> & wholeBundleVec);
    int Process(const std::vector<uint8_t> & rxBuf, const std::size_t messageSize);

    void StartTcpAccept();
    void HandleTcpAccept(boost::shared_ptr<boost::asio::ip::tcp::socket> newTcpSocketPtr, const boost::system::error_code& error);


public:
    uint32_t m_batch;

    uint64_t m_tscTotal;
    int64_t m_rtTotal;
    uint64_t m_totalBytesRx;

    uint64_t m_receivedCount;
    uint64_t m_duplicateCount;
    uint64_t m_seqHval;
    uint64_t m_seqBase;

private:
    const uint16_t m_rxPortUdpOrTcp;
    const bool m_useTcpcl;
    const bool m_useStcp;
    const std::string M_THIS_EID_STRING;
    const uint32_t M_EXTRA_PROCESSING_TIME_MS;

    int m_type;
    boost::asio::io_service m_ioService;
    boost::asio::ip::tcp::acceptor m_tcpAcceptor;

    std::unique_ptr<TcpclBundleSink> m_tcpclBundleSinkPtr;
    std::unique_ptr<StcpBundleSink> m_stcpBundleSinkPtr;
    std::unique_ptr<UdpBundleSink> m_udpBundleSinkPtr;
    std::unique_ptr<boost::thread> m_ioServiceThreadPtr;
    volatile bool m_running;
};


}  // namespace hdtn

#endif  //_BP_SINK_ASYNC_H

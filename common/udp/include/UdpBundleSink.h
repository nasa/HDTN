#ifndef _UDP_BUNDLE_SINK_H
#define _UDP_BUNDLE_SINK_H 1

#include <stdint.h>
#include <boost/asio.hpp>
#include <boost/thread.hpp>
#include <boost/function.hpp>
#include <boost/shared_ptr.hpp>
#include "CircularIndexBufferSingleProducerSingleConsumerConfigurable.h"

class UdpBundleSink {
private:
    UdpBundleSink();
public:
    typedef boost::function<void(const std::vector<uint8_t> & bundleBuffer, const std::size_t bundleSizeBytes)> WholeBundleReadyCallbackUdp_t;
    //typedef boost::function<void()> ConnectionClosedCallback_t;

    UdpBundleSink(boost::asio::io_service & ioService,
                    uint16_t udpPort,
                    const WholeBundleReadyCallbackUdp_t & wholeBundleReadyCallback,
                    //ConnectionClosedCallback_t connectionClosedCallback,
                    const unsigned int numCircularBufferVectors,
                    const unsigned int maxUdpPacketSizeBytes);
    ~UdpBundleSink();
    bool ReadyToBeDeleted();
private:

    void StartUdpReceive();
    void HandleUdpReceive(const boost::system::error_code & error, std::size_t bytesTransferred, unsigned int writeIndex);
    void PopCbThreadFunc();
    void DoUdpShutdown();


    
    //std::vector<uint8_t> m_fragmentedBundleRxConcat;

    WholeBundleReadyCallbackUdp_t m_wholeBundleReadyCallback;
    //ConnectionClosedCallback_t m_connectionClosedCallback;

    boost::asio::ip::udp::socket m_udpSocket;
    

    const unsigned int M_NUM_CIRCULAR_BUFFER_VECTORS;
    const unsigned int M_MAX_UDP_PACKET_SIZE_BYTES;
    CircularIndexBufferSingleProducerSingleConsumerConfigurable m_circularIndexBuffer;
    std::vector<std::vector<boost::uint8_t> > m_udpReceiveBuffersCbVec;
    std::vector<boost::asio::ip::udp::endpoint> m_remoteEndpointsCbVec;
    std::vector<std::size_t> m_udpReceiveBytesTransferredCbVec;
    boost::condition_variable m_conditionVariableCb;
    boost::shared_ptr<boost::thread> m_threadCbReaderPtr;
    volatile bool m_running;
    volatile bool m_safeToDelete;
    uint32_t m_incomingBundleSize;
};



#endif  //_UDP_BUNDLE_SINK_H

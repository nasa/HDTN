#ifndef _STCP_BUNDLE_SINK_H
#define _STCP_BUNDLE_SINK_H 1

#include <stdint.h>
#include <boost/asio.hpp>
#include <boost/thread.hpp>
#include <boost/function.hpp>
#include <boost/shared_ptr.hpp>
#include "CircularIndexBufferSingleProducerSingleConsumerConfigurable.h"

class StcpBundleSink {
private:
    StcpBundleSink();
public:
    typedef boost::function<void(boost::shared_ptr<std::vector<uint8_t> > wholeBundleSharedPtr)> WholeBundleReadyCallback_t;
    //typedef boost::function<void()> ConnectionClosedCallback_t;

    StcpBundleSink(boost::shared_ptr<boost::asio::ip::tcp::socket> tcpSocketPtr,
                    WholeBundleReadyCallback_t wholeBundleReadyCallback,
                    //ConnectionClosedCallback_t connectionClosedCallback,
                    const unsigned int numCircularBufferVectors);
    ~StcpBundleSink();
    bool ReadyToBeDeleted();
private:

    void StartTcpReceiveIncomingBundleSize();
    void HandleTcpReceiveIncomingBundleSize(const boost::system::error_code & error, std::size_t bytesTransferred);
    void StartTcpReceiveBundleData();
    void HandleTcpReceiveBundleData(const boost::system::error_code & error, std::size_t bytesTransferred, unsigned int writeIndex);
    void PopCbThreadFunc();
    void DoStcpShutdown();


    
    //std::vector<uint8_t> m_fragmentedBundleRxConcat;

    WholeBundleReadyCallback_t m_wholeBundleReadyCallback;
    //ConnectionClosedCallback_t m_connectionClosedCallback;

    boost::shared_ptr<boost::asio::ip::tcp::socket> m_tcpSocketPtr;
    

    const unsigned int M_NUM_CIRCULAR_BUFFER_VECTORS;
    CircularIndexBufferSingleProducerSingleConsumerConfigurable m_circularIndexBuffer;
    std::vector<std::vector<boost::uint8_t> > m_tcpReceiveBuffersCbVec;
    std::vector<std::size_t> m_tcpReceiveBytesTransferredCbVec;
    boost::condition_variable m_conditionVariableCb;
    boost::shared_ptr<boost::thread> m_threadCbReaderPtr;
    volatile bool m_running;
    volatile bool m_safeToDelete;
    uint32_t m_incomingBundleSize;
};



#endif  //_STCP_BUNDLE_SINK_H

#ifndef _STCP_BUNDLE_SINK_H
#define _STCP_BUNDLE_SINK_H 1

#include <stdint.h>
#include <boost/asio.hpp>
#include <boost/thread.hpp>
#include <boost/function.hpp>
#include <boost/shared_ptr.hpp>
#include "CircularIndexBufferSingleProducerSingleConsumerConfigurable.h"
#include "PaddedVectorUint8.h"
#include "stcp_lib_export.h"

class StcpBundleSink {
private:
    StcpBundleSink();
public:
    typedef boost::function<void(padded_vector_uint8_t & wholeBundleVec)> WholeBundleReadyCallback_t;
    typedef boost::function<void()> NotifyReadyToDeleteCallback_t;

    STCP_LIB_EXPORT StcpBundleSink(boost::shared_ptr<boost::asio::ip::tcp::socket> tcpSocketPtr,
        boost::asio::io_service & tcpSocketIoServiceRef,
        const WholeBundleReadyCallback_t & wholeBundleReadyCallback,
        const unsigned int numCircularBufferVectors,
        const uint64_t maxBundleSizeBytes,
        const NotifyReadyToDeleteCallback_t & notifyReadyToDeleteCallback = NotifyReadyToDeleteCallback_t());
    STCP_LIB_EXPORT ~StcpBundleSink();
    STCP_LIB_EXPORT bool ReadyToBeDeleted();
private:

    STCP_LIB_EXPORT void TryStartTcpReceive();
    STCP_LIB_EXPORT void HandleTcpReceiveIncomingBundleSize(const boost::system::error_code & error, std::size_t bytesTransferred, const unsigned int writeIndex);
    STCP_LIB_EXPORT void HandleTcpReceiveBundleData(const boost::system::error_code & error, std::size_t bytesTransferred, unsigned int writeIndex);
    STCP_LIB_EXPORT void PopCbThreadFunc();
    STCP_LIB_EXPORT void DoStcpShutdown();
    STCP_LIB_EXPORT void HandleSocketShutdown();
    
    const WholeBundleReadyCallback_t m_wholeBundleReadyCallback;
    const NotifyReadyToDeleteCallback_t m_notifyReadyToDeleteCallback;

    boost::shared_ptr<boost::asio::ip::tcp::socket> m_tcpSocketPtr;
    boost::asio::io_service & m_tcpSocketIoServiceRef;

    const unsigned int M_NUM_CIRCULAR_BUFFER_VECTORS;
    const uint64_t M_MAX_BUNDLE_SIZE_BYTES;
    CircularIndexBufferSingleProducerSingleConsumerConfigurable m_circularIndexBuffer;
    std::vector<padded_vector_uint8_t > m_tcpReceiveBuffersCbVec;
    std::vector<std::size_t> m_tcpReceiveBytesTransferredCbVec;
    boost::condition_variable m_conditionVariableCb;
    std::unique_ptr<boost::thread> m_threadCbReaderPtr;
    bool m_stateTcpReadActive;
    bool m_printedCbTooSmallNotice;
    volatile bool m_running;
    volatile bool m_safeToDelete;
    uint32_t m_incomingBundleSize;
};



#endif  //_STCP_BUNDLE_SINK_H

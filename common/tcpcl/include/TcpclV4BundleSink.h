/**
 * @file TcpclV4BundleSink.h
 * @author  Brian Tomko <brian.j.tomko@nasa.gov>
 *
 * @copyright Copyright © 2021 United States Government as represented by
 * the National Aeronautics and Space Administration.
 * No copyright is claimed in the United States under Title 17, U.S.Code.
 * All Other Rights Reserved.
 *
 * @section LICENSE
 * Released under the NASA Open Source Agreement (NOSA)
 * See LICENSE.md in the source root directory for more information.
 *
 * @section DESCRIPTION
 *
 * This TcpclV4BundleSink class encapsulates the appropriate TCPCL version 4 functionality
 * to receive bundles (or any other user defined data) over a TCPCL version 4 link (either encrypted or not)
 * and calls the user defined function WholeBundleReadyCallback_t when a new bundle
 * is received.
 */

#ifndef _TCPCLV4_BUNDLE_SINK_H
#define _TCPCLV4_BUNDLE_SINK_H 1

#include "TcpclV4BidirectionalLink.h"
#include <atomic>

class CLASS_VISIBILITY_TCPCL_LIB TcpclV4BundleSink : public TcpclV4BidirectionalLink {
private:
    TcpclV4BundleSink();
public:
    typedef boost::function<void(padded_vector_uint8_t & wholeBundleVec)> WholeBundleReadyCallback_t;
    typedef boost::function<void()> NotifyReadyToDeleteCallback_t;
    typedef boost::function<bool(std::pair<std::unique_ptr<zmq::message_t>, padded_vector_uint8_t> & bundleDataPair)> TryGetOpportunisticDataFunction_t;
    typedef boost::function<void()> NotifyOpportunisticDataAckedCallback_t;
    typedef boost::function<void(TcpclV4BundleSink * thisTcpclBundleSinkPtr)> OnContactHeaderCallback_t;

    TCPCL_LIB_EXPORT TcpclV4BundleSink(
#ifdef OPENSSL_SUPPORT_ENABLED
        std::shared_ptr<boost::asio::ssl::stream<boost::asio::ip::tcp::socket> > & sslStreamSharedPtr,
#else
        std::shared_ptr<boost::asio::ip::tcp::socket> & tcpSocketPtr,
#endif
        const bool tlsSuccessfullyConfigured,
        const bool tlsIsRequired,
        const uint16_t desiredKeepAliveIntervalSeconds,
        boost::asio::io_service & tcpSocketIoServiceRef,
        const WholeBundleReadyCallback_t & wholeBundleReadyCallback,
        const unsigned int numCircularBufferVectors,
        const unsigned int circularBufferBytesPerVector,
        const uint64_t myNodeId,
        const uint64_t maxBundleSizeBytes,
        const NotifyReadyToDeleteCallback_t & notifyReadyToDeleteCallback = NotifyReadyToDeleteCallback_t(),
        const OnContactHeaderCallback_t & onContactHeaderCallback = OnContactHeaderCallback_t(),
        //const TryGetOpportunisticDataFunction_t & tryGetOpportunisticDataFunction = TryGetOpportunisticDataFunction_t(),
        //const NotifyOpportunisticDataAckedCallback_t & notifyOpportunisticDataAckedCallback = NotifyOpportunisticDataAckedCallback_t(),
        const unsigned int maxUnacked = 10, const uint64_t maxFragmentSize = 100000000 ); //todo
    TCPCL_LIB_EXPORT virtual ~TcpclV4BundleSink() override;
    TCPCL_LIB_EXPORT bool ReadyToBeDeleted();
    TCPCL_LIB_EXPORT uint64_t GetRemoteNodeId() const;
    TCPCL_LIB_EXPORT void TrySendOpportunisticBundleIfAvailable_FromIoServiceThread();
    TCPCL_LIB_EXPORT void SetTryGetOpportunisticDataFunction(const TryGetOpportunisticDataFunction_t & tryGetOpportunisticDataFunction);
    TCPCL_LIB_EXPORT void SetNotifyOpportunisticDataAckedCallback(const NotifyOpportunisticDataAckedCallback_t & notifyOpportunisticDataAckedCallback);
private:

    
#ifdef OPENSSL_SUPPORT_ENABLED
    TCPCL_LIB_NO_EXPORT void DoSslUpgrade();
    TCPCL_LIB_NO_EXPORT void HandleSslHandshake(const boost::system::error_code & error);
    TCPCL_LIB_NO_EXPORT void TryStartTcpReceiveSecure();
    TCPCL_LIB_NO_EXPORT void HandleTcpReceiveSomeSecure(const boost::system::error_code & error, std::size_t bytesTransferred, unsigned int writeIndex);
#endif
    TCPCL_LIB_NO_EXPORT void TryStartTcpReceiveUnsecure();
    TCPCL_LIB_NO_EXPORT void HandleTcpReceiveSomeUnsecure(const boost::system::error_code & error, std::size_t bytesTransferred, unsigned int writeIndex);
    TCPCL_LIB_NO_EXPORT void PopCbThreadFunc();
    
    TCPCL_LIB_NO_EXPORT virtual void Virtual_OnTcpclShutdownComplete_CalledFromIoServiceThread() override;
    TCPCL_LIB_NO_EXPORT virtual void Virtual_OnSuccessfulWholeBundleAcknowledged() override;
    TCPCL_LIB_NO_EXPORT virtual void Virtual_WholeBundleReady(padded_vector_uint8_t & wholeBundleVec) override;
    TCPCL_LIB_NO_EXPORT virtual void Virtual_OnTcpSendSuccessful_CalledFromIoServiceThread() override;
    TCPCL_LIB_NO_EXPORT virtual void Virtual_OnTcpSendContactHeaderSuccessful_CalledFromIoServiceThread() override;
    TCPCL_LIB_NO_EXPORT virtual void Virtual_OnSessionInitReceivedAndProcessedSuccessfully() override;

    

    const WholeBundleReadyCallback_t m_wholeBundleReadyCallback;
    const NotifyReadyToDeleteCallback_t m_notifyReadyToDeleteCallback;
    TryGetOpportunisticDataFunction_t m_tryGetOpportunisticDataFunction;
    NotifyOpportunisticDataAckedCallback_t m_notifyOpportunisticDataAckedCallback;
    const OnContactHeaderCallback_t m_onContactHeaderCallback;

    
    boost::asio::io_service & m_tcpSocketIoServiceRef;
    

    const unsigned int M_NUM_CIRCULAR_BUFFER_VECTORS;
    const unsigned int M_CIRCULAR_BUFFER_BYTES_PER_VECTOR;
    CircularIndexBufferSingleProducerSingleConsumerConfigurable m_circularIndexBuffer;
    std::vector<std::vector<boost::uint8_t> > m_tcpReceiveBuffersCbVec;
    std::vector<std::size_t> m_tcpReceiveBytesTransferredCbVec;
    boost::condition_variable m_conditionVariableCb;
    boost::mutex m_mutexCb;
    std::unique_ptr<boost::thread> m_threadCbReaderPtr;
    bool m_stateTcpReadActive;
    bool m_printedCbTooSmallNotice;
    std::atomic<bool> m_running;
    

    
};



#endif  //_TCPCLV4_BUNDLE_SINK_H

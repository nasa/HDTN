#include <boost/bind/bind.hpp>
#include <boost/make_shared.hpp>
#include <iostream>
#include "TcpclV4BundleSink.h"
#include <boost/make_unique.hpp>
#include "Uri.h"

TcpclV4BundleSink::TcpclV4BundleSink(
#ifdef OPENSSL_SUPPORT_ENABLED
    boost::shared_ptr<boost::asio::ssl::stream<boost::asio::ip::tcp::socket> > & sslStreamSharedPtr,
#else
    boost::shared_ptr<boost::asio::ip::tcp::socket> & tcpSocketPtr,
#endif
    const bool tlsSuccessfullyConfigured,
    const bool tlsIsRequired,
    const uint16_t desiredKeepAliveIntervalSeconds, //new
    boost::asio::io_service & tcpSocketIoServiceRef,
    const WholeBundleReadyCallback_t & wholeBundleReadyCallback,
    //ConnectionClosedCallback_t connectionClosedCallback,
    const unsigned int numCircularBufferVectors,
    const unsigned int circularBufferBytesPerVector,
    const uint64_t myNodeId,
    const uint64_t maxBundleSizeBytes,
    const NotifyReadyToDeleteCallback_t & notifyReadyToDeleteCallback,
    const OnContactHeaderCallback_t & onContactHeaderCallback,
    //const TryGetOpportunisticDataFunction_t & tryGetOpportunisticDataFunction,
    //const NotifyOpportunisticDataAckedCallback_t & notifyOpportunisticDataAckedCallback,
    const unsigned int maxUnacked, const uint64_t maxFragmentSize) :

    TcpclV4BidirectionalLink(
        "TcpclV4BundleSink",
        3, //bundleSink shutdown message shall send 3 second reconnection delay to prevent disabling hdtn outduct reconnect
        true, //bundleSink shall delete socket after shutdown
        false, //false => sink is passive entity (not active)
        desiredKeepAliveIntervalSeconds,
        &tcpSocketIoServiceRef,
        maxUnacked,
        maxFragmentSize,
        maxBundleSizeBytes,
        myNodeId,
        "", //expectedRemoteEidUri empty => don't check remote connections to this sink (allow all ipn's)
        tlsSuccessfullyConfigured, //tryUseTls
        tlsIsRequired //tlsIsRequired
    ),
    

    m_wholeBundleReadyCallback(wholeBundleReadyCallback),
    m_notifyReadyToDeleteCallback(notifyReadyToDeleteCallback),
    m_onContactHeaderCallback(onContactHeaderCallback),
    m_tcpSocketIoServiceRef(tcpSocketIoServiceRef),
    M_NUM_CIRCULAR_BUFFER_VECTORS(numCircularBufferVectors),
    M_CIRCULAR_BUFFER_BYTES_PER_VECTOR(circularBufferBytesPerVector),
    m_circularIndexBuffer(M_NUM_CIRCULAR_BUFFER_VECTORS),
    m_tcpReceiveBuffersCbVec(M_NUM_CIRCULAR_BUFFER_VECTORS),
    m_tcpReceiveBytesTransferredCbVec(M_NUM_CIRCULAR_BUFFER_VECTORS),
    m_stateTcpReadActive(false),
    m_printedCbTooSmallNotice(false),
    m_running(false)
{
#ifdef OPENSSL_SUPPORT_ENABLED
    m_base_sslStreamSharedPtr = sslStreamSharedPtr;
    m_base_tcpAsyncSenderSslPtr = boost::make_unique<TcpAsyncSenderSsl>(m_base_sslStreamSharedPtr, m_base_ioServiceRef);
#else
    m_base_tcpSocketPtr = tcpSocketPtr;
    m_base_tcpAsyncSenderPtr = boost::make_unique<TcpAsyncSender>(m_base_tcpSocketPtr, m_tcpSocketIoServiceRef);
#endif

    for (unsigned int i = 0; i < M_NUM_CIRCULAR_BUFFER_VECTORS; ++i) {
        m_tcpReceiveBuffersCbVec[i].resize(M_CIRCULAR_BUFFER_BYTES_PER_VECTOR);
    }

    
    m_running = true;
    m_threadCbReaderPtr = boost::make_unique<boost::thread>(
        boost::bind(&TcpclV4BundleSink::PopCbThreadFunc, this)); //create and start the worker thread

    TryStartTcpReceiveUnsecure();
}

TcpclV4BundleSink::~TcpclV4BundleSink() {
    //prevent TcpclV4BundleSink Opportunistic sending of bundles from exiting before all bundles sent and acked
    BaseClass_TryToWaitForAllBundlesToFinishSending();

    if (!m_base_sinkIsSafeToDelete) {
        BaseClass_DoTcpclShutdown(true, TCPCLV4_SESSION_TERMINATION_REASON_CODES::UNKNOWN, false);
        while (!m_base_sinkIsSafeToDelete) {
            boost::this_thread::sleep(boost::posix_time::milliseconds(250));
        }
    }

    m_running = false; //thread stopping criteria

    if (m_threadCbReaderPtr) {
        m_threadCbReaderPtr->join();
        m_threadCbReaderPtr.reset(); //delete it
    }
#ifdef OPENSSL_SUPPORT_ENABLED
    m_base_tcpAsyncSenderSslPtr.reset();
#else
    m_base_tcpAsyncSenderPtr.reset();
#endif
}


#ifdef OPENSSL_SUPPORT_ENABLED
void TcpclV4BundleSink::DoSslUpgrade() { //must run within Io Service Thread
    m_base_sslStreamSharedPtr->next_layer().cancel(); //cancel any active receives
    m_stateTcpReadActive = true; //keep TryStartTcpReceive() from restarting a receive (until handshake complete)
    m_base_sslStreamSharedPtr->async_handshake(boost::asio::ssl::stream_base::server,
        boost::bind(&TcpclV4BundleSink::HandleSslHandshake, this, boost::asio::placeholders::error));
}

void TcpclV4BundleSink::HandleSslHandshake(const boost::system::error_code & error) {
    if (!error) {
        std::cout << "SSL/TLS Handshake succeeded.. all transmissions shall be secure from this point\n";
        m_base_didSuccessfulSslHandshake = true;
        m_stateTcpReadActive = false; //must be false before calling TryStartTcpReceiveSecure
        TryStartTcpReceiveSecure();
        //BaseClass_SendSessionInit(); I am the passive entity and will send (from within my session init rx callback) a session init when i first receive a session init from the active entity (from bundle source)
    }
    else {
        std::cout << "SSL/TLS Handshake failed: " << error.message() << "\n";
        BaseClass_DoTcpclShutdown(false, TCPCLV4_SESSION_TERMINATION_REASON_CODES::UNKNOWN, false);
    }
}

void TcpclV4BundleSink::TryStartTcpReceiveSecure() { //must run within Io Service Thread
    if ((!m_stateTcpReadActive) && (m_base_sslStreamSharedPtr)) {
        const unsigned int writeIndex = m_circularIndexBuffer.GetIndexForWrite(); //store the volatile
        if (writeIndex == UINT32_MAX) {
            if (!m_printedCbTooSmallNotice) {
                m_printedCbTooSmallNotice = true;
                std::cout << "notice in TcpclV4BundleSink::StartTcpReceive(): buffers full.. you might want to increase the circular buffer size for better performance!" << std::endl;
            }
        }
        else {
            m_stateTcpReadActive = true;
            m_base_sslStreamSharedPtr->async_read_some(
                boost::asio::buffer(m_tcpReceiveBuffersCbVec[writeIndex]),
                boost::bind(&TcpclV4BundleSink::HandleTcpReceiveSomeSecure, this,
                    boost::asio::placeholders::error,
                    boost::asio::placeholders::bytes_transferred,
                    writeIndex));
        }
    }
}

void TcpclV4BundleSink::HandleTcpReceiveSomeSecure(const boost::system::error_code & error, std::size_t bytesTransferred, unsigned int writeIndex) {
    if (!error) {
        m_tcpReceiveBytesTransferredCbVec[writeIndex] = bytesTransferred;
        m_circularIndexBuffer.CommitWrite(); //write complete at this point
        m_stateTcpReadActive = false; //must be false before calling TryStartTcpReceive
        m_conditionVariableCb.notify_one();
        TryStartTcpReceiveSecure(); //restart operation only if there was no error
    }
    else if (error == boost::asio::error::eof) {
        std::cout << "TcpclBundleSink Tcp connection closed cleanly by peer" << std::endl;
        BaseClass_DoTcpclShutdown(false, TCPCLV4_SESSION_TERMINATION_REASON_CODES::UNKNOWN, false);
    }
    else if (error != boost::asio::error::operation_aborted) { //will always be operation_aborted when thread is terminating
        std::cerr << "Error in TcpclV4BundleSink::HandleTcpReceiveSomeSecure: " << error.message() << std::endl;
    }
}

void TcpclV4BundleSink::TryStartTcpReceiveUnsecure() { //must run within Io Service Thread
    if ((!m_stateTcpReadActive) && (m_base_sslStreamSharedPtr)) {
        boost::asio::ip::tcp::socket & socketRef = m_base_sslStreamSharedPtr->next_layer();
#else
void TcpclV4BundleSink::TryStartTcpReceiveUnsecure() { //must run within Io Service Thread
    if ((!m_stateTcpReadActive) && (m_base_tcpSocketPtr)) {
        boost::asio::ip::tcp::socket & socketRef = *m_base_tcpSocketPtr;
#endif
    
        const unsigned int writeIndex = m_circularIndexBuffer.GetIndexForWrite(); //store the volatile
        if (writeIndex == UINT32_MAX) {
            if (!m_printedCbTooSmallNotice) {
                m_printedCbTooSmallNotice = true;
                std::cout << "notice in TcpclV4BundleSink::StartTcpReceive(): buffers full.. you might want to increase the circular buffer size for better performance!" << std::endl;
            }
        }
        else {
            m_stateTcpReadActive = true;
            socketRef.async_read_some(
                boost::asio::buffer(m_tcpReceiveBuffersCbVec[writeIndex]),
                boost::bind(&TcpclV4BundleSink::HandleTcpReceiveSomeUnsecure, this,
                    boost::asio::placeholders::error,
                    boost::asio::placeholders::bytes_transferred,
                    writeIndex));
        }
    }
}
void TcpclV4BundleSink::HandleTcpReceiveSomeUnsecure(const boost::system::error_code & error, std::size_t bytesTransferred, unsigned int writeIndex) {
    if (!error) {
        m_tcpReceiveBytesTransferredCbVec[writeIndex] = bytesTransferred;
        m_circularIndexBuffer.CommitWrite(); //write complete at this point
        m_stateTcpReadActive = false; //must be false before calling TryStartTcpReceive
        m_conditionVariableCb.notify_one();
        TryStartTcpReceiveUnsecure(); //restart operation only if there was no error
    }
    else if (error == boost::asio::error::eof) {
        std::cout << "TcpclBundleSink Tcp connection closed cleanly by peer" << std::endl;
        BaseClass_DoTcpclShutdown(false, TCPCLV4_SESSION_TERMINATION_REASON_CODES::UNKNOWN, false);
    }
    else if (error != boost::asio::error::operation_aborted) { //will always be operation_aborted when thread is terminating
        std::cerr << "Error in TcpclV4BundleSink::HandleTcpReceiveSome: " << error.message() << std::endl;
    }
}

void TcpclV4BundleSink::PopCbThreadFunc() {

    boost::mutex localMutex;
    boost::mutex::scoped_lock lock(localMutex);
    boost::function<void()> tryStartTcpReceiveFunction = boost::bind(&TcpclV4BundleSink::TryStartTcpReceiveUnsecure, this);

    while (m_running || (m_circularIndexBuffer.GetIndexForRead() != UINT32_MAX)) { //keep thread alive if running or cb not empty


        const unsigned int consumeIndex = m_circularIndexBuffer.GetIndexForRead(); //store the volatile
        boost::asio::post(m_tcpSocketIoServiceRef, tryStartTcpReceiveFunction); //keep this a thread safe operation by letting ioService thread run it
        if (consumeIndex == UINT32_MAX) { //if empty
            m_conditionVariableCb.timed_wait(lock, boost::posix_time::milliseconds(10)); // call lock.unlock() and blocks the current thread
            //thread is now unblocked, and the lock is reacquired by invoking lock.lock()
            continue;
        }

        m_base_tcpclV4RxStateMachine.HandleReceivedChars(m_tcpReceiveBuffersCbVec[consumeIndex].data(), m_tcpReceiveBytesTransferredCbVec[consumeIndex]);
        m_circularIndexBuffer.CommitRead();
#ifdef OPENSSL_SUPPORT_ENABLED
        if (m_base_doUpgradeSocketToSsl) { //the tcpclv4 rx state machine may have set m_base_doUpgradeSocketToSsl to true after HandleReceivedChars()
            std::cout << "sink going to call handshake\n";
            m_base_doUpgradeSocketToSsl = false;
            tryStartTcpReceiveFunction = boost::bind(&TcpclV4BundleSink::TryStartTcpReceiveSecure, this);
            //boost::asio::post(m_tcpSocketIoServiceRef, boost::bind(&TcpclV4BundleSink::DoSslUpgrade, this)); //keep this a thread safe operation by letting ioService thread run it
        }
#endif
    }

    std::cout << "TcpclBundleSink Circular buffer reader thread exiting\n";

}

void TcpclV4BundleSink::Virtual_OnTcpSendSuccessful_CalledFromIoServiceThread() {
    ////TrySendOpportunisticBundleIfAvailable_FromIoServiceThread();
}
void TcpclV4BundleSink::Virtual_OnTcpSendContactHeaderSuccessful_CalledFromIoServiceThread() {
    std::cout << "sink virtual contact header sent\n";
    if (m_base_usingTls) {
        DoSslUpgrade();
    }
}

void TcpclV4BundleSink::Virtual_OnSessionInitReceivedAndProcessedSuccessfully() {
    if (m_onContactHeaderCallback) {
        m_onContactHeaderCallback(this);
    }
}


void TcpclV4BundleSink::Virtual_OnTcpclShutdownComplete_CalledFromIoServiceThread() {
    if (m_notifyReadyToDeleteCallback) {
        m_notifyReadyToDeleteCallback();
    }
}

void TcpclV4BundleSink::Virtual_OnSuccessfulWholeBundleAcknowledged() {
    if (m_notifyOpportunisticDataAckedCallback) {
        m_notifyOpportunisticDataAckedCallback();
    }
}

void TcpclV4BundleSink::Virtual_WholeBundleReady(std::vector<uint8_t> & wholeBundleVec) {
    m_wholeBundleReadyCallback(wholeBundleVec);
}


bool TcpclV4BundleSink::ReadyToBeDeleted() {
    return m_base_sinkIsSafeToDelete;
}


uint64_t TcpclV4BundleSink::GetRemoteNodeId() const {
    return m_base_tcpclRemoteNodeId;
}

void TcpclV4BundleSink::TrySendOpportunisticBundleIfAvailable_FromIoServiceThread() {
    if (m_base_shutdownCalled || m_base_sinkIsSafeToDelete) {
        std::cerr << "opportunistic link unavailable" << std::endl;
        return;
    }
    std::pair<std::unique_ptr<zmq::message_t>, std::vector<uint8_t> > bundleDataPair;
    const std::size_t totalBundlesUnacked = m_base_totalBundlesSent - m_base_totalBundlesAcked; //same as Virtual_GetTotalBundlesUnacked
    if ((totalBundlesUnacked < M_BASE_MY_MAX_TX_UNACKED_BUNDLES) && m_tryGetOpportunisticDataFunction && m_tryGetOpportunisticDataFunction(bundleDataPair)) {
        BaseClass_Forward(bundleDataPair.first, bundleDataPair.second, static_cast<bool>(bundleDataPair.first));
    }
}

void TcpclV4BundleSink::SetTryGetOpportunisticDataFunction(const TryGetOpportunisticDataFunction_t & tryGetOpportunisticDataFunction) {
    m_tryGetOpportunisticDataFunction = tryGetOpportunisticDataFunction;
}
void TcpclV4BundleSink::SetNotifyOpportunisticDataAckedCallback(const NotifyOpportunisticDataAckedCallback_t & notifyOpportunisticDataAckedCallback) {
    m_notifyOpportunisticDataAckedCallback = notifyOpportunisticDataAckedCallback;
}

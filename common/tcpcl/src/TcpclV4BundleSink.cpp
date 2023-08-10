/**
 * @file TcpclV4BundleSink.cpp
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
 */

#include <boost/bind/bind.hpp>
#include <memory>
#include "TcpclV4BundleSink.h"
#include "Logger.h"
#include <boost/make_unique.hpp>
#include "Uri.h"
#include "ThreadNamer.h"

static constexpr hdtn::Logger::SubProcess subprocess = hdtn::Logger::SubProcess::none;

TcpclV4BundleSink::TcpclV4BundleSink(
#ifdef OPENSSL_SUPPORT_ENABLED
    std::shared_ptr<boost::asio::ssl::stream<boost::asio::ip::tcp::socket> > & sslStreamSharedPtr,
#else
    std::shared_ptr<boost::asio::ip::tcp::socket> & tcpSocketPtr,
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
    m_base_inductConnectionTelemetry.m_connectionName = sslStreamSharedPtr->next_layer().remote_endpoint().address().to_string()
        + ":" + boost::lexical_cast<std::string>(sslStreamSharedPtr->next_layer().remote_endpoint().port());
    m_base_inductConnectionTelemetry.m_inputName = std::string("*:") + boost::lexical_cast<std::string>(m_base_sslStreamSharedPtr->next_layer().local_endpoint().port());
    m_base_tcpAsyncSenderSslPtr = boost::make_unique<TcpAsyncSenderSsl>(m_base_sslStreamSharedPtr, m_base_ioServiceRef);
    m_base_tcpAsyncSenderSslPtr->SetOnFailedBundleVecSendCallback(m_base_onFailedBundleVecSendCallback);
    m_base_tcpAsyncSenderSslPtr->SetOnFailedBundleZmqSendCallback(m_base_onFailedBundleZmqSendCallback);
    m_base_tcpAsyncSenderSslPtr->SetUserAssignedUuid(m_base_userAssignedUuid);
#else
    m_base_tcpSocketPtr = tcpSocketPtr;
    m_base_inductConnectionTelemetry.m_connectionName = tcpSocketPtr->remote_endpoint().address().to_string()
        + ":" + boost::lexical_cast<std::string>(tcpSocketPtr->remote_endpoint().port());
    m_base_inductConnectionTelemetry.m_inputName = std::string("*:") + boost::lexical_cast<std::string>(tcpSocketPtr->local_endpoint().port());
    m_base_tcpAsyncSenderPtr = boost::make_unique<TcpAsyncSender>(m_base_tcpSocketPtr, m_tcpSocketIoServiceRef);
    m_base_tcpAsyncSenderPtr->SetOnFailedBundleVecSendCallback(m_base_onFailedBundleVecSendCallback);
    m_base_tcpAsyncSenderPtr->SetOnFailedBundleZmqSendCallback(m_base_onFailedBundleZmqSendCallback);
    m_base_tcpAsyncSenderPtr->SetUserAssignedUuid(m_base_userAssignedUuid);
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
            try {
                boost::this_thread::sleep(boost::posix_time::milliseconds(250));
            }
            catch (const boost::thread_resource_error&) {}
            catch (const boost::thread_interrupted&) {}
            catch (const boost::condition_error&) {}
            catch (const boost::lock_error&) {}
        }
    }

    m_mutexCb.lock();
    m_running = false; //thread stopping criteria
    m_mutexCb.unlock();
    m_conditionVariableCb.notify_one();

    if (m_threadCbReaderPtr) {
        try {
            m_threadCbReaderPtr->join();
            m_threadCbReaderPtr.reset(); //delete it
        }
        catch (const boost::thread_resource_error&) {
            LOG_ERROR(subprocess) << "error stopping TcpclV4BundleSink threadCbReader";
        }
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
        LOG_INFO(subprocess) << "SSL/TLS Handshake succeeded.. all transmissions shall be secure from this point";
        m_base_didSuccessfulSslHandshake = true;
        m_stateTcpReadActive = false; //must be false before calling TryStartTcpReceiveSecure
        TryStartTcpReceiveSecure();
        //BaseClass_SendSessionInit(); I am the passive entity and will send (from within my session init rx callback) a session init when i first receive a session init from the active entity (from bundle source)
    }
    else {
        LOG_ERROR(subprocess) << "SSL/TLS Handshake failed: " << error.message();
        BaseClass_DoTcpclShutdown(false, TCPCLV4_SESSION_TERMINATION_REASON_CODES::UNKNOWN, false);
    }
}

void TcpclV4BundleSink::TryStartTcpReceiveSecure() { //must run within Io Service Thread
    if ((!m_stateTcpReadActive) && (m_base_sslStreamSharedPtr)) {
        const unsigned int writeIndex = m_circularIndexBuffer.GetIndexForWrite(); //store the volatile
        if (writeIndex == CIRCULAR_INDEX_BUFFER_FULL) {
            if (!m_printedCbTooSmallNotice) {
                m_printedCbTooSmallNotice = true;
                LOG_INFO(subprocess) << "TcpclV4BundleSink::StartTcpReceive(): buffers full.. you might want to increase the circular buffer size for better performance!";
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
        m_mutexCb.lock();
        m_circularIndexBuffer.CommitWrite(); //write complete at this point
        m_mutexCb.unlock();
        m_conditionVariableCb.notify_one();
        m_stateTcpReadActive = false; //must be false before calling TryStartTcpReceive
        TryStartTcpReceiveSecure(); //restart operation only if there was no error
    }
    else if (error == boost::asio::error::eof) {
        LOG_INFO(subprocess) << "TcpclBundleSink Tcp connection closed cleanly by peer";
        BaseClass_DoTcpclShutdown(false, TCPCLV4_SESSION_TERMINATION_REASON_CODES::UNKNOWN, false);
    }
    else if (error != boost::asio::error::operation_aborted) { //will always be operation_aborted when thread is terminating
        LOG_ERROR(subprocess) << "TcpclV4BundleSink::HandleTcpReceiveSomeSecure: " << error.message();
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
        if (writeIndex == CIRCULAR_INDEX_BUFFER_FULL) {
            if (!m_printedCbTooSmallNotice) {
                m_printedCbTooSmallNotice = true;
                LOG_INFO(subprocess) << "TcpclV4BundleSink::StartTcpReceive(): buffers full.. you might want to increase the circular buffer size for better performance!";
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
        m_mutexCb.lock();
        m_circularIndexBuffer.CommitWrite(); //write complete at this point
        m_mutexCb.unlock();
        m_conditionVariableCb.notify_one();
        m_stateTcpReadActive = false; //must be false before calling TryStartTcpReceive
        TryStartTcpReceiveUnsecure(); //restart operation only if there was no error
    }
    else if (error == boost::asio::error::eof) {
        LOG_INFO(subprocess) << "TcpclBundleSink Tcp connection closed cleanly by peer";
        BaseClass_DoTcpclShutdown(false, TCPCLV4_SESSION_TERMINATION_REASON_CODES::UNKNOWN, false);
    }
    else if (error != boost::asio::error::operation_aborted) { //will always be operation_aborted when thread is terminating
        LOG_ERROR(subprocess) << "TcpclV4BundleSink::HandleTcpReceiveSome: " << error.message();
    }
}

void TcpclV4BundleSink::PopCbThreadFunc() {
    ThreadNamer::SetThisThreadName("TcpclV4BundleSinkCbReader");

    boost::function<void()> tryStartTcpReceiveFunction = boost::bind(&TcpclV4BundleSink::TryStartTcpReceiveUnsecure, this);

    while (true) { //keep thread alive if running or cb not empty, i.e. "while (m_running || (m_circularIndexBuffer.GetIndexForRead() != CIRCULAR_INDEX_BUFFER_EMPTY))"
        unsigned int consumeIndex = m_circularIndexBuffer.GetIndexForRead(); //store the volatile
        boost::asio::post(m_tcpSocketIoServiceRef, tryStartTcpReceiveFunction); //keep this a thread safe operation by letting ioService thread run it
        if (consumeIndex == CIRCULAR_INDEX_BUFFER_EMPTY) { //if empty
            //try again, but with the mutex
            boost::mutex::scoped_lock lock(m_mutexCb);
            consumeIndex = m_circularIndexBuffer.GetIndexForRead(); //store the volatile
            if (consumeIndex == CIRCULAR_INDEX_BUFFER_EMPTY) { //if empty again (lock mutex (above) before checking condition)
                if (!m_running.load(std::memory_order_acquire)) { //m_running is mutex protected, if it stopped running, exit the thread (lock mutex (above) before checking condition)
                    break; //thread stopping criteria (empty and not running)
                }
                m_conditionVariableCb.wait(lock); // call lock.unlock() and blocks the current thread
                //thread is now unblocked, and the lock is reacquired by invoking lock.lock()
                continue;
            }
        }
        m_base_dataReceivedServedAsKeepaliveReceived = true;
        m_base_tcpclV4RxStateMachine.HandleReceivedChars(m_tcpReceiveBuffersCbVec[consumeIndex].data(), m_tcpReceiveBytesTransferredCbVec[consumeIndex]);
        m_circularIndexBuffer.CommitRead();
#ifdef OPENSSL_SUPPORT_ENABLED
        if (m_base_doUpgradeSocketToSsl) { //the tcpclv4 rx state machine may have set m_base_doUpgradeSocketToSsl to true after HandleReceivedChars()
            LOG_INFO(subprocess) << "sink going to call handshake";
            m_base_doUpgradeSocketToSsl = false;
            tryStartTcpReceiveFunction = boost::bind(&TcpclV4BundleSink::TryStartTcpReceiveSecure, this);
            //boost::asio::post(m_tcpSocketIoServiceRef, boost::bind(&TcpclV4BundleSink::DoSslUpgrade, this)); //keep this a thread safe operation by letting ioService thread run it
        }
#endif
    }

    LOG_INFO(subprocess) << "TcpclBundleSink Circular buffer reader thread exiting";

}

void TcpclV4BundleSink::Virtual_OnTcpSendSuccessful_CalledFromIoServiceThread() {
    ////TrySendOpportunisticBundleIfAvailable_FromIoServiceThread();
}
void TcpclV4BundleSink::Virtual_OnTcpSendContactHeaderSuccessful_CalledFromIoServiceThread() {
#ifdef OPENSSL_SUPPORT_ENABLED
    if (m_base_usingTls) {
        DoSslUpgrade();
    }
#endif
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

void TcpclV4BundleSink::Virtual_WholeBundleReady(padded_vector_uint8_t & wholeBundleVec) {
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
        LOG_ERROR(subprocess) << "opportunistic link unavailable";
        return;
    }
    std::pair<std::unique_ptr<zmq::message_t>, padded_vector_uint8_t> bundleDataPair;
    const std::size_t totalBundlesUnacked = m_base_outductTelemetry.m_totalBundlesSent - m_base_outductTelemetry.m_totalBundlesAcked; //same as Virtual_GetTotalBundlesUnacked
    if ((totalBundlesUnacked < M_BASE_MY_MAX_TX_UNACKED_BUNDLES) && m_tryGetOpportunisticDataFunction && m_tryGetOpportunisticDataFunction(bundleDataPair)) {
        BaseClass_Forward(bundleDataPair.first, bundleDataPair.second, static_cast<bool>(bundleDataPair.first), std::vector<uint8_t>());
    }
}

void TcpclV4BundleSink::SetTryGetOpportunisticDataFunction(const TryGetOpportunisticDataFunction_t & tryGetOpportunisticDataFunction) {
    m_tryGetOpportunisticDataFunction = tryGetOpportunisticDataFunction;
}
void TcpclV4BundleSink::SetNotifyOpportunisticDataAckedCallback(const NotifyOpportunisticDataAckedCallback_t & notifyOpportunisticDataAckedCallback) {
    m_notifyOpportunisticDataAckedCallback = notifyOpportunisticDataAckedCallback;
}

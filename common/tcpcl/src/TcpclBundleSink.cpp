#include <boost/bind/bind.hpp>
#include <boost/make_shared.hpp>
#include <iostream>
#include "TcpclBundleSink.h"
#include <boost/make_unique.hpp>
#include "Uri.h"

TcpclBundleSink::TcpclBundleSink(
    const uint16_t desiredKeepAliveIntervalSeconds, //new
    boost::shared_ptr<boost::asio::ip::tcp::socket> & tcpSocketPtr,
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

    TcpclV3BidirectionalLink(
        "TcpclV3BundleSink",
        3, //bundleSink shutdown message shall send 3 second reconnection delay to prevent disabling hdtn outduct reconnect
        true, //bundleSink shall delete socket after shutdown
        true, //contactHeaderMustReply
        desiredKeepAliveIntervalSeconds,
        &tcpSocketIoServiceRef,
        maxUnacked,
        maxBundleSizeBytes,
        maxFragmentSize,
        myNodeId,
        "" //expectedRemoteEidUri empty => don't check remote connections to this sink (allow all ipn's)
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
    m_base_tcpSocketPtr = tcpSocketPtr;

    for (unsigned int i = 0; i < M_NUM_CIRCULAR_BUFFER_VECTORS; ++i) {
        m_tcpReceiveBuffersCbVec[i].resize(M_CIRCULAR_BUFFER_BYTES_PER_VECTOR);
    }

    m_base_tcpAsyncSenderPtr = boost::make_unique<TcpAsyncSender>(m_base_tcpSocketPtr, m_tcpSocketIoServiceRef);
    
    m_running = true;
    m_threadCbReaderPtr = boost::make_unique<boost::thread>(
        boost::bind(&TcpclBundleSink::PopCbThreadFunc, this)); //create and start the worker thread

    TryStartTcpReceive();
}

TcpclBundleSink::~TcpclBundleSink() {
    //prevent TcpclBundleSink Opportunistic sending of bundles from exiting before all bundles sent and acked
    BaseClass_TryToWaitForAllBundlesToFinishSending();

    if (!m_base_sinkIsSafeToDelete) {
        BaseClass_DoTcpclShutdown(true, false);
        while (!m_base_sinkIsSafeToDelete) {
            boost::this_thread::sleep(boost::posix_time::milliseconds(250));
        }
    }

    m_running = false; //thread stopping criteria

    if (m_threadCbReaderPtr) {
        m_threadCbReaderPtr->join();
        m_threadCbReaderPtr.reset(); //delete it
    }
    m_base_tcpAsyncSenderPtr.reset();

    //print stats
    std::cout << "TcpclV3 Bundle Sink totalBundlesAcked " << m_base_totalBundlesAcked << std::endl;
    std::cout << "TcpclV3 Bundle Sink totalBytesAcked " << m_base_totalBytesAcked << std::endl;
    std::cout << "TcpclV3 Bundle Sink totalBundlesSent " << m_base_totalBundlesSent << std::endl;
    std::cout << "TcpclV3 Bundle Sink totalFragmentedAcked " << m_base_totalFragmentedAcked << std::endl;
    std::cout << "TcpclV3 Bundle Sink totalFragmentedSent " << m_base_totalFragmentedSent << std::endl;
    std::cout << "TcpclV3 Bundle Sink totalBundleBytesSent " << m_base_totalBundleBytesSent << std::endl;
}




void TcpclBundleSink::TryStartTcpReceive() {
    if ((!m_stateTcpReadActive) && (m_base_tcpSocketPtr)) {
        const unsigned int writeIndex = m_circularIndexBuffer.GetIndexForWrite(); //store the volatile
        if (writeIndex == CIRCULAR_INDEX_BUFFER_FULL) {
            if (!m_printedCbTooSmallNotice) {
                m_printedCbTooSmallNotice = true;
                std::cout << "notice in TcpclBundleSink::StartTcpReceive(): buffers full.. you might want to increase the circular buffer size for better performance!" << std::endl;
            }
        }
        else {
            m_stateTcpReadActive = true;
            m_base_tcpSocketPtr->async_read_some(
                boost::asio::buffer(m_tcpReceiveBuffersCbVec[writeIndex]),
                boost::bind(&TcpclBundleSink::HandleTcpReceiveSome, this,
                    boost::asio::placeholders::error,
                    boost::asio::placeholders::bytes_transferred,
                    writeIndex));
        }
    }
}
void TcpclBundleSink::HandleTcpReceiveSome(const boost::system::error_code & error, std::size_t bytesTransferred, unsigned int writeIndex) {
    if (!error) {
        m_tcpReceiveBytesTransferredCbVec[writeIndex] = bytesTransferred;
        m_circularIndexBuffer.CommitWrite(); //write complete at this point
        m_stateTcpReadActive = false; //must be false before calling TryStartTcpReceive
        m_conditionVariableCb.notify_one();
        TryStartTcpReceive(); //restart operation only if there was no error
    }
    else if (error == boost::asio::error::eof) {
        std::cout << "TcpclBundleSink Tcp connection closed cleanly by peer" << std::endl;
        BaseClass_DoTcpclShutdown(false, false);
    }
    else if (error != boost::asio::error::operation_aborted) { //will always be operation_aborted when thread is terminating
        std::cerr << "Error in TcpclBundleSink::HandleTcpReceiveSome: " << error.message() << std::endl;
    }
}

void TcpclBundleSink::PopCbThreadFunc() {

    boost::mutex localMutex;
    boost::mutex::scoped_lock lock(localMutex);

    while (m_running || (m_circularIndexBuffer.GetIndexForRead() != CIRCULAR_INDEX_BUFFER_EMPTY)) { //keep thread alive if running or cb not empty


        const unsigned int consumeIndex = m_circularIndexBuffer.GetIndexForRead(); //store the volatile
        boost::asio::post(m_tcpSocketIoServiceRef, boost::bind(&TcpclBundleSink::TryStartTcpReceive, this)); //keep this a thread safe operation by letting ioService thread run it
        if (consumeIndex == CIRCULAR_INDEX_BUFFER_EMPTY) { //if empty
            m_conditionVariableCb.timed_wait(lock, boost::posix_time::milliseconds(10)); // call lock.unlock() and blocks the current thread
            //thread is now unblocked, and the lock is reacquired by invoking lock.lock()
            continue;
        }
        m_base_dataReceivedServedAsKeepaliveReceived = true;
        m_base_tcpclV3RxStateMachine.HandleReceivedChars(m_tcpReceiveBuffersCbVec[consumeIndex].data(), m_tcpReceiveBytesTransferredCbVec[consumeIndex]);

        m_circularIndexBuffer.CommitRead();
    }

    std::cout << "TcpclBundleSink Circular buffer reader thread exiting\n";

}

void TcpclBundleSink::Virtual_OnTcpSendSuccessful_CalledFromIoServiceThread() {
    ////TrySendOpportunisticBundleIfAvailable_FromIoServiceThread();
}

void TcpclBundleSink::Virtual_OnContactHeaderCompletedSuccessfully() {
    if (m_onContactHeaderCallback) {
        m_onContactHeaderCallback(this);
    }
}


void TcpclBundleSink::Virtual_OnTcpclShutdownComplete_CalledFromIoServiceThread() {
    if (m_notifyReadyToDeleteCallback) {
        m_notifyReadyToDeleteCallback();
    }
}

void TcpclBundleSink::Virtual_OnSuccessfulWholeBundleAcknowledged() {
    if (m_notifyOpportunisticDataAckedCallback) {
        m_notifyOpportunisticDataAckedCallback();
    }
}

void TcpclBundleSink::Virtual_WholeBundleReady(padded_vector_uint8_t & wholeBundleVec) {
    m_wholeBundleReadyCallback(wholeBundleVec);
}


bool TcpclBundleSink::ReadyToBeDeleted() {
    return m_base_sinkIsSafeToDelete;
}


uint64_t TcpclBundleSink::GetRemoteNodeId() const {
    return m_base_tcpclRemoteNodeId;
}

void TcpclBundleSink::TrySendOpportunisticBundleIfAvailable_FromIoServiceThread() {
    if (m_base_shutdownCalled || m_base_sinkIsSafeToDelete) {
        std::cerr << "opportunistic link unavailable" << std::endl;
        return;
    }
    std::pair<std::unique_ptr<zmq::message_t>, std::vector<uint8_t> > bundleDataPair;
    const std::size_t totalBundlesUnacked = m_base_totalBundlesSent - m_base_totalBundlesAcked; //same as Virtual_GetTotalBundlesUnacked
    if ((totalBundlesUnacked < M_BASE_MAX_UNACKED_BUNDLES_IN_PIPELINE) && m_tryGetOpportunisticDataFunction && m_tryGetOpportunisticDataFunction(bundleDataPair)) {
        BaseClass_Forward(bundleDataPair.first, bundleDataPair.second, static_cast<bool>(bundleDataPair.first));
    }
}

void TcpclBundleSink::SetTryGetOpportunisticDataFunction(const TryGetOpportunisticDataFunction_t & tryGetOpportunisticDataFunction) {
    m_tryGetOpportunisticDataFunction = tryGetOpportunisticDataFunction;
}
void TcpclBundleSink::SetNotifyOpportunisticDataAckedCallback(const NotifyOpportunisticDataAckedCallback_t & notifyOpportunisticDataAckedCallback) {
    m_notifyOpportunisticDataAckedCallback = notifyOpportunisticDataAckedCallback;
}

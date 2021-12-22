#include <string>
#include <iostream>
#include "TcpclV4BundleSource.h"
#include <boost/lexical_cast.hpp>
#include <boost/make_shared.hpp>
#include <boost/make_unique.hpp>
#include "Uri.h"

TcpclV4BundleSource::TcpclV4BundleSource(const uint16_t desiredKeepAliveIntervalSeconds, const uint64_t myNodeId,
    const std::string & expectedRemoteEidUri, const unsigned int maxUnacked, const uint64_t maxFragmentSize,
    const OutductOpportunisticProcessReceivedBundleCallback_t & outductOpportunisticProcessReceivedBundleCallback) :

    TcpclV4BidirectionalLink(
        "TcpclV4BundleSource",
        0, //shutdown message shall send 0 meaning infinite reconnection delay (sink shall not try to reconnect)
        false, //bundleSource shall NOT delete socket after shutdown
        true, //isActiveEntity,
        desiredKeepAliveIntervalSeconds,
        NULL, // NULL will create a local io_service
        maxUnacked,
        maxFragmentSize,
        100000000, //todo 100MB maxBundleSizeBytes for receive
        myNodeId,
        expectedRemoteEidUri,
        false, //tryUseTls
        false //tlsIsRequired
    ),
m_work(m_base_ioServiceRef), //prevent stopping of ioservice until destructor
m_resolver(m_base_ioServiceRef),
m_reconnectAfterShutdownTimer(m_base_ioServiceRef),
m_reconnectAfterOnConnectErrorTimer(m_base_ioServiceRef),
m_outductOpportunisticProcessReceivedBundleCallback(outductOpportunisticProcessReceivedBundleCallback),
m_tcpReadSomeBufferVec(10000) //todo 10KB rx buffer
{
    m_ioServiceThreadPtr = boost::make_unique<boost::thread>(boost::bind(&boost::asio::io_service::run, &m_base_ioServiceRef));
}

TcpclV4BundleSource::~TcpclV4BundleSource() {
    Stop();
}

void TcpclV4BundleSource::Stop() {
    //prevent TcpclBundleSource from exiting before all bundles sent and acked
    BaseClass_TryToWaitForAllBundlesToFinishSending();

    BaseClass_DoTcpclShutdown(true, TCPCLV4_SESSION_TERMINATION_REASON_CODES::UNKNOWN, false);
    while (!m_base_tcpclShutdownComplete) {
        boost::this_thread::sleep(boost::posix_time::milliseconds(250));
    }
    m_base_tcpAsyncSenderPtr.reset(); //stop this first
    m_base_ioServiceRef.stop(); //ioservice requires stopping before join because of the m_work object

    if(m_ioServiceThreadPtr) {
        m_ioServiceThreadPtr->join();
        m_ioServiceThreadPtr.reset(); //delete it
    }

    //print stats
    std::cout << "TcpclV4 Bundle Source totalBundlesAcked " << m_base_totalBundlesAcked << std::endl;
    std::cout << "TcpclV4 Bundle Source totalBytesAcked " << m_base_totalBytesAcked << std::endl;
    std::cout << "TcpclV4 Bundle Source totalBundlesSent " << m_base_totalBundlesSent << std::endl;
    std::cout << "TcpclV4 Bundle Source totalFragmentedAcked " << m_base_totalFragmentedAcked << std::endl;
    std::cout << "TcpclV4 Bundle Source totalFragmentedSent " << m_base_totalFragmentedSent << std::endl;
    std::cout << "TcpclV4 Bundle Source totalBundleBytesSent " << m_base_totalBundleBytesSent << std::endl;
}



void TcpclV4BundleSource::Connect(const std::string & hostname, const std::string & port) {

    boost::asio::ip::tcp::resolver::query query(hostname, port);
    m_resolver.async_resolve(query, boost::bind(&TcpclV4BundleSource::OnResolve,
                                                this, boost::asio::placeholders::error,
                                                boost::asio::placeholders::results));
}

void TcpclV4BundleSource::OnResolve(const boost::system::error_code & ec, boost::asio::ip::tcp::resolver::results_type results) { // Resolved endpoints as a range.
    if(ec) {
        std::cerr << "Error resolving: " << ec.message() << std::endl;
    }
    else {
        std::cout << "resolved host to " << results->endpoint().address() << ":" << results->endpoint().port() << ".  Connecting..." << std::endl;
        m_base_tcpSocketPtr = boost::make_shared<boost::asio::ip::tcp::socket>(m_base_ioServiceRef);
        m_resolverResults = results;
        boost::asio::async_connect(
            *m_base_tcpSocketPtr,
            m_resolverResults,
            boost::bind(
                &TcpclV4BundleSource::OnConnect,
                this,
                boost::asio::placeholders::error));
    }
}

void TcpclV4BundleSource::OnConnect(const boost::system::error_code & ec) {

    if (ec) {
        if (ec != boost::asio::error::operation_aborted) {
            std::cerr << "Error in OnConnect: " << ec.value() << " " << ec.message() << "\n";
            std::cout << "Will try to reconnect after 2 seconds" << std::endl;
            m_reconnectAfterOnConnectErrorTimer.expires_from_now(boost::posix_time::seconds(2));
            m_reconnectAfterOnConnectErrorTimer.async_wait(boost::bind(&TcpclV4BundleSource::OnReconnectAfterOnConnectError_TimerExpired, this, boost::asio::placeholders::error));
        }
        return;
    }

    std::cout << "connected.. sending contact header..\n";
    m_base_tcpclShutdownComplete = false;

    
    if(m_base_tcpSocketPtr) {
        m_base_tcpAsyncSenderPtr = boost::make_unique<TcpAsyncSender>(m_base_tcpSocketPtr, m_base_ioServiceRef);

        TcpAsyncSenderElement * el = new TcpAsyncSenderElement();
        el->m_underlyingData.resize(1);
        TcpclV4::GenerateContactHeader(el->m_underlyingData[0], M_BASE_TRY_USE_TLS);
        el->m_constBufferVec.emplace_back(boost::asio::buffer(el->m_underlyingData[0])); //only one element so resize not needed
        el->m_onSuccessfulSendCallbackByIoServiceThreadPtr = &m_base_handleTcpSendCallback;
        m_base_tcpAsyncSenderPtr->AsyncSend_ThreadSafe(el);

        StartTcpReceive();
    }
}

void TcpclV4BundleSource::OnReconnectAfterOnConnectError_TimerExpired(const boost::system::error_code& e) {
    if (e != boost::asio::error::operation_aborted) {
        // Timer was not cancelled, take necessary action.
        std::cout << "TcpclV4BundleSource Trying to reconnect..." << std::endl;
        boost::asio::async_connect(
            *m_base_tcpSocketPtr,
            m_resolverResults,
            boost::bind(
                &TcpclV4BundleSource::OnConnect,
                this,
                boost::asio::placeholders::error));
    }
}




void TcpclV4BundleSource::StartTcpReceive() {
    m_base_tcpSocketPtr->async_read_some(
        boost::asio::buffer(m_tcpReadSomeBufferVec),
        boost::bind(&TcpclV4BundleSource::HandleTcpReceiveSome, this,
                    boost::asio::placeholders::error,
                    boost::asio::placeholders::bytes_transferred));
}
void TcpclV4BundleSource::HandleTcpReceiveSome(const boost::system::error_code & error, std::size_t bytesTransferred) {
    if (!error) {
        //std::cout << "received " << bytesTransferred << "\n";

        //because TcpclBundleSource will not receive much data from the destination,
        //a separate thread is not needed to process it, but rather this
        //io_service thread will do the processing
        m_base_tcpclV4RxStateMachine.HandleReceivedChars(m_tcpReadSomeBufferVec.data(), bytesTransferred);
        StartTcpReceive(); //restart operation only if there was no error
    }
    else if (error == boost::asio::error::eof) {
        std::cout << "Tcp connection closed cleanly by peer" << std::endl;
        BaseClass_DoTcpclShutdown(false, TCPCLV4_SESSION_TERMINATION_REASON_CODES::UNKNOWN, false);
    }
    else if (error != boost::asio::error::operation_aborted) { //will always be operation_aborted when thread is terminating
        std::cerr << "Error in TcpclV4BundleSource::HandleTcpReceiveSome: " << error.message() << std::endl;
    }
}














void TcpclV4BundleSource::Virtual_OnTcpclShutdownComplete_CalledFromIoServiceThread() {
    if (m_base_reconnectionDelaySecondsIfNotZero) {
        m_reconnectAfterShutdownTimer.expires_from_now(boost::posix_time::seconds(m_base_reconnectionDelaySecondsIfNotZero));
        m_reconnectAfterShutdownTimer.async_wait(boost::bind(&TcpclV4BundleSource::OnNeedToReconnectAfterShutdown_TimerExpired, this, boost::asio::placeholders::error));
    }
}

void TcpclV4BundleSource::Virtual_OnSuccessfulWholeBundleAcknowledged() {
    if (m_onSuccessfulAckCallback) {
        m_onSuccessfulAckCallback();
    }
}

//for when tcpclAllowOpportunisticReceiveBundles is set to true (not designed for extremely high throughput)
void TcpclV4BundleSource::Virtual_WholeBundleReady(std::vector<uint8_t> & wholeBundleVec) {
    if (m_outductOpportunisticProcessReceivedBundleCallback) {
        m_outductOpportunisticProcessReceivedBundleCallback(wholeBundleVec);
    }
    else {
        std::cout << "TcpclV4BundleSource should never enter DataSegmentCallback if tcpclAllowOpportunisticReceiveBundles is set to false" << std::endl;
    }
}



void TcpclV4BundleSource::OnNeedToReconnectAfterShutdown_TimerExpired(const boost::system::error_code& e) {
    if (e != boost::asio::error::operation_aborted) {
        // Timer was not cancelled, take necessary action.
        std::cout << "Trying to reconnect..." << std::endl;
        m_base_tcpAsyncSenderPtr.reset();
        m_base_tcpSocketPtr = boost::make_shared<boost::asio::ip::tcp::socket>(m_base_ioServiceRef);
        m_base_shutdownCalled = false;
        boost::asio::async_connect(
            *m_base_tcpSocketPtr,
            m_resolverResults,
            boost::bind(
                &TcpclV4BundleSource::OnConnect,
                this,
                boost::asio::placeholders::error));
    }
}

bool TcpclV4BundleSource::ReadyToForward() {
    return m_base_readyToForward;
}

void TcpclV4BundleSource::SetOnSuccessfulAckCallback(const OnSuccessfulAckCallback_t & callback) {
    m_onSuccessfulAckCallback = callback;
}

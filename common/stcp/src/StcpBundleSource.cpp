/**
 * @file StcpBundleSource.cpp
 * @author  Brian Tomko <brian.j.tomko@nasa.gov>
 *
 * @copyright Copyright � 2021 United States Government as represented by
 * the National Aeronautics and Space Administration.
 * No copyright is claimed in the United States under Title 17, U.S.Code.
 * All Other Rights Reserved.
 *
 * @section LICENSE
 * Released under the NASA Open Source Agreement (NOSA)
 * See LICENSE.md in the source root directory for more information.
 */

#include <string>
#include "Logger.h"
#include "StcpBundleSource.h"
#include <boost/lexical_cast.hpp>
#include <memory>
#include <boost/endian/conversion.hpp>
#include <boost/make_unique.hpp>

#define RECONNECTION_DELAY_AFTER_SHUTDOWN_SECONDS 3

static constexpr hdtn::Logger::SubProcess subprocess = hdtn::Logger::SubProcess::none;

StcpBundleSource::StcpBundleSource(const uint16_t desiredKeeAliveIntervlSeconds, const unsigned int maxUnacked) :
m_work(m_ioService), //prevent stopping of ioservice until destructor
m_resolver(m_ioService),
m_needToSendKeepAliveMessageTimer(m_ioService),
m_reconnectAfterShutdownTimer(m_ioService),
m_reconnectAfterOnConnectErrorTimer(m_ioService),
M_KEEP_ALIVE_INTERVAL_SECONDS(desiredKeeAliveIntervlSeconds),
MAX_UNACKED(maxUnacked),
m_bytesToAckByTcpSendCallbackCb(MAX_UNACKED),
m_bytesToAckByTcpSendCallbackCbVec(MAX_UNACKED),
m_readyToForward(false),
m_stcpShutdownComplete(true),
m_dataServedAsKeepAlive(true),
m_useLocalConditionVariableAckReceived(false), //for destructor only
m_userAssignedUuid(0),

m_stcpOutductTelemetry()


{
    m_handleTcpSendCallback = boost::bind(&StcpBundleSource::HandleTcpSend, this, boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred, boost::placeholders::_3);
    m_handleTcpSendKeepAliveCallback = boost::bind(&StcpBundleSource::HandleTcpSendKeepAlive, this, boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred, boost::placeholders::_3);

    m_ioServiceThreadPtr = boost::make_unique<boost::thread>(boost::bind(&boost::asio::io_service::run, &m_ioService));

}

StcpBundleSource::~StcpBundleSource() {
    Stop();
}

void StcpBundleSource::Stop() {
    //prevent StcpBundleSource from exiting before all bundles sent and acked
    boost::mutex localMutex;
    boost::mutex::scoped_lock lock(localMutex);
    m_useLocalConditionVariableAckReceived = true;
    std::size_t previousUnacked = std::numeric_limits<std::size_t>::max();
    for (unsigned int attempt = 0; attempt < 20; ++attempt) {
        const std::size_t numUnacked = GetTotalDataSegmentsUnacked();
        if (numUnacked) {
            LOG_INFO(subprocess) << "StcpBundleSource destructor waiting on " << numUnacked << " unacked bundles";


            if (previousUnacked > numUnacked) {
                previousUnacked = numUnacked;
                attempt = 0;
            }
            m_localConditionVariableAckReceived.timed_wait(lock, boost::posix_time::milliseconds(500)); // call lock.unlock() and blocks the current thread
            //thread is now unblocked, and the lock is reacquired by invoking lock.lock()
            continue;
        }
        break;
    }

    DoStcpShutdown(0);
    while (!m_stcpShutdownComplete) {
        boost::this_thread::sleep(boost::posix_time::milliseconds(250));
    }

    m_tcpAsyncSenderPtr.reset(); //stop this first
    //This function does not block, but instead simply signals the io_service to stop
    //All invocations of its run() or run_one() member functions should return as soon as possible.
    //Subsequent calls to run(), run_one(), poll() or poll_one() will return immediately until reset() is called.
    m_ioService.stop(); //ioservice requires stopping before join because of the m_work object

    if(m_ioServiceThreadPtr) {
        m_ioServiceThreadPtr->join();
        m_ioServiceThreadPtr.reset(); //delete it
    }

    //print stats
    LOG_INFO(subprocess) << "m_stcpOutductTelemetry.totalBundlesSent " << m_stcpOutductTelemetry.totalBundlesSent;
    LOG_INFO(subprocess) << "m_stcpOutductTelemetry.totalBundlesAcked " << m_stcpOutductTelemetry.totalBundlesAcked;
    LOG_INFO(subprocess) << "m_stcpOutductTelemetry.totalBundleBytesSent " << m_stcpOutductTelemetry.totalBundleBytesSent;
    LOG_INFO(subprocess) << "m_stcpOutductTelemetry.totalStcpBytesSent " << m_stcpOutductTelemetry.totalStcpBytesSent;
    LOG_INFO(subprocess) << "m_stcpOutductTelemetry.totalBundleBytesAcked " << m_stcpOutductTelemetry.totalBundleBytesAcked;
}

//An STCP protocol data unit (SPDU) is simply a serialized bundle
//preceded by an integer indicating the length of that serialized
//bundle.
void StcpBundleSource::GenerateDataUnit(std::vector<uint8_t> & dataUnit, const uint8_t * contents, uint32_t sizeContents) {
    uint32_t sizeContentsBigEndian = sizeContents;
    boost::endian::native_to_big_inplace(sizeContentsBigEndian);
    
   
    dataUnit.resize(sizeof(uint32_t) + sizeContents);

    memcpy(&dataUnit[0], &sizeContentsBigEndian, sizeof(uint32_t));
    if (sizeContents) {
        memcpy(&dataUnit[sizeof(uint32_t)], contents, sizeContents);
    }
}

void StcpBundleSource::GenerateDataUnitHeaderOnly(std::vector<uint8_t> & dataUnit, uint32_t sizeContents) {
    uint32_t sizeContentsBigEndian = sizeContents;
    boost::endian::native_to_big_inplace(sizeContentsBigEndian);


    dataUnit.resize(sizeof(uint32_t));

    memcpy(&dataUnit[0], &sizeContentsBigEndian, sizeof(uint32_t));
}




bool StcpBundleSource::Forward(zmq::message_t & dataZmq, std::vector<uint8_t>&& userData) {
    if (!m_readyToForward) {
        LOG_ERROR(subprocess) << "link not ready to forward yet";
        return false;
    }


    const unsigned int writeIndexTcpSendCallback = m_bytesToAckByTcpSendCallbackCb.GetIndexForWrite(); //don't put this in tcp async write callback
    if (writeIndexTcpSendCallback == CIRCULAR_INDEX_BUFFER_FULL) { //push check
        LOG_ERROR(subprocess) << "StcpBundleSource::Forward.. too many unacked packets by tcp send callback";
        return false;
    }


    ++m_stcpOutductTelemetry.totalBundlesSent;
    m_stcpOutductTelemetry.totalBundleBytesSent += dataZmq.size();

    const uint32_t dataUnitSize = static_cast<uint32_t>(dataZmq.size() + sizeof(uint32_t));
    m_stcpOutductTelemetry.totalStcpBytesSent += dataUnitSize;


    m_bytesToAckByTcpSendCallbackCbVec[writeIndexTcpSendCallback] = dataUnitSize;
    m_bytesToAckByTcpSendCallbackCb.CommitWrite(); //pushed

    m_dataServedAsKeepAlive = true;


    TcpAsyncSenderElement * el = new TcpAsyncSenderElement();
    el->m_userData = std::move(userData);
    el->m_underlyingDataVecHeaders.resize(1);
    StcpBundleSource::GenerateDataUnitHeaderOnly(el->m_underlyingDataVecHeaders[0], static_cast<uint32_t>(dataZmq.size()));
    el->m_underlyingDataZmqBundle = boost::make_unique<zmq::message_t>(std::move(dataZmq));
    el->m_constBufferVec.resize(2);
    el->m_constBufferVec[0] = boost::asio::buffer(el->m_underlyingDataVecHeaders[0]);
    el->m_constBufferVec[1] = boost::asio::buffer(el->m_underlyingDataZmqBundle->data(), el->m_underlyingDataZmqBundle->size());
    el->m_onSuccessfulSendCallbackByIoServiceThreadPtr = &m_handleTcpSendCallback;
    m_tcpAsyncSenderPtr->AsyncSend_ThreadSafe(el);

    return true;
}

bool StcpBundleSource::Forward(std::vector<uint8_t> & dataVec, std::vector<uint8_t>&& userData) {
    if (!m_readyToForward) {
        LOG_ERROR(subprocess) << "link not ready to forward yet";
        return false;
    }

    const unsigned int writeIndexTcpSendCallback = m_bytesToAckByTcpSendCallbackCb.GetIndexForWrite(); //don't put this in tcp async write callback
    if (writeIndexTcpSendCallback == CIRCULAR_INDEX_BUFFER_FULL) { //push check
        LOG_ERROR(subprocess) << "StcpBundleSource::Forward.. too many unacked packets by tcp send callback";
        return false;
    }



    ++m_stcpOutductTelemetry.totalBundlesSent;
    m_stcpOutductTelemetry.totalBundleBytesSent += dataVec.size();

    const uint32_t dataUnitSize = static_cast<uint32_t>(dataVec.size() + sizeof(uint32_t));
    m_stcpOutductTelemetry.totalStcpBytesSent += dataUnitSize;

    m_bytesToAckByTcpSendCallbackCbVec[writeIndexTcpSendCallback] = dataUnitSize;
    m_bytesToAckByTcpSendCallbackCb.CommitWrite(); //pushed


    m_dataServedAsKeepAlive = true;

    TcpAsyncSenderElement * el = new TcpAsyncSenderElement();
    el->m_userData = std::move(userData);
    el->m_underlyingDataVecHeaders.resize(1);
    StcpBundleSource::GenerateDataUnitHeaderOnly(el->m_underlyingDataVecHeaders[0], static_cast<uint32_t>(dataVec.size()));
    el->m_underlyingDataVecBundle = std::move(dataVec);
    el->m_constBufferVec.resize(2);
    el->m_constBufferVec[0] = boost::asio::buffer(el->m_underlyingDataVecHeaders[0]);
    el->m_constBufferVec[1] = boost::asio::buffer(el->m_underlyingDataVecBundle);
    el->m_onSuccessfulSendCallbackByIoServiceThreadPtr = &m_handleTcpSendCallback;
    m_tcpAsyncSenderPtr->AsyncSend_ThreadSafe(el);

    return true;
}


bool StcpBundleSource::Forward(const uint8_t* bundleData, const std::size_t size, std::vector<uint8_t>&& userData) {
    std::vector<uint8_t> vec(bundleData, bundleData + size);
    return Forward(vec, std::move(userData));
}


std::size_t StcpBundleSource::GetTotalDataSegmentsAcked() {
    return m_stcpOutductTelemetry.totalBundlesAcked;
}

std::size_t StcpBundleSource::GetTotalDataSegmentsSent() {
    return m_stcpOutductTelemetry.totalBundlesSent;
}

std::size_t StcpBundleSource::GetTotalDataSegmentsUnacked() {
    return m_stcpOutductTelemetry.GetTotalBundlesQueued();
}

std::size_t StcpBundleSource::GetTotalBundleBytesAcked() {
    return m_stcpOutductTelemetry.totalBundleBytesAcked;
}

std::size_t StcpBundleSource::GetTotalBundleBytesSent() {
    return m_stcpOutductTelemetry.totalBundleBytesSent;
}

std::size_t StcpBundleSource::GetTotalBundleBytesUnacked() {
    return m_stcpOutductTelemetry.GetTotalBundleBytesQueued();
}


void StcpBundleSource::Connect(const std::string & hostname, const std::string & port) {

    boost::asio::ip::tcp::resolver::query query(hostname, port);
    m_resolver.async_resolve(query, boost::bind(&StcpBundleSource::OnResolve,
                                                this, boost::asio::placeholders::error,
                                                boost::asio::placeholders::results));
}

void StcpBundleSource::OnResolve(const boost::system::error_code & ec, boost::asio::ip::tcp::resolver::results_type results) { // Resolved endpoints as a range.
    if(ec) {
        LOG_ERROR(subprocess) << "Error resolving: " << ec.message();
    }
    else {
        LOG_INFO(subprocess) << "resolved host to " << results->endpoint().address() << ":" << results->endpoint().port() << ".  Connecting...";
        m_tcpSocketPtr = std::make_shared<boost::asio::ip::tcp::socket>(m_ioService);
        m_resolverResults = results;
        boost::asio::async_connect(
            *m_tcpSocketPtr,
            m_resolverResults,
            boost::bind(
                &StcpBundleSource::OnConnect,
                this,
                boost::asio::placeholders::error));
    }
}

void StcpBundleSource::OnConnect(const boost::system::error_code & ec) {

    if (ec) {
        if (ec != boost::asio::error::operation_aborted) {
            LOG_ERROR(subprocess) << "OnConnect: " << ec.value() << " " << ec.message();
            LOG_ERROR(subprocess) << "Will try to reconnect after 2 seconds";
            m_reconnectAfterOnConnectErrorTimer.expires_from_now(boost::posix_time::seconds(2));
            m_reconnectAfterOnConnectErrorTimer.async_wait(boost::bind(&StcpBundleSource::OnReconnectAfterOnConnectError_TimerExpired, this, boost::asio::placeholders::error));
        }
        return;
    }

    LOG_INFO(subprocess) << "Stcp connection complete";
    m_stcpShutdownComplete = false;
    m_readyToForward = true;


    m_needToSendKeepAliveMessageTimer.expires_from_now(boost::posix_time::seconds(M_KEEP_ALIVE_INTERVAL_SECONDS));
    m_needToSendKeepAliveMessageTimer.async_wait(boost::bind(&StcpBundleSource::OnNeedToSendKeepAliveMessage_TimerExpired, this, boost::asio::placeholders::error));

    if(m_tcpSocketPtr) {
        m_tcpAsyncSenderPtr = boost::make_unique<TcpAsyncSender>(m_tcpSocketPtr, m_ioService);
        m_tcpAsyncSenderPtr->SetOnFailedBundleVecSendCallback(m_onFailedBundleVecSendCallback);
        m_tcpAsyncSenderPtr->SetOnFailedBundleZmqSendCallback(m_onFailedBundleZmqSendCallback);
        m_tcpAsyncSenderPtr->SetUserAssignedUuid(m_userAssignedUuid);
        StartTcpReceive();
        if (m_onOutductLinkStatusChangedCallback) { //let user know of link up event
            m_onOutductLinkStatusChangedCallback(false, m_userAssignedUuid);
        }
    }
}

void StcpBundleSource::OnReconnectAfterOnConnectError_TimerExpired(const boost::system::error_code& e) {
    if (e != boost::asio::error::operation_aborted) {
        // Timer was not cancelled, take necessary action.
        LOG_INFO(subprocess) << "Trying to reconnect...";
        boost::asio::async_connect(
            *m_tcpSocketPtr,
            m_resolverResults,
            boost::bind(
                &StcpBundleSource::OnConnect,
                this,
                boost::asio::placeholders::error));
    }
}



void StcpBundleSource::HandleTcpSend(const boost::system::error_code& error, std::size_t bytes_transferred, TcpAsyncSenderElement* elPtr) {
    if (error) {
        LOG_ERROR(subprocess) << "StcpBundleSource::HandleTcpSend: " << error.message();
        DoStcpShutdown(RECONNECTION_DELAY_AFTER_SHUTDOWN_SECONDS);
    }
    else {
        const unsigned int readIndex = m_bytesToAckByTcpSendCallbackCb.GetIndexForRead();
        if (readIndex == CIRCULAR_INDEX_BUFFER_EMPTY) { //empty
            LOG_ERROR(subprocess) << "AckCallback called with empty queue";
        }
        else if (m_bytesToAckByTcpSendCallbackCbVec[readIndex] == bytes_transferred) {
            ++m_stcpOutductTelemetry.totalBundlesAcked;
            m_stcpOutductTelemetry.totalBundleBytesAcked += m_bytesToAckByTcpSendCallbackCbVec[readIndex] - sizeof(uint32_t);
            m_bytesToAckByTcpSendCallbackCb.CommitRead();

            if (m_onSuccessfulBundleSendCallback) {
                m_onSuccessfulBundleSendCallback(elPtr->m_userData, m_userAssignedUuid);
            }
            if (m_useLocalConditionVariableAckReceived) {
                m_localConditionVariableAckReceived.notify_one();
            }
            
        }
        else {
            LOG_ERROR(subprocess) << "StcpBundleSource::HandleTcpSend: wrong bytes acked: expected " << m_bytesToAckByTcpSendCallbackCbVec[readIndex] << " but got " << bytes_transferred;
        }
    }
}

void StcpBundleSource::HandleTcpSendKeepAlive(const boost::system::error_code& error, std::size_t bytes_transferred, TcpAsyncSenderElement* elPtr) {
    if (error) {
        LOG_ERROR(subprocess) << "StcpBundleSource::HandleTcpSendKeepAlive: " << error.message();
        DoStcpShutdown(RECONNECTION_DELAY_AFTER_SHUTDOWN_SECONDS);
    }
    else {
        LOG_INFO(subprocess) << "keepalive packet sent";
    }
}

void StcpBundleSource::StartTcpReceive() {
    m_tcpSocketPtr->async_read_some(
        boost::asio::buffer(m_tcpReadSomeBuffer, 10),
        boost::bind(&StcpBundleSource::HandleTcpReceiveSome, this,
                    boost::asio::placeholders::error,
                    boost::asio::placeholders::bytes_transferred));
}
void StcpBundleSource::HandleTcpReceiveSome(const boost::system::error_code & error, std::size_t bytesTransferred) {
    if (!error) {
        LOG_ERROR(subprocess) << "StcpBundleSource::HandleTcpReceiveSome: received " << bytesTransferred << " but should never receive any data";

        //shutdown
    }
    else if (error == boost::asio::error::eof) {
        LOG_INFO(subprocess) << "Tcp connection closed cleanly by peer";
        DoStcpShutdown(RECONNECTION_DELAY_AFTER_SHUTDOWN_SECONDS);
        
    }
    else if (error != boost::asio::error::operation_aborted) {
        LOG_ERROR(subprocess) << "StcpBundleSource::HandleTcpReceiveSome: " << error.message();
        DoStcpShutdown(RECONNECTION_DELAY_AFTER_SHUTDOWN_SECONDS);
    }
}



void StcpBundleSource::OnNeedToSendKeepAliveMessage_TimerExpired(const boost::system::error_code& e) {
    if (e != boost::asio::error::operation_aborted) {
        // Timer was not cancelled, take necessary action.
        if (m_tcpSocketPtr) {
            m_needToSendKeepAliveMessageTimer.expires_from_now(boost::posix_time::seconds(M_KEEP_ALIVE_INTERVAL_SECONDS));
            m_needToSendKeepAliveMessageTimer.async_wait(boost::bind(&StcpBundleSource::OnNeedToSendKeepAliveMessage_TimerExpired, this, boost::asio::placeholders::error));
            //SEND KEEPALIVE PACKET
            if (!m_dataServedAsKeepAlive) {
                static const uint32_t keepAliveData = 0; //0 is the keep alive signal 

                TcpAsyncSenderElement * el = new TcpAsyncSenderElement();
                el->m_constBufferVec.emplace_back(boost::asio::buffer((const uint8_t *)&keepAliveData, sizeof(keepAliveData))); //only one element so resize not needed
                el->m_onSuccessfulSendCallbackByIoServiceThreadPtr = &m_handleTcpSendKeepAliveCallback;
                m_tcpAsyncSenderPtr->AsyncSend_NotThreadSafe(el); //timer runs in same thread as socket so special thread safety not needed
            }
            else {
                LOG_INFO(subprocess) << "stcp keepalive packet not needed";
            }
        }
        m_dataServedAsKeepAlive = false;
    }
    else {
        m_stcpShutdownComplete = true; //last step in sequence
    }
}

void StcpBundleSource::DoStcpShutdown(unsigned int reconnectionDelaySecondsIfNotZero) {
    boost::asio::post(m_ioService, boost::bind(&StcpBundleSource::DoHandleSocketShutdown, this, reconnectionDelaySecondsIfNotZero));
}

void StcpBundleSource::DoHandleSocketShutdown(unsigned int reconnectionDelaySecondsIfNotZero) {
    //final code to shut down tcp sockets
    m_readyToForward = false;
    if (m_onOutductLinkStatusChangedCallback) { //let user know of link down event
        m_onOutductLinkStatusChangedCallback(true, m_userAssignedUuid);
    }
    if (m_tcpSocketPtr && m_tcpSocketPtr->is_open()) {
        try {
            LOG_INFO(subprocess) << "shutting down StcpBundleSource TCP socket..";
            m_tcpSocketPtr->shutdown(boost::asio::socket_base::shutdown_type::shutdown_both);
        }
        catch (const boost::system::system_error & e) {
            LOG_ERROR(subprocess) << "StcpBundleSource::DoStcpShutdown: " << e.what();
        }
        try {
            LOG_INFO(subprocess) << "closing StcpBundleSource TCP socket socket..";
            m_tcpSocketPtr->close();
        }
        catch (const boost::system::system_error & e) {
            LOG_ERROR(subprocess) << "StcpBundleSource::DoStcpShutdown: " << e.what();
        }
        //don't delete the tcp socket because the Forward function is multi-threaded without a mutex to
        //increase speed, so prevent a race condition that would cause a null pointer exception
        //m_tcpSocketPtr = std::shared_ptr<boost::asio::ip::tcp::socket>();
    }
    m_needToSendKeepAliveMessageTimer.cancel();
    if (reconnectionDelaySecondsIfNotZero) {
        m_reconnectAfterShutdownTimer.expires_from_now(boost::posix_time::seconds(reconnectionDelaySecondsIfNotZero));
        m_reconnectAfterShutdownTimer.async_wait(boost::bind(&StcpBundleSource::OnNeedToReconnectAfterShutdown_TimerExpired, this, boost::asio::placeholders::error));
    }
}

void StcpBundleSource::OnNeedToReconnectAfterShutdown_TimerExpired(const boost::system::error_code& e) {
    if (e != boost::asio::error::operation_aborted) {
        // Timer was not cancelled, take necessary action.
        LOG_INFO(subprocess) << "Trying to reconnect...";
        m_tcpAsyncSenderPtr.reset();
        m_tcpSocketPtr = std::make_shared<boost::asio::ip::tcp::socket>(m_ioService);
        boost::asio::async_connect(
            *m_tcpSocketPtr,
            m_resolverResults,
            boost::bind(
                &StcpBundleSource::OnConnect,
                this,
                boost::asio::placeholders::error));
    }
}

bool StcpBundleSource::ReadyToForward() {
    return m_readyToForward;
}

void StcpBundleSource::SetOnFailedBundleVecSendCallback(const OnFailedBundleVecSendCallback_t& callback) {
    m_onFailedBundleVecSendCallback = callback;
}
void StcpBundleSource::SetOnFailedBundleZmqSendCallback(const OnFailedBundleZmqSendCallback_t& callback) {
    m_onFailedBundleZmqSendCallback = callback;
}
void StcpBundleSource::SetOnSuccessfulBundleSendCallback(const OnSuccessfulBundleSendCallback_t& callback) {
    m_onSuccessfulBundleSendCallback = callback;
}
void StcpBundleSource::SetOnOutductLinkStatusChangedCallback(const OnOutductLinkStatusChangedCallback_t& callback) {
    m_onOutductLinkStatusChangedCallback = callback;
}
void StcpBundleSource::SetUserAssignedUuid(uint64_t userAssignedUuid) {
    m_userAssignedUuid = userAssignedUuid;
}

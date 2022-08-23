/**
 * @file StcpBundleSource.cpp
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

#include <string>
#include <iostream>
#include "StcpBundleSource.h"
#include <boost/lexical_cast.hpp>
#include <boost/make_shared.hpp>
#include <boost/endian/conversion.hpp>
#include <boost/make_unique.hpp>

#define RECONNECTION_DELAY_AFTER_SHUTDOWN_SECONDS 3

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

m_stcpOutductTelemetry()

{
    m_handleTcpSendCallback = boost::bind(&StcpBundleSource::HandleTcpSend, this, boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred);
    m_handleTcpSendKeepAliveCallback = boost::bind(&StcpBundleSource::HandleTcpSendKeepAlive, this, boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred);

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
            std::cout << "notice: StcpBundleSource destructor waiting on " << numUnacked << " unacked bundles" << std::endl;

//            std::cout << "   acked by rate: " << m_totalDataSegmentsAckedByRate << std::endl;
//            std::cout << "   acked by cb: " << m_totalDataSegmentsAckedByTcpSendCallback << std::endl;
//            std::cout << "   total sent: " << m_totalDataSegmentsSent << std::endl;

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
    std::cout << "m_stcpOutductTelemetry.totalBundlesSent " << m_stcpOutductTelemetry.totalBundlesSent << std::endl;
    std::cout << "m_stcpOutductTelemetry.totalBundlesAcked " << m_stcpOutductTelemetry.totalBundlesAcked << std::endl;
    std::cout << "m_stcpOutductTelemetry.totalBundleBytesSent " << m_stcpOutductTelemetry.totalBundleBytesSent << std::endl;
    std::cout << "m_stcpOutductTelemetry.totalStcpBytesSent " << m_stcpOutductTelemetry.totalStcpBytesSent << std::endl;
    std::cout << "m_stcpOutductTelemetry.totalBundleBytesAcked " << m_stcpOutductTelemetry.totalBundleBytesAcked << std::endl;
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




bool StcpBundleSource::Forward(zmq::message_t & dataZmq) {
    if (!m_readyToForward) {
        std::cerr << "link not ready to forward yet" << std::endl;
        return false;
    }


    const unsigned int writeIndexTcpSendCallback = m_bytesToAckByTcpSendCallbackCb.GetIndexForWrite(); //don't put this in tcp async write callback
    if (writeIndexTcpSendCallback == CIRCULAR_INDEX_BUFFER_FULL) { //push check
        std::cerr << "Error in StcpBundleSource::Forward.. too many unacked packets by tcp send callback" << std::endl;
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
    el->m_underlyingData.resize(1);
    StcpBundleSource::GenerateDataUnitHeaderOnly(el->m_underlyingData[0], static_cast<uint32_t>(dataZmq.size()));
    el->m_underlyingDataZmq = boost::make_unique<zmq::message_t>(std::move(dataZmq));
    el->m_constBufferVec.resize(2);
    el->m_constBufferVec[0] = boost::asio::buffer(el->m_underlyingData[0]);
    el->m_constBufferVec[1] = boost::asio::buffer(el->m_underlyingDataZmq->data(), el->m_underlyingDataZmq->size());
    el->m_onSuccessfulSendCallbackByIoServiceThreadPtr = &m_handleTcpSendCallback;
    m_tcpAsyncSenderPtr->AsyncSend_ThreadSafe(el);

    return true;
}

bool StcpBundleSource::Forward(std::vector<uint8_t> & dataVec) {
    if (!m_readyToForward) {
        std::cerr << "link not ready to forward yet" << std::endl;
        return false;
    }

    const unsigned int writeIndexTcpSendCallback = m_bytesToAckByTcpSendCallbackCb.GetIndexForWrite(); //don't put this in tcp async write callback
    if (writeIndexTcpSendCallback == CIRCULAR_INDEX_BUFFER_FULL) { //push check
        std::cerr << "Error in StcpBundleSource::Forward.. too many unacked packets by tcp send callback" << std::endl;
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
    el->m_underlyingData.resize(2);
    StcpBundleSource::GenerateDataUnitHeaderOnly(el->m_underlyingData[0], static_cast<uint32_t>(dataVec.size()));
    el->m_underlyingData[1] = std::move(dataVec);
    el->m_constBufferVec.resize(2);
    el->m_constBufferVec[0] = boost::asio::buffer(el->m_underlyingData[0]);
    el->m_constBufferVec[1] = boost::asio::buffer(el->m_underlyingData[1]);
    el->m_onSuccessfulSendCallbackByIoServiceThreadPtr = &m_handleTcpSendCallback;
    m_tcpAsyncSenderPtr->AsyncSend_ThreadSafe(el);

    return true;
}


bool StcpBundleSource::Forward(const uint8_t* bundleData, const std::size_t size) {
    std::vector<uint8_t> vec(bundleData, bundleData + size);
    return Forward(vec);
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
        std::cerr << "Error resolving: " << ec.message() << std::endl;
    }
    else {
        std::cout << "resolved host to " << results->endpoint().address() << ":" << results->endpoint().port() << ".  Connecting..." << std::endl;
        m_tcpSocketPtr = boost::make_shared<boost::asio::ip::tcp::socket>(m_ioService);
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
            std::cerr << "Error in OnConnect: " << ec.value() << " " << ec.message() << "\n";
            std::cout << "Will try to reconnect after 2 seconds" << std::endl;
            m_reconnectAfterOnConnectErrorTimer.expires_from_now(boost::posix_time::seconds(2));
            m_reconnectAfterOnConnectErrorTimer.async_wait(boost::bind(&StcpBundleSource::OnReconnectAfterOnConnectError_TimerExpired, this, boost::asio::placeholders::error));
        }
        return;
    }

    std::cout << "Stcp connection complete" << std::endl;
    m_stcpShutdownComplete = false;
    m_readyToForward = true;


    m_needToSendKeepAliveMessageTimer.expires_from_now(boost::posix_time::seconds(M_KEEP_ALIVE_INTERVAL_SECONDS));
    m_needToSendKeepAliveMessageTimer.async_wait(boost::bind(&StcpBundleSource::OnNeedToSendKeepAliveMessage_TimerExpired, this, boost::asio::placeholders::error));

    if(m_tcpSocketPtr) {
        m_tcpAsyncSenderPtr = boost::make_unique<TcpAsyncSender>(m_tcpSocketPtr, m_ioService);

        StartTcpReceive();
    }
}

void StcpBundleSource::OnReconnectAfterOnConnectError_TimerExpired(const boost::system::error_code& e) {
    if (e != boost::asio::error::operation_aborted) {
        // Timer was not cancelled, take necessary action.
        std::cout << "Trying to reconnect..." << std::endl;
        boost::asio::async_connect(
            *m_tcpSocketPtr,
            m_resolverResults,
            boost::bind(
                &StcpBundleSource::OnConnect,
                this,
                boost::asio::placeholders::error));
    }
}



void StcpBundleSource::HandleTcpSend(const boost::system::error_code& error, std::size_t bytes_transferred) {
    if (error) {
        std::cerr << "error in StcpBundleSource::HandleTcpSend: " << error.message() << std::endl;
        DoStcpShutdown(RECONNECTION_DELAY_AFTER_SHUTDOWN_SECONDS);
    }
    else {
        const unsigned int readIndex = m_bytesToAckByTcpSendCallbackCb.GetIndexForRead();
        if (readIndex == CIRCULAR_INDEX_BUFFER_EMPTY) { //empty
            std::cerr << "error: AckCallback called with empty queue" << std::endl;
        }
        else if (m_bytesToAckByTcpSendCallbackCbVec[readIndex] == bytes_transferred) {
            ++m_stcpOutductTelemetry.totalBundlesAcked;
            m_stcpOutductTelemetry.totalBundleBytesAcked += m_bytesToAckByTcpSendCallbackCbVec[readIndex] - sizeof(uint32_t);
            m_bytesToAckByTcpSendCallbackCb.CommitRead();
            
            if (m_onSuccessfulAckCallback) {
                m_onSuccessfulAckCallback();
            }
            if (m_useLocalConditionVariableAckReceived) {
                m_localConditionVariableAckReceived.notify_one();
            }
            
        }
        else {
            std::cerr << "error in StcpBundleSource::HandleTcpSend: wrong bytes acked: expected " << m_bytesToAckByTcpSendCallbackCbVec[readIndex] << " but got " << bytes_transferred << std::endl;
        }
    }
}

void StcpBundleSource::HandleTcpSendKeepAlive(const boost::system::error_code& error, std::size_t bytes_transferred) {
    if (error) {
        std::cerr << "error in StcpBundleSource::HandleTcpSendKeepAlive: " << error.message() << std::endl;
        DoStcpShutdown(RECONNECTION_DELAY_AFTER_SHUTDOWN_SECONDS);
    }
    else {
        std::cout << "notice: keepalive packet sent" << std::endl;
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
        std::cerr << "Error in StcpBundleSource::HandleTcpReceiveSome: received " << bytesTransferred << " but should never receive any data" << std::endl;

        //shutdown
    }
    else if (error == boost::asio::error::eof) {
        std::cout << "Tcp connection closed cleanly by peer" << std::endl;
        DoStcpShutdown(RECONNECTION_DELAY_AFTER_SHUTDOWN_SECONDS);
        
    }
    else if (error != boost::asio::error::operation_aborted) {
        std::cerr << "Error in StcpBundleSource::HandleTcpReceiveSome: " << error.message() << std::endl;
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
                std::cout << "notice: stcp keepalive packet not needed" << std::endl;
            }
        }
        m_dataServedAsKeepAlive = false;
    }
    else {
        //std::cout << "timer cancelled\n";
        m_stcpShutdownComplete = true; //last step in sequence
    }
}

void StcpBundleSource::DoStcpShutdown(unsigned int reconnectionDelaySecondsIfNotZero) {
    boost::asio::post(m_ioService, boost::bind(&StcpBundleSource::DoHandleSocketShutdown, this, reconnectionDelaySecondsIfNotZero));
}

void StcpBundleSource::DoHandleSocketShutdown(unsigned int reconnectionDelaySecondsIfNotZero) {
    //final code to shut down tcp sockets
    m_readyToForward = false;
    if (m_tcpSocketPtr && m_tcpSocketPtr->is_open()) {
        try {
            std::cout << "shutting down StcpBundleSource TCP socket.." << std::endl;
            m_tcpSocketPtr->shutdown(boost::asio::socket_base::shutdown_type::shutdown_both);
        }
        catch (const boost::system::system_error & e) {
            std::cerr << "error in StcpBundleSource::DoStcpShutdown: " << e.what() << std::endl;
        }
        try {
            std::cout << "closing StcpBundleSource TCP socket socket.." << std::endl;
            m_tcpSocketPtr->close();
        }
        catch (const boost::system::system_error & e) {
            std::cerr << "error in StcpBundleSource::DoStcpShutdown: " << e.what() << std::endl;
        }
        //don't delete the tcp socket because the Forward function is multi-threaded without a mutex to
        //increase speed, so prevent a race condition that would cause a null pointer exception
        //std::cout << "deleting tcp socket" << std::endl;
        //m_tcpSocketPtr = boost::shared_ptr<boost::asio::ip::tcp::socket>();
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
        std::cout << "Trying to reconnect..." << std::endl;
        m_tcpAsyncSenderPtr.reset();
        m_tcpSocketPtr = boost::make_shared<boost::asio::ip::tcp::socket>(m_ioService);
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

void StcpBundleSource::SetOnSuccessfulAckCallback(const OnSuccessfulAckCallback_t & callback) {
    m_onSuccessfulAckCallback = callback;
}

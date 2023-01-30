/**
 * @file UdpBundleSource.cpp
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
#include "UdpBundleSource.h"
#include "Logger.h"
#include <boost/lexical_cast.hpp>
#include <memory>
#include <boost/make_unique.hpp>
#include <boost/endian/conversion.hpp>
#include "ThreadNamer.h"

static constexpr hdtn::Logger::SubProcess subprocess = hdtn::Logger::SubProcess::none;

static const boost::posix_time::time_duration static_tokenMaxLimitDurationWindow(boost::posix_time::milliseconds(100));
static const boost::posix_time::time_duration static_tokenRefreshTimeDurationWindow(boost::posix_time::milliseconds(20));

UdpBundleSource::UdpBundleSource(const uint64_t rateBps, const unsigned int maxUnacked) :
m_work(m_ioService), //prevent stopping of ioservice until destructor
m_resolver(m_ioService),
m_tokenRefreshTimer(m_ioService),
m_lastTimeTokensWereRefreshed(boost::posix_time::special_values::neg_infin),
m_udpSocket(m_ioService),
m_maxPacketsBeingSent(maxUnacked),
m_bytesToAckBySentCallbackCb(static_cast<uint32_t>(m_maxPacketsBeingSent + 10)),
m_bytesToAckBySentCallbackCbVec(m_maxPacketsBeingSent + 10),
m_userDataCbVec(m_maxPacketsBeingSent + 10),
m_readyToForward(false),
m_useLocalConditionVariableAckReceived(false), //for destructor only
m_tokenRefreshTimerIsRunning(false),
m_userAssignedUuid(0),
m_totalPacketsSentBySentCallback(0),
m_totalBytesSentBySentCallback(0),
m_totalPacketsDequeuedForSend(0),
m_totalBytesDequeuedForSend(0),
m_totalPacketsLimitedByRate(0)
{
    //m_rateManagerAsync.SetPacketsSentCallback(boost::bind(&UdpBundleSource::PacketsSentCallback, this));
    //const uint64_t minimumRateBytesPerSecond = 655360;
    //const uint64_t minimumRateBitsPerSecond = 655360 << 3;
    UpdateRate(rateBps);
    
    const uint64_t tokenLimit = m_tokenRateLimiter.GetRemainingTokens();
    LOG_INFO(subprocess) << "UdpBundleSource: rate bitsPerSec = " << rateBps << "  token limit = " << tokenLimit;

    //The following error message should no longer be relevant since the Token Bucket is allowed to go negative if there is at least 1 token in the bucket.
    //if (tokenLimit < 65536u) {
    //}

    m_ioServiceThreadPtr = boost::make_unique<boost::thread>(boost::bind(&boost::asio::io_service::run, &m_ioService));
    const std::string threadName = "ioServiceUdpBundleSource";
    ThreadNamer::SetThreadName(*m_ioServiceThreadPtr, threadName);
}

UdpBundleSource::~UdpBundleSource() {
    Stop();
    //print stats
    LOG_INFO(subprocess) << "m_totalPacketsSentBySentCallback " << m_totalPacketsSentBySentCallback;
    LOG_INFO(subprocess) << "m_totalBytesSentBySentCallback " << m_totalBytesSentBySentCallback;
    LOG_INFO(subprocess) << "m_totalPacketsDequeuedForSend " << m_totalPacketsDequeuedForSend;
    LOG_INFO(subprocess) << "m_totalBytesDequeuedForSend " << m_totalBytesDequeuedForSend;
    LOG_INFO(subprocess) << "m_totalPacketsLimitedByRate " << m_totalPacketsLimitedByRate;
}

void UdpBundleSource::Stop() {
    //prevent UdpBundleSource from exiting before all bundles sent and acked
    boost::mutex localMutex;
    boost::mutex::scoped_lock lock(localMutex);
    m_useLocalConditionVariableAckReceived = true;
    std::size_t previousUnacked = std::numeric_limits<std::size_t>::max();
    for (unsigned int attempt = 0; attempt < 20; ++attempt) {
        const std::size_t numUnacked = GetTotalUdpPacketsUnacked();
        if (numUnacked) {
            LOG_INFO(subprocess) << "UdpBundleSource destructor waiting on " << numUnacked << " unacked bundles";

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

    DoUdpShutdown();
    while (m_udpSocket.is_open()) {
        boost::this_thread::sleep(boost::posix_time::milliseconds(250));
    }

    //This function does not block, but instead simply signals the io_service to stop
    //All invocations of its run() or run_one() member functions should return as soon as possible.
    //Subsequent calls to run(), run_one(), poll() or poll_one() will return immediately until reset() is called.
    if (!m_ioService.stopped()) {
        m_ioService.stop(); //ioservice requires stopping before join because of the m_work object
    }

    if(m_ioServiceThreadPtr) {
        m_ioServiceThreadPtr->join();
        m_ioServiceThreadPtr.reset(); //delete it
    }

}

void UdpBundleSource::UpdateRate(uint64_t rateBitsPerSec) {
    const uint64_t rateBytesPerSecond = rateBitsPerSec >> 3;
    m_tokenRateLimiter.SetRate(
        rateBytesPerSecond, // 20ms per token
        boost::posix_time::seconds(1),
        static_tokenMaxLimitDurationWindow //token limit of rateBytesPerSecond / (1000ms/100ms) = rateBytesPerSecond / 10
    );
}

bool UdpBundleSource::Forward(std::vector<uint8_t> & dataVec, std::vector<uint8_t>&& userData) {

    if(!m_readyToForward) {
        LOG_ERROR(subprocess) << "link not ready to forward yet";
        return false;
    }

    const unsigned int writeIndexSentCallback = m_bytesToAckBySentCallbackCb.GetIndexForWrite(); //don't put this in tcp async write callback
    if (writeIndexSentCallback == CIRCULAR_INDEX_BUFFER_FULL) { //push check
        LOG_ERROR(subprocess) << "RateManagerAsync::SignalNewPacketDequeuedForSend.. too many unacked packets by tcp send callback";
        return false;
    }

    ++m_totalPacketsDequeuedForSend;
    m_totalBytesDequeuedForSend += dataVec.size();

    m_bytesToAckBySentCallbackCbVec[writeIndexSentCallback] = dataVec.size();
    m_userDataCbVec[writeIndexSentCallback] = std::move(userData);
    m_bytesToAckBySentCallbackCb.CommitWrite(); //pushed

    std::shared_ptr<std::vector<uint8_t> > udpDataToSendPtr = std::make_shared<std::vector<uint8_t> >(std::move(dataVec));
    //dataVec invalid after this point
    boost::asio::post(m_ioService, boost::bind(&UdpBundleSource::HandlePostForUdpSendVecMessage, this, std::move(udpDataToSendPtr)));
    
    return true;
}

bool UdpBundleSource::Forward(zmq::message_t & dataZmq, std::vector<uint8_t>&& userData) {

    if (!m_readyToForward) {
        LOG_ERROR(subprocess) << "link not ready to forward yet";
        return false;
    }

    const unsigned int writeIndexSentCallback = m_bytesToAckBySentCallbackCb.GetIndexForWrite(); //don't put this in tcp async write callback
    if (writeIndexSentCallback == CIRCULAR_INDEX_BUFFER_FULL) { //push check
        LOG_ERROR(subprocess) << "RateManagerAsync::SignalNewPacketDequeuedForSend.. too many unacked packets by tcp send callback";
        return false;
    }

    ++m_totalPacketsDequeuedForSend;
    m_totalBytesDequeuedForSend += dataZmq.size();

    m_bytesToAckBySentCallbackCbVec[writeIndexSentCallback] = dataZmq.size();
    m_userDataCbVec[writeIndexSentCallback] = std::move(userData);
    m_bytesToAckBySentCallbackCb.CommitWrite(); //pushed

    std::shared_ptr<zmq::message_t> zmqDataToSendPtr = std::make_shared<zmq::message_t>(std::move(dataZmq));
    //dataZmq invalid after this point
    boost::asio::post(m_ioService, boost::bind(&UdpBundleSource::HandlePostForUdpSendZmqMessage, this, std::move(zmqDataToSendPtr)));
    return true;
}

bool UdpBundleSource::Forward(const uint8_t* bundleData, const std::size_t size, std::vector<uint8_t>&& userData) {
    std::vector<uint8_t> vec(bundleData, bundleData + size);
    return Forward(vec, std::move(userData));
}


std::size_t UdpBundleSource::GetTotalUdpPacketsAcked() {
    return m_totalPacketsSentBySentCallback;
}

std::size_t UdpBundleSource::GetTotalUdpPacketsSent() {
    return m_totalPacketsDequeuedForSend;
}

std::size_t UdpBundleSource::GetTotalUdpPacketsUnacked() {
    return m_totalPacketsDequeuedForSend - m_totalPacketsSentBySentCallback;
}

std::size_t UdpBundleSource::GetTotalBundleBytesAcked() {
    return m_totalBytesSentBySentCallback;
}

std::size_t UdpBundleSource::GetTotalBundleBytesSent() {
    return m_totalBytesDequeuedForSend;
}

std::size_t UdpBundleSource::GetTotalBundleBytesUnacked() {
    return m_totalBytesDequeuedForSend - m_totalBytesSentBySentCallback;
}


void UdpBundleSource::Connect(const std::string & hostname, const std::string & port) {

    static const boost::asio::ip::resolver_query_base::flags UDP_RESOLVER_FLAGS = boost::asio::ip::resolver_query_base::canonical_name; //boost resolver flags
    LOG_INFO(subprocess) << "udp resolving " << hostname << ":" << port;
    boost::asio::ip::udp::resolver::query query(boost::asio::ip::udp::v4(), hostname, port, UDP_RESOLVER_FLAGS);
    m_resolver.async_resolve(query, boost::bind(&UdpBundleSource::OnResolve,
                                                this, boost::asio::placeholders::error,
                                                boost::asio::placeholders::results));
}

void UdpBundleSource::OnResolve(const boost::system::error_code & ec, boost::asio::ip::udp::resolver::results_type results) { // Resolved endpoints as a range.
    if(ec) {
        LOG_ERROR(subprocess) << "Error resolving: " << ec.message();
    }
    else {
        m_udpDestinationEndpoint = *results;
        LOG_INFO(subprocess) << "resolved host to " << m_udpDestinationEndpoint.address() << ":" << m_udpDestinationEndpoint.port() << ".  Binding...";
        try {            
            m_udpSocket.open(boost::asio::ip::udp::v4());
            m_udpSocket.bind(boost::asio::ip::udp::endpoint(boost::asio::ip::udp::v4(), 0)); //bind to 0 (random ephemeral port)

            LOG_INFO(subprocess) << "UDP Bound on ephemeral port " << m_udpSocket.local_endpoint().port();
            LOG_INFO(subprocess) << "UDP READY";
            m_readyToForward = true;

        }
        catch (const boost::system::system_error & e) {
            LOG_ERROR(subprocess) << "UdpBundleSource::OnResolve(): " << e.what();
            return;
        }
    }
}

void UdpBundleSource::HandlePostForUdpSendVecMessage(std::shared_ptr<std::vector<boost::uint8_t> > & vecDataToSendPtr) {
    //now that the token rate limiter can be used entirely in one thread (the io_service thread), take tokens
    m_queueVecDataToSendPtrs.emplace(std::move(vecDataToSendPtr)); //put on the queue first (there might be other packets in there that need to be sent first)
    std::shared_ptr<std::vector<boost::uint8_t> > & vecDataToSendFrontOfQueuePtr = m_queueVecDataToSendPtrs.front();
    //try to remove the front of the queue if tokens available
    if (m_tokenRateLimiter.TakeTokens(vecDataToSendFrontOfQueuePtr->size())) { //there are tokens available for the packet at the front of the queue, send this now
        boost::asio::const_buffer bufToSend = boost::asio::buffer(*vecDataToSendFrontOfQueuePtr);
        m_udpSocket.async_send_to(bufToSend, m_udpDestinationEndpoint,
            boost::bind(&UdpBundleSource::HandleUdpSendVecMessage, this, std::move(vecDataToSendFrontOfQueuePtr),
                boost::asio::placeholders::error,
                boost::asio::placeholders::bytes_transferred));
        m_queueVecDataToSendPtrs.pop();
        m_totalPacketsLimitedByRate += (!m_queueVecDataToSendPtrs.empty());
    }
    //else //no tokens available, the already queued packet will be processed by the m_tokenRefreshTimer expiration
        
    TryRestartTokenRefreshTimer(); //start the token refresh timer if and only if it is not already running
}

void UdpBundleSource::HandlePostForUdpSendZmqMessage(std::shared_ptr<zmq::message_t> & zmqDataToSendPtr) {
    //now that the token rate limiter can be used entirely in one thread (the io_service thread), take tokens
    m_queueZmqDataToSendPtrs.emplace(std::move(zmqDataToSendPtr)); //put on the queue first (there might be other packets in there that need to be sent first)
    std::shared_ptr<zmq::message_t> & zmqDataToSendFrontOfQueuePtr = m_queueZmqDataToSendPtrs.front();
    //try to remove the front of the queue if tokens available
    if (m_tokenRateLimiter.TakeTokens(zmqDataToSendFrontOfQueuePtr->size())) { //there are tokens available, send this now
        boost::asio::const_buffer bufToSend = boost::asio::buffer(zmqDataToSendFrontOfQueuePtr->data(), zmqDataToSendFrontOfQueuePtr->size());
        m_udpSocket.async_send_to(bufToSend, m_udpDestinationEndpoint,
            boost::bind(&UdpBundleSource::HandleUdpSendZmqMessage, this, std::move(zmqDataToSendFrontOfQueuePtr),
                boost::asio::placeholders::error,
                boost::asio::placeholders::bytes_transferred));
        m_queueZmqDataToSendPtrs.pop();
        m_totalPacketsLimitedByRate += (!m_queueZmqDataToSendPtrs.empty());
    }
    //else //no tokens available, the already queued packet will be processed by the m_tokenRefreshTimer expiration

    TryRestartTokenRefreshTimer(); //start the token refresh timer if and only if it is not already running
}


void UdpBundleSource::HandleUdpSendVecMessage(std::shared_ptr<std::vector<boost::uint8_t> > & dataSentPtr, const boost::system::error_code& error, std::size_t bytes_transferred) {
    if (error) {
        LOG_ERROR(subprocess) << "UdpBundleSource::HandleUdpSend: " << error.message();
        DoUdpShutdown();
    }
    else if (!ProcessPacketSent(bytes_transferred)) {
        DoUdpShutdown();
    }
}

void UdpBundleSource::HandleUdpSendZmqMessage(std::shared_ptr<zmq::message_t> & dataZmqSentPtr, const boost::system::error_code& error, std::size_t bytes_transferred) {
    if (error) {
        LOG_ERROR(subprocess) << "UdpBundleSource::HandleUdpSendZmqMessage: " << error.message();
        DoUdpShutdown();
    }
    else if(!ProcessPacketSent(bytes_transferred)) {
        DoUdpShutdown();
    }
}

bool UdpBundleSource::ProcessPacketSent(std::size_t bytes_transferred) {

    const unsigned int readIndex = m_bytesToAckBySentCallbackCb.GetIndexForRead();
    if (readIndex == CIRCULAR_INDEX_BUFFER_EMPTY) { //empty
        LOG_ERROR(subprocess) << "UdpBundleSource::ProcessPacketSent: AckCallback called with empty queue";
        return false;
    }
    else if (m_bytesToAckBySentCallbackCbVec[readIndex] == bytes_transferred) {
        ++m_totalPacketsSentBySentCallback;
        m_totalBytesSentBySentCallback += m_bytesToAckBySentCallbackCbVec[readIndex];
        std::vector<uint8_t> userData(std::move(m_userDataCbVec[readIndex]));
        m_bytesToAckBySentCallbackCb.CommitRead();

        if (m_onSuccessfulBundleSendCallback) {
            m_onSuccessfulBundleSendCallback(userData, m_userAssignedUuid);
        }
        if (m_useLocalConditionVariableAckReceived) {
            m_localConditionVariableAckReceived.notify_one();
        }
        return true;
    }
    else {
        LOG_ERROR(subprocess) << "UdpBundleSource::ProcessPacketSent: wrong bytes acked: expected " << m_bytesToAckBySentCallbackCbVec[readIndex] << " but got " << bytes_transferred;
        return false;
    }
}


void UdpBundleSource::DoUdpShutdown() {
    boost::asio::post(m_ioService, boost::bind(&UdpBundleSource::DoHandleSocketShutdown, this));
}

void UdpBundleSource::DoHandleSocketShutdown() {
    //final code to shut down tcp sockets
    m_readyToForward = false;
    if (m_udpSocket.is_open()) {
        try {
            LOG_INFO(subprocess) << "shutting down UdpBundleSource UDP socket..";
            m_udpSocket.shutdown(boost::asio::socket_base::shutdown_type::shutdown_both);
        }
        catch (const boost::system::system_error & e) {
            LOG_ERROR(subprocess) << "UdpBundleSource::DoUdpShutdown: " << e.what();
        }
        try {
            LOG_INFO(subprocess) << "closing UdpBundleSource UDP socket..";
            m_udpSocket.close();
        }
        catch (const boost::system::system_error & e) {
            LOG_ERROR(subprocess) << "UdpBundleSource::DoUdpShutdown: " << e.what();
        }
    }
}

bool UdpBundleSource::ReadyToForward() const {
    return m_readyToForward;
}


//restarts the token refresh timer if it is not running from now
void UdpBundleSource::TryRestartTokenRefreshTimer() {
    if (!m_tokenRefreshTimerIsRunning) {
        const boost::posix_time::ptime nowPtime = boost::posix_time::microsec_clock::universal_time();
        if (m_lastTimeTokensWereRefreshed.is_neg_infinity()) {
            m_lastTimeTokensWereRefreshed = nowPtime;
        }
        m_tokenRefreshTimer.expires_at(nowPtime + static_tokenRefreshTimeDurationWindow);
        m_tokenRefreshTimer.async_wait(boost::bind(&UdpBundleSource::OnTokenRefresh_TimerExpired, this, boost::asio::placeholders::error));
        m_tokenRefreshTimerIsRunning = true;
    }
}
//restarts the token refresh timer if it is not running from the given ptime
void UdpBundleSource::TryRestartTokenRefreshTimer(const boost::posix_time::ptime & nowPtime) {
    if (!m_tokenRefreshTimerIsRunning) {
        if (m_lastTimeTokensWereRefreshed.is_neg_infinity()) {
            m_lastTimeTokensWereRefreshed = nowPtime;
        }
        m_tokenRefreshTimer.expires_at(nowPtime + static_tokenRefreshTimeDurationWindow);
        m_tokenRefreshTimer.async_wait(boost::bind(&UdpBundleSource::OnTokenRefresh_TimerExpired, this, boost::asio::placeholders::error));
        m_tokenRefreshTimerIsRunning = true;
    }
}

void UdpBundleSource::OnTokenRefresh_TimerExpired(const boost::system::error_code& e) {
    const boost::posix_time::ptime nowPtime = boost::posix_time::microsec_clock::universal_time();
    const boost::posix_time::time_duration diff = nowPtime - m_lastTimeTokensWereRefreshed;
    m_tokenRateLimiter.AddTime(diff);
    m_lastTimeTokensWereRefreshed = nowPtime;
    m_tokenRefreshTimerIsRunning = false;
    if (e != boost::asio::error::operation_aborted) {
        // Timer was not cancelled, take necessary action.
        while (!m_queueVecDataToSendPtrs.empty()) {
            std::shared_ptr<std::vector<boost::uint8_t> > & vecDataToSendPtr = m_queueVecDataToSendPtrs.front();
            //empty the queue of rate limited packets
            if (m_tokenRateLimiter.TakeTokens(vecDataToSendPtr->size())) { //there are tokens available, send this now
                boost::asio::const_buffer bufToSend = boost::asio::buffer(*vecDataToSendPtr);
                m_udpSocket.async_send_to(bufToSend, m_udpDestinationEndpoint,
                    boost::bind(&UdpBundleSource::HandleUdpSendVecMessage, this, std::move(vecDataToSendPtr),
                        boost::asio::placeholders::error,
                        boost::asio::placeholders::bytes_transferred));
                m_queueVecDataToSendPtrs.pop();
                ++m_totalPacketsLimitedByRate;
            }
            else { //no tokens available, empty the packet queue at the next m_tokenRefreshTimer expiration
                TryRestartTokenRefreshTimer(nowPtime);
                return;
            }
        }
        while (!m_queueZmqDataToSendPtrs.empty()) {
            std::shared_ptr<zmq::message_t> & zmqDataToSendPtr = m_queueZmqDataToSendPtrs.front();
            //empty the queue of rate limited packets
            if (m_tokenRateLimiter.TakeTokens(zmqDataToSendPtr->size())) { //there are tokens available, send this now
                boost::asio::const_buffer bufToSend = boost::asio::buffer(zmqDataToSendPtr->data(), zmqDataToSendPtr->size());
                m_udpSocket.async_send_to(bufToSend, m_udpDestinationEndpoint,
                    boost::bind(&UdpBundleSource::HandleUdpSendZmqMessage, this, std::move(zmqDataToSendPtr),
                        boost::asio::placeholders::error,
                        boost::asio::placeholders::bytes_transferred));
                m_queueZmqDataToSendPtrs.pop();
                ++m_totalPacketsLimitedByRate;
            }
            else { //no tokens available, empty the packet queue at the next m_tokenRefreshTimer expiration
                TryRestartTokenRefreshTimer(nowPtime);
                return;
            }
        }
        //If more tokens can be added, restart the timer so more tokens will be added at the next timer expiration.
        //Otherwise, if full, don't restart the timer and the next send packet operation will start it.
        if (!m_tokenRateLimiter.HasFullBucketOfTokens()) {
            TryRestartTokenRefreshTimer(nowPtime);
        }
    }
}

void UdpBundleSource::SetOnFailedBundleVecSendCallback(const OnFailedBundleVecSendCallback_t& callback) {
    m_onFailedBundleVecSendCallback = callback;
}
void UdpBundleSource::SetOnFailedBundleZmqSendCallback(const OnFailedBundleZmqSendCallback_t& callback) {
    m_onFailedBundleZmqSendCallback = callback;
}
void UdpBundleSource::SetOnSuccessfulBundleSendCallback(const OnSuccessfulBundleSendCallback_t& callback) {
    m_onSuccessfulBundleSendCallback = callback;
}
void UdpBundleSource::SetOnOutductLinkStatusChangedCallback(const OnOutductLinkStatusChangedCallback_t& callback) {
    m_onOutductLinkStatusChangedCallback = callback;
}
void UdpBundleSource::SetUserAssignedUuid(uint64_t userAssignedUuid) {
    m_userAssignedUuid = userAssignedUuid;
}

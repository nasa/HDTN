/**
 * @file BpOverEncapBundleSource.cpp
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
#include "Logger.h"
#include "BpOverEncapBundleSource.h"
#include <memory>
#include <boost/make_unique.hpp>
#include "ThreadNamer.h"

static constexpr hdtn::Logger::SubProcess subprocess = hdtn::Logger::SubProcess::none;

BpOverEncapBundleSource::BpOverEncapBundleSource(const unsigned int maxUnacked) :
m_work(m_ioService), //prevent stopping of ioservice until destructor
m_encapAsyncDuplexLocalStream(m_ioService, //ltp engine will handle i/o, keeping entirely single threaded
    ENCAP_PACKET_TYPE::BP,
    1,//maxBundleSizeBytes, //initial resize (don't waste memory with potential max bundle size)
    boost::bind(&BpOverEncapBundleSource::OnFullEncapPacketReceived, this,
        boost::placeholders::_1, boost::placeholders::_2, boost::placeholders::_3),
    boost::bind(&BpOverEncapBundleSource::OnLocalStreamConnectionStatusChanged, this,
        boost::placeholders::_1),
    false), //false => discard encap header (hdtn only cares about the bundle)
MAX_UNACKED(maxUnacked),
m_toSendCb(MAX_UNACKED),
m_bytesToAckBySendCallbackCbVec(MAX_UNACKED),
m_tcpAsyncSenderElementsCbVec(MAX_UNACKED),
m_writeInProgress(false),
m_sendErrorOccurred(false),
m_useLocalConditionVariableAckReceived(false), //for destructor only
m_userAssignedUuid(0),
//telemetry
m_totalBundlesSent(0),
m_totalBundlesAcked(0),
m_totalBundleBytesSent(0),
m_totalEncapHeaderBytesSent(0),
m_totalBundleBytesAcked(0),
m_totalBundlesFailedToSend(0)
{
    m_ioServiceThreadPtr = boost::make_unique<boost::thread>(boost::bind(&boost::asio::io_service::run, &m_ioService));
    ThreadNamer::SetIoServiceThreadName(m_ioService, "ioServiceBpEncapSrc");
}

BpOverEncapBundleSource::~BpOverEncapBundleSource() {
    Stop();
}

void BpOverEncapBundleSource::Stop() {
    //prevent BpOverEncapBundleSource from exiting before all bundles sent and acked
    try {
        { //scope for destruction of lock within try block
            boost::mutex localMutex;
            boost::mutex::scoped_lock lock(localMutex);
            m_useLocalConditionVariableAckReceived = true;
            std::size_t previousUnacked = std::numeric_limits<std::size_t>::max();
            for (unsigned int attempt = 0; attempt < 20; ++attempt) {
                const std::size_t numUnacked = GetTotalBundlesUnacked();
                if (numUnacked) {
                    LOG_INFO(subprocess) << "StcpBundleSource destructor waiting on " << numUnacked << " unacked bundles";


                    if (previousUnacked > numUnacked) {
                        previousUnacked = numUnacked;
                        attempt = 0;
                    }
                    m_localConditionVariableAckReceived.timed_wait(lock, boost::posix_time::milliseconds(500)); // call lock.unlock() and blocks the current thread
                    continue;
                }
                break;
            }
        }
    }
    catch (const boost::condition_error& e) {
        LOG_ERROR(subprocess) << "condition_error in StcpBundleSource::Stop: " << e.what();
    }
    catch (const boost::thread_resource_error& e) {
        LOG_ERROR(subprocess) << "thread_resource_error in StcpBundleSource::Stop: " << e.what();
    }
    catch (const boost::thread_interrupted&) {
        LOG_ERROR(subprocess) << "thread_interrupted in StcpBundleSource::Stop";
    }
    catch (const boost::lock_error& e) {
        LOG_ERROR(subprocess) << "lock_error in StcpBundleSource::Stop: " << e.what();
    }

    m_encapAsyncDuplexLocalStream.Stop(); //stop this first

    //This function does not block, but instead simply signals the io_service to stop
    //All invocations of its run() or run_one() member functions should return as soon as possible.
    //Subsequent calls to run(), run_one(), poll() or poll_one() will return immediately until reset() is called.
    m_ioService.stop(); //ioservice requires stopping before join because of the m_work object

    if (m_ioServiceThreadPtr) {
        try {
            m_ioServiceThreadPtr->join();
            m_ioServiceThreadPtr.reset(); //delete it
        }
        catch (const boost::thread_resource_error&) {
            LOG_ERROR(subprocess) << "error stopping StcpBundleSource io_service";
        }
        //print stats once
        LOG_INFO(subprocess) << "Stcp Outduct / Bundle Source:"
            << "\n totalBundlesSent " << m_totalBundlesSent
            << "\n totalBundlesAcked " << m_totalBundlesAcked
            << "\n totalBundleBytesSent " << m_totalBundleBytesSent
            << "\n m_totalEncapHeaderBytesSent " << m_totalEncapHeaderBytesSent
            << "\n totalBundleBytesAcked " << m_totalBundleBytesAcked
            << "\n m_totalBundlesFailedToSend " << m_totalBundlesFailedToSend;
    }
}

void BpOverEncapBundleSource::DoFailedBundleCallback(TcpAsyncSenderElement& el) {
    ++m_totalBundlesFailedToSend;
    if ((el.m_underlyingDataVecBundle.size()) && (m_onFailedBundleVecSendCallback)) {
        m_onFailedBundleVecSendCallback(el.m_underlyingDataVecBundle, el.m_userData, m_userAssignedUuid, false);
    }
    else if ((el.m_underlyingDataZmqBundle) && (m_onFailedBundleZmqSendCallback)) {
        m_onFailedBundleZmqSendCallback(*el.m_underlyingDataZmqBundle, el.m_userData, m_userAssignedUuid, false);
    }
}

void BpOverEncapBundleSource::TrySendDataIfAvailable_NotThreadSafe() {
    const unsigned int readIndex = m_toSendCb.GetIndexForRead();
    if (readIndex != CIRCULAR_INDEX_BUFFER_EMPTY) { //not empty
        TcpAsyncSenderElement& el = m_tcpAsyncSenderElementsCbVec[readIndex];
        if (m_sendErrorOccurred.load(std::memory_order_acquire)) {
            //prevent bundle from being queued
            DoFailedBundleCallback(el);
            el.DeallocateBundleMemory();
            m_toSendCb.CommitRead();
            TrySendDataIfAvailable_ThreadSafe(); //empty queue without recursion
        }
        else {
            if (!m_writeInProgress.exchange(true)) {
                boost::asio::async_write(m_encapAsyncDuplexLocalStream.GetStreamHandleRef(),
                    el.m_constBufferVec, //same as senderElementNeedingDeleted since m_writeInProgress is false
                    boost::bind(&BpOverEncapBundleSource::HandleSend, this,
                        boost::asio::placeholders::error,
                        boost::asio::placeholders::bytes_transferred,
                        readIndex));
            }
        }
    }
}

void BpOverEncapBundleSource::TrySendDataIfAvailable_ThreadSafe() {
    boost::asio::post(m_ioService, boost::bind(&BpOverEncapBundleSource::TrySendDataIfAvailable_NotThreadSafe, this));
}

void BpOverEncapBundleSource::HandleSend(const boost::system::error_code& error, std::size_t bytes_transferred, const unsigned int readIndex) {
    if (error) {
        m_sendErrorOccurred = true;
        LOG_ERROR(subprocess) << "BpOverEncapBundleSource::HandleSend: " << error.message();
    }
    else if (m_bytesToAckBySendCallbackCbVec[readIndex] != bytes_transferred) {
        m_sendErrorOccurred = true;
        LOG_ERROR(subprocess) << "BpOverEncapBundleSource::HandleSend: wrong bytes acked: expected " << m_bytesToAckBySendCallbackCbVec[readIndex] << " but got " << bytes_transferred;
    }
    else {
        TcpAsyncSenderElement& el = m_tcpAsyncSenderElementsCbVec[readIndex];
        m_totalBundlesAcked.fetch_add(1, std::memory_order_relaxed);
        m_totalBundleBytesAcked.fetch_add(el.m_constBufferVec.back().size(), std::memory_order_relaxed); //back is the bundle, front is the encap header 

        if (m_onSuccessfulBundleSendCallback) {
            m_onSuccessfulBundleSendCallback(el.m_userData, m_userAssignedUuid);
        }
        if (m_useLocalConditionVariableAckReceived.load(std::memory_order_acquire)) {
            m_localConditionVariableAckReceived.notify_one();
        }
        el.DeallocateBundleMemory();
        m_toSendCb.CommitRead();
    }
    TrySendDataIfAvailable_NotThreadSafe(); //will empty the queue (with failed callbacks) on error
}


bool BpOverEncapBundleSource::Forward(zmq::message_t & dataZmq, std::vector<uint8_t>&& userData) {
    if (!ReadyToForward()) {
        LOG_ERROR(subprocess) << "link not ready to forward yet";
        return false;
    }


    const unsigned int writeIndex = m_toSendCb.GetIndexForWrite(); //don't put this in async write callback
    if (writeIndex == CIRCULAR_INDEX_BUFFER_FULL) { //push check
        LOG_ERROR(subprocess) << "BpOverEncapBundleSource::Forward.. too many unacked packets by send callback";
        return false;
    }

    TcpAsyncSenderElement& el = m_tcpAsyncSenderElementsCbVec[writeIndex];

    //encode encap header
    el.m_underlyingDataVecHeaders.resize(1);
    std::vector<uint8_t>& encapHeaderVec = el.m_underlyingDataVecHeaders[0];
    encapHeaderVec.resize(8);
    uint8_t* const encapOutHeader8Byte = encapHeaderVec.data();
    uint8_t encodedEncapHeaderSize;
    //encode
    if (!GetCcsdsEncapHeader(ENCAP_PACKET_TYPE::BP, encapOutHeader8Byte, static_cast<uint32_t>(dataZmq.size()), encodedEncapHeaderSize)) {
        LOG_FATAL(subprocess) << "BpOverEncapBundleSource::Forward.. unable to encode encap header";
        return false;
    }
    

    encapHeaderVec.resize(encodedEncapHeaderSize);


    m_totalBundlesSent.fetch_add(1, std::memory_order_relaxed);
    m_totalBundleBytesSent.fetch_add(dataZmq.size(), std::memory_order_relaxed);

    const std::size_t encapHeaderPlusBundleSize = dataZmq.size() + encodedEncapHeaderSize;
    m_totalEncapHeaderBytesSent.fetch_add(encodedEncapHeaderSize, std::memory_order_relaxed);
    m_bytesToAckBySendCallbackCbVec[writeIndex] = encapHeaderPlusBundleSize;

    el.m_userData = std::move(userData);
    
    
    
    
    
    el.m_underlyingDataZmqBundle = boost::make_unique<zmq::message_t>(std::move(dataZmq));
    el.m_constBufferVec.resize(2);
    el.m_constBufferVec[0] = boost::asio::buffer(el.m_underlyingDataVecHeaders[0]);
    el.m_constBufferVec[1] = boost::asio::buffer(el.m_underlyingDataZmqBundle->data(), el.m_underlyingDataZmqBundle->size());
    m_toSendCb.CommitWrite(); //pushed
    TrySendDataIfAvailable_ThreadSafe();

    return true;
}

bool BpOverEncapBundleSource::Forward(padded_vector_uint8_t& dataVec, std::vector<uint8_t>&& userData) {

    if (!ReadyToForward()) {
        LOG_ERROR(subprocess) << "link not ready to forward yet";
        return false;
    }


    const unsigned int writeIndex = m_toSendCb.GetIndexForWrite(); //don't put this in async write callback
    if (writeIndex == CIRCULAR_INDEX_BUFFER_FULL) { //push check
        LOG_ERROR(subprocess) << "BpOverEncapBundleSource::Forward.. too many unacked packets by send callback";
        return false;
    }

    TcpAsyncSenderElement& el = m_tcpAsyncSenderElementsCbVec[writeIndex];

    //encode encap header
    el.m_underlyingDataVecHeaders.resize(1);
    std::vector<uint8_t>& encapHeaderVec = el.m_underlyingDataVecHeaders[0];
    encapHeaderVec.resize(8);
    uint8_t* const encapOutHeader8Byte = encapHeaderVec.data();
    uint8_t encodedEncapHeaderSize;
    //encode
    if (!GetCcsdsEncapHeader(ENCAP_PACKET_TYPE::BP, encapOutHeader8Byte, static_cast<uint32_t>(dataVec.size()), encodedEncapHeaderSize)) {
        LOG_FATAL(subprocess) << "BpOverEncapBundleSource::Forward.. unable to encode encap header";
        return false;
    }


    encapHeaderVec.resize(encodedEncapHeaderSize);


    m_totalBundlesSent.fetch_add(1, std::memory_order_relaxed);
    m_totalBundleBytesSent.fetch_add(dataVec.size(), std::memory_order_relaxed);

    const std::size_t encapHeaderPlusBundleSize = dataVec.size() + encodedEncapHeaderSize;
    m_totalEncapHeaderBytesSent.fetch_add(encodedEncapHeaderSize, std::memory_order_relaxed);
    m_bytesToAckBySendCallbackCbVec[writeIndex] = encapHeaderPlusBundleSize;

    el.m_userData = std::move(userData);




    el.m_underlyingDataVecBundle = std::move(dataVec);
    el.m_constBufferVec.resize(2);
    el.m_constBufferVec[0] = boost::asio::buffer(el.m_underlyingDataVecHeaders[0]);
    el.m_constBufferVec[1] = boost::asio::buffer(el.m_underlyingDataVecBundle);
    m_toSendCb.CommitWrite(); //pushed
    TrySendDataIfAvailable_ThreadSafe();

    return true;
    
}


bool BpOverEncapBundleSource::Forward(const uint8_t* bundleData, const std::size_t size, std::vector<uint8_t>&& userData) {
    padded_vector_uint8_t vec(bundleData, bundleData + size);
    return Forward(vec, std::move(userData));
}


std::size_t BpOverEncapBundleSource::GetTotalBundlesAcked() const noexcept {
    return m_totalBundlesAcked.load(std::memory_order_acquire);
}

std::size_t BpOverEncapBundleSource::GetTotalBundlesSent() const noexcept {
    return m_totalBundlesSent.load(std::memory_order_acquire);
}

std::size_t BpOverEncapBundleSource::GetTotalBundlesUnacked() const noexcept {
    return m_totalBundlesSent.load(std::memory_order_acquire) - m_totalBundlesAcked.load(std::memory_order_acquire);
}

std::size_t BpOverEncapBundleSource::GetTotalBundleBytesAcked() const noexcept {
    return m_totalBundleBytesAcked.load(std::memory_order_acquire);
}

std::size_t BpOverEncapBundleSource::GetTotalBundleBytesSent() const noexcept {
    return m_totalBundleBytesSent.load(std::memory_order_acquire);
}

std::size_t BpOverEncapBundleSource::GetTotalBundleBytesUnacked() const noexcept {
    return m_totalBundleBytesSent.load(std::memory_order_acquire) - m_totalBundleBytesAcked.load(std::memory_order_acquire);
}


void BpOverEncapBundleSource::Connect(const std::string& socketOrPipePath) {

    if (!m_encapAsyncDuplexLocalStream.Init(socketOrPipePath, false)) {
        LOG_FATAL(subprocess) << "cannot init BP over Encap";
        return;
    }
}


void BpOverEncapBundleSource::OnLocalStreamConnectionStatusChanged(bool isOnConnectionEvent) {
    LOG_INFO(subprocess) << "BpOverEncapBundleSource connection " << ((isOnConnectionEvent) ? "up" : "down");
    if (m_onOutductLinkStatusChangedCallback) { //let user know of link up event
        m_onOutductLinkStatusChangedCallback(!isOnConnectionEvent, m_userAssignedUuid);
    }
}



void BpOverEncapBundleSource::OnFullEncapPacketReceived(padded_vector_uint8_t& receivedFullEncapPacket,
    uint32_t decodedEncapPayloadSize, uint8_t decodedEncapHeaderSize)
{
   /* m_totalBundleBytesReceived.fetch_add(receivedFullEncapPacket.size(), std::memory_order_relaxed);
    m_totalBundlesReceived.fetch_add(1, std::memory_order_relaxed);
    m_wholeBundleReadyCallback(receivedFullEncapPacket); //receivedFullEncapPacket is just the bundle (encap header discarded)*/
    //called by the io_service thread (can use non-thread-safe function calls)
    m_encapAsyncDuplexLocalStream.StartReadFirstEncapHeaderByte_NotThreadSafe();
}


bool BpOverEncapBundleSource::ReadyToForward() {
    return m_encapAsyncDuplexLocalStream.ReadyToSend();
}

void BpOverEncapBundleSource::SetOnFailedBundleVecSendCallback(const OnFailedBundleVecSendCallback_t& callback) {
    m_onFailedBundleVecSendCallback = callback;
}
void BpOverEncapBundleSource::SetOnFailedBundleZmqSendCallback(const OnFailedBundleZmqSendCallback_t& callback) {
    m_onFailedBundleZmqSendCallback = callback;
}
void BpOverEncapBundleSource::SetOnSuccessfulBundleSendCallback(const OnSuccessfulBundleSendCallback_t& callback) {
    m_onSuccessfulBundleSendCallback = callback;
}
void BpOverEncapBundleSource::SetOnOutductLinkStatusChangedCallback(const OnOutductLinkStatusChangedCallback_t& callback) {
    m_onOutductLinkStatusChangedCallback = callback;
}
void BpOverEncapBundleSource::SetUserAssignedUuid(uint64_t userAssignedUuid) {
    m_userAssignedUuid = userAssignedUuid;
}

void BpOverEncapBundleSource::GetTelemetry(/*Stcp*/OutductTelemetry_t& telem) const {
    telem.m_totalBundlesSent = m_totalBundlesSent.load(std::memory_order_acquire);
    telem.m_totalBundlesAcked = m_totalBundlesAcked.load(std::memory_order_acquire);
    telem.m_totalBundleBytesSent = m_totalBundleBytesSent.load(std::memory_order_acquire);
    ////telem.m_totalStcpBytesSent = m_totalEncapHeaderBytesSent.load(std::memory_order_acquire);
    telem.m_totalBundleBytesAcked = m_totalBundleBytesAcked.load(std::memory_order_acquire);
    telem.m_totalBundlesFailedToSend = m_totalBundlesFailedToSend.load(std::memory_order_acquire);
    telem.m_linkIsUpPhysically = m_encapAsyncDuplexLocalStream.ReadyToSend();
}

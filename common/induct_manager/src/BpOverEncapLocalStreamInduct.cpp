/**
 * @file BpOverEncapLocalStreamInduct.cpp
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

#include "BpOverEncapLocalStreamInduct.h"
#include "Logger.h"
#include <boost/make_unique.hpp>
#include <boost/lexical_cast.hpp>
#include "ThreadNamer.h"

static constexpr hdtn::Logger::SubProcess subprocess = hdtn::Logger::SubProcess::none;
static const unsigned int maxTxBundlesInFlight = 5;

BpOverEncapLocalStreamInduct::BpOverEncapLocalStreamInduct(const InductProcessBundleCallback_t & inductProcessBundleCallback, const induct_element_config_t & inductConfig,
    const uint64_t maxBundleSizeBytes, const OnNewOpportunisticLinkCallback_t & onNewOpportunisticLinkCallback,
    const OnDeletedOpportunisticLinkCallback_t & onDeletedOpportunisticLinkCallback) :
    Induct(inductProcessBundleCallback, inductConfig),
    m_encapAsyncDuplexLocalStream(m_ioService, //ltp engine will handle i/o, keeping entirely single threaded
        ENCAP_PACKET_TYPE::BP,
        1,//maxBundleSizeBytes, //initial resize (don't waste memory with potential max bundle size)
        boost::bind(&BpOverEncapLocalStreamInduct::OnFullEncapPacketReceived, this,
            boost::placeholders::_1, boost::placeholders::_2, boost::placeholders::_3),
        boost::bind(&BpOverEncapLocalStreamInduct::OnLocalStreamConnectionStatusChanged, this,
            boost::placeholders::_1),
        false), //false => discard encap header (hdtn only cares about the bundle)
    M_MAX_BUNDLE_SIZE_BYTES(maxBundleSizeBytes),
    m_opportunisticEncapHeaderBeingSent{0, 0, 0, 0, 0, 0, 0, 0},
    m_writeInProgress(false),
    m_sendErrorOccurred(false),
    //telemetry
    M_CONNECTION_NAME(inductConfig.bpEncapLocalSocketOrPipePath),
    M_INPUT_NAME(
#ifdef STREAM_USE_WINDOWS_NAMED_PIPE
        "pipe"
#else
        "AF_UNIX"
#endif
    ),
    m_totalBundleBytesReceived(0),
    m_totalBundlesReceived(0),
    m_totalOpportunisticBundleBytesSent(0),
    m_totalOpportunisticBundlesSent(0),
    m_totalOpportunisticBundleBytesSentAndAcked(0),
    m_totalOpportunisticBundlesSentAndAcked(0),
    m_totalOpportunisticBundlesFailedToSend(0),
    m_totalOpportunisticEncapHeaderBytesSent(0),
    m_totalEncapHeaderBytesReceived(0),
    m_largestEncapHeaderSizeBytesReceived(0),
    m_smallestEncapHeaderSizeBytesReceived(UINT64_MAX),
    m_opportunisticBundleQueuePtr(NULL)
    
{    
    if(!m_encapAsyncDuplexLocalStream.Init(inductConfig.bpEncapLocalSocketOrPipePath, true)) {
        LOG_FATAL(subprocess) << "cannot init BP over Encap";
        return;
    }

    m_mapNodeIdToOpportunisticBundleQueueMutex.lock();
    m_mapNodeIdToOpportunisticBundleQueue.erase(inductConfig.remoteNodeId);
    OpportunisticBundleQueue& opportunisticBundleQueue = m_mapNodeIdToOpportunisticBundleQueue[inductConfig.remoteNodeId];
    opportunisticBundleQueue.m_maxTxBundlesInPipeline = maxTxBundlesInFlight;
    opportunisticBundleQueue.m_remoteNodeId = inductConfig.remoteNodeId;
    m_opportunisticBundleQueuePtr = &opportunisticBundleQueue;
    m_mapNodeIdToOpportunisticBundleQueueMutex.unlock();

    m_ioServiceThreadPtr = boost::make_unique<boost::thread>(boost::bind(&boost::asio::io_service::run, &m_ioService));
    ThreadNamer::SetIoServiceThreadName(m_ioService, "ioServiceBpEncapSink");

    m_onNewOpportunisticLinkCallback = onNewOpportunisticLinkCallback;
    m_onDeletedOpportunisticLinkCallback = onDeletedOpportunisticLinkCallback;
}
BpOverEncapLocalStreamInduct::~BpOverEncapLocalStreamInduct() {
    m_encapAsyncDuplexLocalStream.Stop();

    if (m_ioServiceThreadPtr) {
        m_ioService.stop();
        try {
            m_ioServiceThreadPtr->join();
            m_ioServiceThreadPtr.reset(); //delete it
        }
        catch (const boost::thread_resource_error&) {
            LOG_ERROR(subprocess) << "error stopping BpOverEncapBundleSink io_service thread";
        }
        //print stats once
        LOG_INFO(subprocess) << "BpOverEncap Induct Connection:"
            << "\n connectionName " << M_CONNECTION_NAME
            << "\n inputName " << M_INPUT_NAME
            << "\n totalBundleBytesReceived " << m_totalBundleBytesReceived
            << "\n totalBundlesReceived " << m_totalBundlesReceived
            << "\n totalEncapHeaderBytesReceived " << m_totalEncapHeaderBytesReceived
            << "\n largestEncapHeaderSizeBytesReceived " << m_largestEncapHeaderSizeBytesReceived
            << "\n smallestEncapHeaderSizeBytesReceived " << m_smallestEncapHeaderSizeBytesReceived
            << "\n totalOpportunisticBundleBytesSent " << m_totalOpportunisticBundleBytesSent
            << "\n totalOpportunisticBundlesSent " << m_totalOpportunisticBundlesSent
            << "\n totalOpportunisticBundleBytesSentAndAcked " << m_totalOpportunisticBundleBytesSentAndAcked
            << "\n totalOpportunisticBundlesSentAndAcked " << m_totalOpportunisticBundlesSentAndAcked
            << "\n totalOpportunisticBundlesFailedToSend " << m_totalOpportunisticBundlesFailedToSend
            << "\n totalOpportunisticEncapHeaderBytesSent " << m_totalOpportunisticEncapHeaderBytesSent;
    }
}

void BpOverEncapLocalStreamInduct::OnLocalStreamConnectionStatusChanged(bool isOnConnectionEvent) {
    LOG_INFO(subprocess) << "BpOverEncapLocalStreamInduct connection " << ((isOnConnectionEvent) ? "up" : "down");
    if (isOnConnectionEvent) {
        if (m_onNewOpportunisticLinkCallback) {
            m_onNewOpportunisticLinkCallback(m_inductConfig.remoteNodeId, this, NULL);
        }
    }
    else {
        if (m_onDeletedOpportunisticLinkCallback) {
            m_onDeletedOpportunisticLinkCallback(m_inductConfig.remoteNodeId, this, NULL);
        }
    }
}


void BpOverEncapLocalStreamInduct::TrySendBundleIfAvailable_NotThreadSafe() {
    //only do this if idle (no write in progress)
    if (!m_writeInProgress) {
        if (m_sendErrorOccurred) {
            //prevent bundle from being sent
            //EmptySendQueueOnFailure();
            //DoFailedBundleCallback(el); //notify first
            //m_txBundlesCb.CommitRead(); //cb is sized 1 larger in case a Forward() is called between notify and CommitRead
        }
        else {
            if (BundleSinkTryGetData_FromIoServiceThread(*m_opportunisticBundleQueuePtr, m_opportunisticBundleDataPairBeingSent)) {
                //encode encap header
                uint8_t encodedEncapHeaderSize;
                uint64_t bundleSize;
                uint8_t* bundleData;
                if (m_opportunisticBundleDataPairBeingSent.first) { //zmq data
                    bundleSize = m_opportunisticBundleDataPairBeingSent.first->size();
                    bundleData = (uint8_t*) m_opportunisticBundleDataPairBeingSent.first->data();
                }
                else { //vec data
                    bundleSize = m_opportunisticBundleDataPairBeingSent.second.size();
                    bundleData = m_opportunisticBundleDataPairBeingSent.second.data();
                }
                if (!GetCcsdsEncapHeader(ENCAP_PACKET_TYPE::BP, m_opportunisticEncapHeaderBeingSent, static_cast<uint32_t>(bundleSize), encodedEncapHeaderSize)) {
                    LOG_FATAL(subprocess) << "BpOverEncapLocalStreamInduct::TrySendBundleIfAvailable_NotThreadSafe.. unable to encode encap header";
                    return;
                }

                m_opportunisticConstBuffersBeingSent.resize(2);
                m_opportunisticConstBuffersBeingSent[0] = boost::asio::buffer(m_opportunisticEncapHeaderBeingSent, encodedEncapHeaderSize);
                m_opportunisticConstBuffersBeingSent[1] = boost::asio::buffer(bundleData, bundleSize);

                m_writeInProgress = true;
                m_totalOpportunisticBundlesSent.fetch_add(1, std::memory_order_relaxed);
                m_totalOpportunisticBundleBytesSent.fetch_add(bundleSize, std::memory_order_relaxed);
                m_totalOpportunisticEncapHeaderBytesSent.fetch_add(encodedEncapHeaderSize, std::memory_order_relaxed);
                boost::asio::async_write(m_encapAsyncDuplexLocalStream.GetStreamHandleRef(),
                    m_opportunisticConstBuffersBeingSent,
                    boost::bind(&BpOverEncapLocalStreamInduct::HandleSend, this,
                        boost::asio::placeholders::error,
                        boost::asio::placeholders::bytes_transferred,
                        encodedEncapHeaderSize));
            }
        }
    }
}

void BpOverEncapLocalStreamInduct::HandleSend(const boost::system::error_code& error, std::size_t bytes_transferred, uint8_t encodedEncapHeaderSize) {
    m_writeInProgress = false;
    if (error) {
        m_sendErrorOccurred = true;
        LOG_ERROR(subprocess) << "BpOverEncapLocalStreamInduct::HandleSend: " << error.message();
        m_totalOpportunisticBundlesFailedToSend.fetch_add(1, std::memory_order_relaxed);
        //empty the queue
        //EmptySendQueueOnFailure();
    }
    else {
        ++m_totalOpportunisticBundlesSentAndAcked;
        m_totalOpportunisticBundleBytesSentAndAcked += (bytes_transferred - encodedEncapHeaderSize);
        //skip the following for induct sending opportunistic bundles?
        //if (m_onSuccessfulBundleSendCallback) { //notify first
        //    m_onSuccessfulBundleSendCallback(el.m_userData, m_userAssignedUuid);
        //}
        //m_txBundlesCb.CommitRead(); //cb is sized 1 larger in case a Forward() is called between notify and CommitRead
        //if (m_useLocalConditionVariableAckReceived.load(std::memory_order_acquire)) { //for destructor
        //    m_localConditionVariableAckReceived.notify_one();
        //}
        TrySendBundleIfAvailable_NotThreadSafe();
    }
}

void BpOverEncapLocalStreamInduct::TrySendBundleIfAvailable_ThreadSafe() {
    boost::asio::post(m_ioService, boost::bind(&BpOverEncapLocalStreamInduct::TrySendBundleIfAvailable_NotThreadSafe, this));
}

void BpOverEncapLocalStreamInduct::NotifyBundleReadyToSend_FromIoServiceThread(const uint64_t remoteNodeId) {
    if (!m_encapAsyncDuplexLocalStream.ReadyToSend()) {
        LOG_ERROR(subprocess) << "opportunistic link unavailable";
        return;
    }
    if (m_inductConfig.remoteNodeId != remoteNodeId) {
        LOG_ERROR(subprocess) << "BpOverEncapLocalStreamInduct remote node mismatch: expected "
            << m_inductConfig.remoteNodeId << " but got " << remoteNodeId;
        return;
    }
    TrySendBundleIfAvailable_NotThreadSafe();
}

void BpOverEncapLocalStreamInduct::Virtual_PostNotifyBundleReadyToSend_FromIoServiceThread(const uint64_t remoteNodeId) {
    boost::asio::post(m_ioService, boost::bind(&BpOverEncapLocalStreamInduct::NotifyBundleReadyToSend_FromIoServiceThread, this, remoteNodeId));
}



void BpOverEncapLocalStreamInduct::OnFullEncapPacketReceived(padded_vector_uint8_t& receivedFullEncapPacket,
    uint32_t decodedEncapPayloadSize, uint8_t decodedEncapHeaderSize)
{
    //note: decodedEncapHeaderSize still set even though it is omitted from receivedFullEncapPacket
    if (receivedFullEncapPacket.size() <= M_MAX_BUNDLE_SIZE_BYTES) {
        m_totalBundleBytesReceived.fetch_add(receivedFullEncapPacket.size(), std::memory_order_relaxed);
        m_totalBundlesReceived.fetch_add(1, std::memory_order_relaxed);
        m_totalEncapHeaderBytesReceived.fetch_add(decodedEncapHeaderSize, std::memory_order_relaxed);
        if (m_largestEncapHeaderSizeBytesReceived.load(std::memory_order_acquire) < decodedEncapHeaderSize) {
            m_largestEncapHeaderSizeBytesReceived.store(decodedEncapHeaderSize, std::memory_order_release);
        }
        if (m_smallestEncapHeaderSizeBytesReceived.load(std::memory_order_acquire) > decodedEncapHeaderSize) {
            m_smallestEncapHeaderSizeBytesReceived.store(decodedEncapHeaderSize, std::memory_order_release);
        }
        m_inductProcessBundleCallback(receivedFullEncapPacket); //receivedFullEncapPacket is just the bundle (encap header discarded)
    }
    else {
        LOG_WARNING(subprocess) << "BpOverEncapLocalStreamInduct RX bundle exceeded size limit of "
            << M_MAX_BUNDLE_SIZE_BYTES << " bytes from previous node "
            << m_inductConfig.remoteNodeId << "..dropping bundle!";
    }
    //called by the io_service thread (can use non-thread-safe function calls)
    m_encapAsyncDuplexLocalStream.StartReadFirstEncapHeaderByte_NotThreadSafe();
}

void BpOverEncapLocalStreamInduct::PopulateInductTelemetry(InductTelemetry_t& inductTelem) {
    //m_uartInterface.SyncTelemetry();
    inductTelem.m_convergenceLayer = "bp_over_encap_local_stream";
    inductTelem.m_listInductConnections.clear();
    
    std::unique_ptr<BpOverEncapLocalStreamInductConnectionTelemetry_t> telemPtr = boost::make_unique<BpOverEncapLocalStreamInductConnectionTelemetry_t>();
    BpOverEncapLocalStreamInductConnectionTelemetry_t& telem = *telemPtr;
    telem.m_connectionName = M_CONNECTION_NAME;
    telem.m_inputName = M_INPUT_NAME;
    telem.m_totalBundleBytesReceived = m_totalBundleBytesReceived.load(std::memory_order_acquire);
    telem.m_totalBundlesReceived = m_totalBundlesReceived.load(std::memory_order_acquire);

    telem.m_totalEncapHeaderBytesSent = m_totalOpportunisticEncapHeaderBytesSent.load(std::memory_order_acquire);
    telem.m_totalEncapHeaderBytesReceived = m_totalEncapHeaderBytesReceived.load(std::memory_order_acquire);
    telem.m_largestEncapHeaderSizeBytesReceived = m_largestEncapHeaderSizeBytesReceived.load(std::memory_order_acquire);
    telem.m_smallestEncapHeaderSizeBytesReceived = m_smallestEncapHeaderSizeBytesReceived.load(std::memory_order_acquire);
    if (telem.m_totalBundlesReceived) {
        telem.m_averageEncapHeaderSizeBytesReceived = telem.m_totalEncapHeaderBytesReceived / telem.m_totalBundlesReceived;
    }
    else {
        telem.m_averageEncapHeaderSizeBytesReceived = 0;
    }

    //bidirectionality (identical to OutductTelemetry_t)
    telem.m_totalBundlesSentAndAcked = m_totalOpportunisticBundlesSentAndAcked.load(std::memory_order_acquire);
    telem.m_totalBundleBytesSentAndAcked = m_totalOpportunisticBundleBytesSentAndAcked.load(std::memory_order_acquire);
    telem.m_totalBundlesSent = m_totalOpportunisticBundlesSent.load(std::memory_order_acquire);
    telem.m_totalBundleBytesSent = m_totalOpportunisticBundleBytesSent.load(std::memory_order_acquire);
    telem.m_totalBundlesFailedToSend = m_totalOpportunisticBundlesFailedToSend.load(std::memory_order_acquire);

    inductTelem.m_listInductConnections.emplace_back(std::move(telemPtr));
}

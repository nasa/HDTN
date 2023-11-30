/**
 * @file BpOverEncapBundleSink.cpp
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
#include "Logger.h"
#include "BpOverEncapBundleSink.h"
#include <boost/make_unique.hpp>
#include "ThreadNamer.h"

static constexpr hdtn::Logger::SubProcess subprocess = hdtn::Logger::SubProcess::none;

BpOverEncapBundleSink::BpOverEncapBundleSink(const WholeBundleReadyCallback_t& wholeBundleReadyCallback,
    const uint64_t maxBundleSizeBytes,
    const std::string& socketOrPipePath) :

    m_encapAsyncDuplexLocalStream(m_ioService, //ltp engine will handle i/o, keeping entirely single threaded
        ENCAP_PACKET_TYPE::BP,
        1,//maxBundleSizeBytes, //initial resize (don't waste memory with potential max bundle size)
        boost::bind(&BpOverEncapBundleSink::OnFullEncapPacketReceived, this,
            boost::placeholders::_1, boost::placeholders::_2, boost::placeholders::_3),
        boost::bind(&BpOverEncapBundleSink::OnLocalStreamConnectionStatusChanged, this,
            boost::placeholders::_1),
        false), //false => discard encap header (hdtn only cares about the bundle)
    m_wholeBundleReadyCallback(wholeBundleReadyCallback),
    M_MAX_BUNDLE_SIZE_BYTES(maxBundleSizeBytes),
    //telemetry
    M_CONNECTION_NAME(socketOrPipePath),
    M_INPUT_NAME(
#ifdef STREAM_USE_WINDOWS_NAMED_PIPE
        "pipe"
#else
        "AF_UNIX"
#endif
    ),
    m_totalBundleBytesReceived(0),
    m_totalBundlesReceived(0)
{
    if (!m_encapAsyncDuplexLocalStream.Init(socketOrPipePath, true)) {
        LOG_FATAL(subprocess) << "cannot init BP over Encap";
        return;
    }

    m_ioServiceThreadPtr = boost::make_unique<boost::thread>(boost::bind(&boost::asio::io_service::run, &m_ioService));
    ThreadNamer::SetIoServiceThreadName(m_ioService, "ioServiceBpEncapSink");
}

BpOverEncapBundleSink::~BpOverEncapBundleSink() {

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
        LOG_INFO(subprocess) << "BpOverEncap Bundle Sink / Induct Connection:"
            << "\n connectionName " << M_CONNECTION_NAME
            << "\n inputName " << M_INPUT_NAME
            << "\n totalBundleBytesReceived " << m_totalBundleBytesReceived
            << "\n totalBundlesReceived " << m_totalBundlesReceived;
    }
}

void BpOverEncapBundleSink::OnLocalStreamConnectionStatusChanged(bool isOnConnectionEvent) {
    LOG_INFO(subprocess) << "BpOverEncapBundleSink connection " << ((isOnConnectionEvent) ? "up" : "down");
}


void BpOverEncapBundleSink::OnFullEncapPacketReceived(padded_vector_uint8_t& receivedFullEncapPacket,
    uint32_t decodedEncapPayloadSize, uint8_t decodedEncapHeaderSize)
{
    m_totalBundleBytesReceived.fetch_add(receivedFullEncapPacket.size(), std::memory_order_relaxed);
    m_totalBundlesReceived.fetch_add(1, std::memory_order_relaxed);
    m_wholeBundleReadyCallback(receivedFullEncapPacket); //receivedFullEncapPacket is just the bundle (encap header discarded)
    //called by the io_service thread (can use non-thread-safe function calls)
    m_encapAsyncDuplexLocalStream.StartReadFirstEncapHeaderByte_NotThreadSafe();
}

void BpOverEncapBundleSink::GetTelemetry(InductConnectionTelemetry_t& telem) const {
    telem.m_connectionName = M_CONNECTION_NAME;
    telem.m_inputName = M_INPUT_NAME;
    telem.m_totalBundleBytesReceived = m_totalBundleBytesReceived.load(std::memory_order_acquire);
    telem.m_totalBundlesReceived = m_totalBundlesReceived.load(std::memory_order_acquire);
}

/**
 * @file StcpBundleSink.h
 * @author  Brian Tomko <brian.j.tomko@nasa.gov>
 *
 * @copyright Copyright (c) 2021 United States Government as represented by
 * the National Aeronautics and Space Administration.
 * No copyright is claimed in the United States under Title 17, U.S.Code.
 * All Other Rights Reserved.
 *
 * @section LICENSE
 * Released under the NASA Open Source Agreement (NOSA)
 * See LICENSE.md in the source root directory for more information.
 *
 * @section DESCRIPTION
 *
 * This StcpBundleSink class encapsulates the appropriate "DTN simple TCP convergence layer (STCP)" functionality
 * to receive bundles (or any other user defined data) over an STCP link
 * and calls the user defined function WholeBundleReadyCallback_t when a new bundle
 * is received.
 * This class is implemented based on the ION.pdf V4.0.1 sections STCPCLI and STCPCLO.
 */

#ifndef _STCP_BUNDLE_SINK_H
#define _STCP_BUNDLE_SINK_H 1

#include <stdint.h>
#include <boost/asio.hpp>
#include <boost/thread.hpp>
#include <boost/function.hpp>
#include <memory>
#include "CircularIndexBufferSingleProducerSingleConsumerConfigurable.h"
#include "PaddedVectorUint8.h"
#include "TelemetryDefinitions.h"
#include "stcp_lib_export.h"
#include <atomic>

class StcpBundleSink {
private:
    StcpBundleSink();
public:
    typedef boost::function<void(padded_vector_uint8_t & wholeBundleVec)> WholeBundleReadyCallback_t;
    typedef boost::function<void()> NotifyReadyToDeleteCallback_t;

    STCP_LIB_EXPORT StcpBundleSink(std::shared_ptr<boost::asio::ip::tcp::socket> tcpSocketPtr,
        boost::asio::io_service & tcpSocketIoServiceRef,
        const WholeBundleReadyCallback_t & wholeBundleReadyCallback,
        const unsigned int numCircularBufferVectors,
        const uint64_t maxBundleSizeBytes,
        const NotifyReadyToDeleteCallback_t & notifyReadyToDeleteCallback = NotifyReadyToDeleteCallback_t());
    STCP_LIB_EXPORT ~StcpBundleSink();
    STCP_LIB_EXPORT bool ReadyToBeDeleted();
    STCP_LIB_EXPORT void GetTelemetry(StcpInductConnectionTelemetry_t& telem) const;
private:

    STCP_LIB_NO_EXPORT void TryStartTcpReceive();
    STCP_LIB_NO_EXPORT void HandleTcpReceiveIncomingBundleSize(const boost::system::error_code & error, std::size_t bytesTransferred, const unsigned int writeIndex);
    STCP_LIB_NO_EXPORT void HandleTcpReceiveBundleData(const boost::system::error_code & error, std::size_t bytesTransferred);
    STCP_LIB_NO_EXPORT void PopCbThreadFunc();
    STCP_LIB_NO_EXPORT void DoStcpShutdown();
    STCP_LIB_NO_EXPORT void HandleSocketShutdown();


private:
    
    const WholeBundleReadyCallback_t m_wholeBundleReadyCallback;
    const NotifyReadyToDeleteCallback_t m_notifyReadyToDeleteCallback;

    std::shared_ptr<boost::asio::ip::tcp::socket> m_tcpSocketPtr;
    boost::asio::io_service & m_tcpSocketIoServiceRef;

    const unsigned int M_NUM_CIRCULAR_BUFFER_VECTORS;
    const uint64_t M_MAX_BUNDLE_SIZE_BYTES;
    CircularIndexBufferSingleProducerSingleConsumerConfigurable m_circularIndexBuffer;
    std::vector<padded_vector_uint8_t > m_tcpReceiveBuffersCbVec;
    boost::condition_variable m_conditionVariableCb;
    boost::mutex m_mutexCb;
    std::unique_ptr<boost::thread> m_threadCbReaderPtr;
    bool m_stateTcpReadActive;
    bool m_printedCbTooSmallNotice;
    std::atomic<bool> m_running;
    std::atomic<bool> m_safeToDelete;
    uint32_t m_incomingBundleSize;

    //telemetry
    const std::string M_CONNECTION_NAME;
    const std::string M_INPUT_NAME;
    std::atomic<uint64_t> m_totalStcpBytesReceived;
    std::atomic<uint64_t> m_totalBundleBytesReceived;
    std::atomic<uint64_t> m_totalBundlesReceived;
};



#endif  //_STCP_BUNDLE_SINK_H

/**
 * @file BpOverEncapBundleSink.h
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
 *
 * @section DESCRIPTION
 *
 * This BpOverEncapBundleSink class encapsulates the appropriate BpOverEncap functionality
 * to receive bundles (or any other user defined data) over an BpOverEncap link
 * and calls the user defined function WholeBundleReadyCallback_t when a new bundle
 * is received.
 */

#ifndef _BP_OVER_ENCAP_BUNDLE_SINK_H
#define _BP_OVER_ENCAP_BUNDLE_SINK_H 1

#include <cstdint>
#include <boost/asio.hpp>
#include <boost/thread.hpp>
#include <boost/function.hpp>
#include <memory>
#include "PaddedVectorUint8.h"
#include "TelemetryDefinitions.h"
#include "bp_over_encap_lib_export.h"
#include <atomic>
#include "EncapAsyncDuplexLocalStream.h"

class BpOverEncapBundleSink {
private:
    BpOverEncapBundleSink() = delete;
public:
    typedef boost::function<void(padded_vector_uint8_t & wholeBundleVec)> WholeBundleReadyCallback_t;

    BP_OVER_ENCAP_LIB_EXPORT BpOverEncapBundleSink(
        const WholeBundleReadyCallback_t & wholeBundleReadyCallback,
        const uint64_t maxBundleSizeBytes,
        const std::string& socketOrPipePath);
    BP_OVER_ENCAP_LIB_EXPORT ~BpOverEncapBundleSink();
    BP_OVER_ENCAP_LIB_EXPORT void GetTelemetry(/*BpOverEncap*/InductConnectionTelemetry_t& telem) const;
private:

    BP_OVER_ENCAP_LIB_NO_EXPORT void OnFullEncapPacketReceived(padded_vector_uint8_t& receivedFullEncapPacket,
        uint32_t decodedEncapPayloadSize, uint8_t decodedEncapHeaderSize);
    BP_OVER_ENCAP_LIB_NO_EXPORT void OnLocalStreamConnectionStatusChanged(bool isOnConnectionEvent);


private:

    boost::asio::io_service m_ioService;
    EncapAsyncDuplexLocalStream m_encapAsyncDuplexLocalStream;
    std::unique_ptr<boost::thread> m_ioServiceThreadPtr;
    
    const WholeBundleReadyCallback_t m_wholeBundleReadyCallback;

    

    const uint64_t M_MAX_BUNDLE_SIZE_BYTES;

    //telemetry
    const std::string M_CONNECTION_NAME;
    const std::string M_INPUT_NAME;
    std::atomic<uint64_t> m_totalBundleBytesReceived;
    std::atomic<uint64_t> m_totalBundlesReceived;
};



#endif  //_BP_OVER_ENCAP_BUNDLE_SINK_H

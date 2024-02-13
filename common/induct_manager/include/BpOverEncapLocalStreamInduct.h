/**
 * @file BpOverEncapLocalStreamInduct.h
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
 * The BpOverEncapLocalStreamInduct class contains the functionality for a Bp Over Encap Local Stream induct
 * used by the InductManager.  This class is the interface to bp_over_encap_lib.
 */

#ifndef BP_OVER_ENCAP_LOCAL_STREAM_INDUCT_H
#define BP_OVER_ENCAP_LOCAL_STREAM_INDUCT_H 1

#include <cstdint>
#include <boost/asio.hpp>
#include <boost/thread.hpp>
#include <boost/function.hpp>
#include <memory>
#include "PaddedVectorUint8.h"
#include "TelemetryDefinitions.h"
#include <atomic>
#include "EncapAsyncDuplexLocalStream.h"
#include <string>
#include "Induct.h"
#include <memory>
#include <atomic>

class CLASS_VISIBILITY_INDUCT_MANAGER_LIB BpOverEncapLocalStreamInduct : public Induct {
public:

    INDUCT_MANAGER_LIB_EXPORT BpOverEncapLocalStreamInduct(const InductProcessBundleCallback_t & inductProcessBundleCallback, const induct_element_config_t & inductConfig,
        const uint64_t maxBundleSizeBytes, const OnNewOpportunisticLinkCallback_t & onNewOpportunisticLinkCallback,
        const OnDeletedOpportunisticLinkCallback_t & onDeletedOpportunisticLinkCallback);
    INDUCT_MANAGER_LIB_EXPORT virtual ~BpOverEncapLocalStreamInduct() override;
    INDUCT_MANAGER_LIB_EXPORT virtual void PopulateInductTelemetry(InductTelemetry_t& inductTelem) override;
protected:
    
    INDUCT_MANAGER_LIB_EXPORT virtual void Virtual_PostNotifyBundleReadyToSend_FromIoServiceThread(const uint64_t remoteNodeId) override;
private:
    

    BpOverEncapLocalStreamInduct();
    INDUCT_MANAGER_LIB_NO_EXPORT void OnFullEncapPacketReceived(padded_vector_uint8_t& receivedFullEncapPacket,
        uint32_t decodedEncapPayloadSize, uint8_t decodedEncapHeaderSize);

    INDUCT_MANAGER_LIB_NO_EXPORT void OnLocalStreamConnectionStatusChanged(bool isOnConnectionEvent);
    INDUCT_MANAGER_LIB_NO_EXPORT void NotifyBundleReadyToSend_FromIoServiceThread(const uint64_t remoteNodeId);

    INDUCT_MANAGER_LIB_NO_EXPORT void TrySendBundleIfAvailable_NotThreadSafe();
    INDUCT_MANAGER_LIB_NO_EXPORT void TrySendBundleIfAvailable_ThreadSafe();
    INDUCT_MANAGER_LIB_NO_EXPORT void HandleSend(const boost::system::error_code& error, std::size_t bytes_transferred, uint8_t encodedEncapHeaderSize);

    //BpOverEncapBundleSink m_bpOverEncapBundleSink;
    boost::asio::io_service m_ioService;
    EncapAsyncDuplexLocalStream m_encapAsyncDuplexLocalStream;
    std::unique_ptr<boost::thread> m_ioServiceThreadPtr;



    const uint64_t M_MAX_BUNDLE_SIZE_BYTES;

    //todo
    //
    uint8_t m_opportunisticEncapHeaderBeingSent[8];
    std::pair<std::unique_ptr<zmq::message_t>, padded_vector_uint8_t> m_opportunisticBundleDataPairBeingSent;
    std::vector<boost::asio::const_buffer> m_opportunisticConstBuffersBeingSent;
    std::atomic<bool> m_writeInProgress;
    std::atomic<bool> m_sendErrorOccurred;

    //telemetry
    const std::string M_CONNECTION_NAME;
    const std::string M_INPUT_NAME;
    std::atomic<uint64_t> m_totalBundleBytesReceived;
    std::atomic<uint64_t> m_totalBundlesReceived;
    std::atomic<uint64_t> m_totalOpportunisticBundleBytesSent;
    std::atomic<uint64_t> m_totalOpportunisticBundlesSent;
    std::atomic<uint64_t> m_totalOpportunisticBundleBytesSentAndAcked;
    std::atomic<uint64_t> m_totalOpportunisticBundlesSentAndAcked;
    std::atomic<uint64_t> m_totalOpportunisticBundlesFailedToSend;
    std::atomic<uint64_t> m_totalOpportunisticEncapHeaderBytesSent;
    std::atomic<uint64_t> m_totalEncapHeaderBytesReceived;
    std::atomic<uint64_t> m_largestEncapHeaderSizeBytesReceived;
    std::atomic<uint64_t> m_smallestEncapHeaderSizeBytesReceived;

    OpportunisticBundleQueue* m_opportunisticBundleQueuePtr;

    

    
};


#endif // BP_OVER_ENCAP_LOCAL_STREAM_INDUCT_H


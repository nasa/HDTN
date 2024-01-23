/**
 * @file BpOverEncapLocalStreamOutduct.h
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
 * The BpOverEncapLocalStreamOutduct class contains the functionality for a Bp Over Encap Local Stream outduct
 * used by the OutductManager.  This class is the interface to bp_over_encap_lib.
 */

#ifndef BP_OVER_ENCAP_LOCAL_STREAM_OUTDUCT_H
#define BP_OVER_ENCAP_LOCAL_STREAM_OUTDUCT_H 1

#include <string>
#include "Outduct.h"
#include <boost/thread.hpp>
#include <boost/asio.hpp>
#include <memory>
#include <atomic>
#include "CircularIndexBufferSingleProducerSingleConsumerConfigurable.h"
#include "TelemetryDefinitions.h"
#include "TcpAsyncSender.h"
#include "BundleCallbackFunctionDefines.h"
#include "EncapAsyncDuplexLocalStream.h"

class CLASS_VISIBILITY_OUTDUCT_MANAGER_LIB BpOverEncapLocalStreamOutduct : public Outduct {
public:
    OUTDUCT_MANAGER_LIB_EXPORT BpOverEncapLocalStreamOutduct(const outduct_element_config_t & outductConfig, const uint64_t outductUuid,
        const OutductOpportunisticProcessReceivedBundleCallback_t & outductOpportunisticProcessReceivedBundleCallback = OutductOpportunisticProcessReceivedBundleCallback_t());
    OUTDUCT_MANAGER_LIB_EXPORT virtual ~BpOverEncapLocalStreamOutduct() override;
    OUTDUCT_MANAGER_LIB_EXPORT virtual void PopulateOutductTelemetry(std::unique_ptr<OutductTelemetry_t>& outductTelem) override;
    OUTDUCT_MANAGER_LIB_EXPORT virtual std::size_t GetTotalBundlesUnacked() const noexcept override;
    OUTDUCT_MANAGER_LIB_EXPORT std::size_t GetTotalBundlesAcked() const noexcept;
    OUTDUCT_MANAGER_LIB_EXPORT std::size_t GetTotalBundlesSent() const noexcept;
    OUTDUCT_MANAGER_LIB_EXPORT std::size_t GetTotalBundleBytesAcked() const noexcept;
    OUTDUCT_MANAGER_LIB_EXPORT std::size_t GetTotalBundleBytesSent() const noexcept;
    OUTDUCT_MANAGER_LIB_EXPORT std::size_t GetTotalBundleBytesUnacked() const noexcept;
    OUTDUCT_MANAGER_LIB_EXPORT virtual bool Forward(const uint8_t* bundleData, const std::size_t size, std::vector<uint8_t>&& userData) override;
    OUTDUCT_MANAGER_LIB_EXPORT virtual bool Forward(zmq::message_t & movableDataZmq, std::vector<uint8_t>&& userData) override;
    OUTDUCT_MANAGER_LIB_EXPORT virtual bool Forward(padded_vector_uint8_t& movableDataVec, std::vector<uint8_t>&& userData) override;
    OUTDUCT_MANAGER_LIB_EXPORT virtual void SetOnFailedBundleVecSendCallback(const OnFailedBundleVecSendCallback_t& callback) override;
    OUTDUCT_MANAGER_LIB_EXPORT virtual void SetOnFailedBundleZmqSendCallback(const OnFailedBundleZmqSendCallback_t& callback) override;
    OUTDUCT_MANAGER_LIB_EXPORT virtual void SetOnSuccessfulBundleSendCallback(const OnSuccessfulBundleSendCallback_t& callback) override;
    OUTDUCT_MANAGER_LIB_EXPORT virtual void SetOnOutductLinkStatusChangedCallback(const OnOutductLinkStatusChangedCallback_t& callback) override;
    OUTDUCT_MANAGER_LIB_EXPORT virtual void SetUserAssignedUuid(uint64_t userAssignedUuid) override;
    OUTDUCT_MANAGER_LIB_EXPORT virtual void Connect() override;
    OUTDUCT_MANAGER_LIB_EXPORT virtual bool ReadyToForward() override;
    OUTDUCT_MANAGER_LIB_EXPORT virtual void Stop() override;
    OUTDUCT_MANAGER_LIB_EXPORT virtual void GetOutductFinalStats(OutductFinalStats & finalStats) override;

private:
    OUTDUCT_MANAGER_LIB_NO_EXPORT void DoFailedBundleCallback(TcpAsyncSenderElement& el);
    OUTDUCT_MANAGER_LIB_NO_EXPORT void TrySendDataIfAvailable_NotThreadSafe();
    OUTDUCT_MANAGER_LIB_NO_EXPORT void TrySendDataIfAvailable_ThreadSafe();
    OUTDUCT_MANAGER_LIB_NO_EXPORT void HandleSend(const boost::system::error_code& error, std::size_t bytes_transferred, const unsigned int readIndex);
    OUTDUCT_MANAGER_LIB_NO_EXPORT static void GenerateDataUnitHeaderOnly(std::vector<uint8_t>& dataUnit, uint32_t bundleSizeBytes);
    OUTDUCT_MANAGER_LIB_NO_EXPORT void OnFullEncapPacketReceived(padded_vector_uint8_t& receivedFullEncapPacket,
        uint32_t decodedEncapPayloadSize, uint8_t decodedEncapHeaderSize);
    OUTDUCT_MANAGER_LIB_NO_EXPORT void OnLocalStreamConnectionStatusChanged(bool isOnConnectionEvent);

private:
    BpOverEncapLocalStreamOutduct();


    boost::asio::io_service m_ioService;
    boost::asio::io_service::work m_work;
    EncapAsyncDuplexLocalStream m_encapAsyncDuplexLocalStream;
    std::unique_ptr<boost::thread> m_ioServiceThreadPtr;
    boost::condition_variable m_localConditionVariableAckReceived;

    const unsigned int MAX_UNACKED;
    CircularIndexBufferSingleProducerSingleConsumerConfigurable m_toSendCb;
    std::vector<std::size_t> m_bytesToAckBySendCallbackCbVec;
    std::vector<TcpAsyncSenderElement> m_tcpAsyncSenderElementsCbVec;
    std::atomic<bool> m_writeInProgress;
    std::atomic<bool> m_sendErrorOccurred;
    std::atomic<bool> m_useLocalConditionVariableAckReceived;

    OnFailedBundleVecSendCallback_t m_onFailedBundleVecSendCallback;
    OnFailedBundleZmqSendCallback_t m_onFailedBundleZmqSendCallback;
    OnSuccessfulBundleSendCallback_t m_onSuccessfulBundleSendCallback;
    OnOutductLinkStatusChangedCallback_t m_onOutductLinkStatusChangedCallback;
    uint64_t m_userAssignedUuid;

    //opportunistic receive bundles
    const OutductOpportunisticProcessReceivedBundleCallback_t m_outductOpportunisticProcessReceivedBundleCallback;

    //stats
    std::atomic<uint64_t> m_totalBundlesSent;
    std::atomic<uint64_t> m_totalBundlesAcked;
    std::atomic<uint64_t> m_totalBundleBytesSent;
    std::atomic<uint64_t> m_totalBundleBytesAcked;
    std::atomic<uint64_t> m_totalBundlesFailedToSend;
    std::atomic<uint64_t> m_totalEncapHeaderBytesSent;
    std::atomic<uint64_t> m_largestEncapHeaderSizeBytesSent;
    std::atomic<uint64_t> m_smallestEncapHeaderSizeBytesSent;
    //opportunistic stats
    std::atomic<uint64_t> m_totalBundleBytesReceived;
    std::atomic<uint64_t> m_totalBundlesReceived;
    std::atomic<uint64_t> m_totalEncapHeaderBytesReceived;
};


#endif // BP_OVER_ENCAP_LOCAL_STREAM_OUTDUCT_H


#ifndef TCPCLV4_OUTDUCT_H
#define TCPCLV4_OUTDUCT_H 1

#include <string>
#include "Outduct.h"
#include "TcpclV4BundleSource.h"
#include <list>

class CLASS_VISIBILITY_OUTDUCT_MANAGER_LIB TcpclV4Outduct : public Outduct {
public:
    OUTDUCT_MANAGER_LIB_EXPORT TcpclV4Outduct(const outduct_element_config_t & outductConfig, const uint64_t myNodeId, const uint64_t outductUuid,
        const uint64_t maxOpportunisticRxBundleSizeBytes,
        const OutductOpportunisticProcessReceivedBundleCallback_t & outductOpportunisticProcessReceivedBundleCallback = OutductOpportunisticProcessReceivedBundleCallback_t());
    OUTDUCT_MANAGER_LIB_EXPORT virtual ~TcpclV4Outduct();
    OUTDUCT_MANAGER_LIB_EXPORT virtual std::size_t GetTotalDataSegmentsUnacked();
    OUTDUCT_MANAGER_LIB_EXPORT virtual bool Forward(const uint8_t* bundleData, const std::size_t size, std::vector<uint8_t>&& userData);
    OUTDUCT_MANAGER_LIB_EXPORT virtual bool Forward(zmq::message_t & movableDataZmq, std::vector<uint8_t>&& userData);
    OUTDUCT_MANAGER_LIB_EXPORT virtual bool Forward(std::vector<uint8_t> & movableDataVec, std::vector<uint8_t>&& userData);
    OUTDUCT_MANAGER_LIB_EXPORT virtual void SetOnSuccessfulAckCallback(const OnSuccessfulOutductAckCallback_t & callback);
    OUTDUCT_MANAGER_LIB_EXPORT virtual void SetOnFailedBundleVecSendCallback(const OnFailedBundleVecSendCallback_t& callback);
    OUTDUCT_MANAGER_LIB_EXPORT virtual void SetOnFailedBundleZmqSendCallback(const OnFailedBundleZmqSendCallback_t& callback);
    OUTDUCT_MANAGER_LIB_EXPORT virtual void SetOnSuccessfulBundleSendCallback(const OnSuccessfulBundleSendCallback_t& callback);
    OUTDUCT_MANAGER_LIB_EXPORT virtual void SetOnOutductLinkStatusChangedCallback(const OnOutductLinkStatusChangedCallback_t& callback);
    OUTDUCT_MANAGER_LIB_EXPORT virtual void SetUserAssignedUuid(uint64_t userAssignedUuid);
    OUTDUCT_MANAGER_LIB_EXPORT virtual void Connect();
    OUTDUCT_MANAGER_LIB_EXPORT virtual bool ReadyToForward();
    OUTDUCT_MANAGER_LIB_EXPORT virtual void Stop();
    OUTDUCT_MANAGER_LIB_EXPORT virtual void GetOutductFinalStats(OutductFinalStats & finalStats);

private:
    TcpclV4Outduct();

#ifdef OPENSSL_SUPPORT_ENABLED
    boost::asio::ssl::context m_shareableSslContext;
    bool VerifyCertificate(bool preverified, boost::asio::ssl::verify_context& ctx, const std::string & nextHopEndpointIdStrWithServiceIdZero, bool doVerifyNextHopEndpointIdStr, bool doX509CertificateVerification);
#endif
    TcpclV4BundleSource m_tcpclV4BundleSource;

};


#endif // TCPCLV4_OUTDUCT_H


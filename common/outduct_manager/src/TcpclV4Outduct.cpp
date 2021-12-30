#include "TcpclV4Outduct.h"
#include <iostream>
#include <boost/make_unique.hpp>
#include <boost/make_shared.hpp>
#include <boost/lexical_cast.hpp>

TcpclV4Outduct::TcpclV4Outduct(const outduct_element_config_t & outductConfig, const uint64_t myNodeId, const uint64_t outductUuid,
    const OutductOpportunisticProcessReceivedBundleCallback_t & outductOpportunisticProcessReceivedBundleCallback) :
    Outduct(outductConfig, outductUuid),

#ifdef OPENSSL_SUPPORT_ENABLED
    m_shareableSslContext(boost::asio::ssl::context::tlsv12_client),
    m_tcpclV4BundleSource(m_shareableSslContext,
#else
    m_tcpclV4BundleSource(
#endif
    outductConfig.keepAliveIntervalSeconds, myNodeId, outductConfig.nextHopEndpointId,
        outductConfig.bundlePipelineLimit + 5, outductConfig.tcpclAutoFragmentSizeBytes, outductOpportunisticProcessReceivedBundleCallback)
{
#ifdef OPENSSL_SUPPORT_ENABLED
    if (true){//M_BASE_TRY_USE_TLS) {
        try {
            m_shareableSslContext.load_verify_file("C:/hdtn_ssl_certificates/cert.pem");
            m_shareableSslContext.set_verify_mode(boost::asio::ssl::verify_peer);
        }
        catch (boost::system::system_error & e) {
            std::cout << "error in TcpclV4Outduct constructor: " << e.what() << std::endl;
            return;
        }
        m_shareableSslContext.set_verify_callback(
            boost::bind(&TcpclV4Outduct::VerifyCertificate, this, boost::placeholders::_1, boost::placeholders::_2));
    }
#endif
}
TcpclV4Outduct::~TcpclV4Outduct() {}

std::size_t TcpclV4Outduct::GetTotalDataSegmentsUnacked() {
    return m_tcpclV4BundleSource.Virtual_GetTotalBundlesUnacked();
}
bool TcpclV4Outduct::Forward(const uint8_t* bundleData, const std::size_t size) {
    return m_tcpclV4BundleSource.BaseClass_Forward(bundleData, size);
}
bool TcpclV4Outduct::Forward(zmq::message_t & movableDataZmq) {
    return m_tcpclV4BundleSource.BaseClass_Forward(movableDataZmq);
}
bool TcpclV4Outduct::Forward(std::vector<uint8_t> & movableDataVec) {
    return m_tcpclV4BundleSource.BaseClass_Forward(movableDataVec);
}

void TcpclV4Outduct::SetOnSuccessfulAckCallback(const OnSuccessfulOutductAckCallback_t & callback) {
    m_tcpclV4BundleSource.SetOnSuccessfulAckCallback(callback);
}

void TcpclV4Outduct::Connect() {
    m_tcpclV4BundleSource.Connect(m_outductConfig.remoteHostname, boost::lexical_cast<std::string>(m_outductConfig.remotePort));
}
bool TcpclV4Outduct::ReadyToForward() {
    return m_tcpclV4BundleSource.ReadyToForward();
}
void TcpclV4Outduct::Stop() {
    m_tcpclV4BundleSource.Stop();
}
void TcpclV4Outduct::GetOutductFinalStats(OutductFinalStats & finalStats) {
    finalStats.m_convergenceLayer = m_outductConfig.convergenceLayer;
    finalStats.m_totalDataSegmentsOrPacketsAcked = m_tcpclV4BundleSource.Virtual_GetTotalBundlesAcked();
    finalStats.m_totalDataSegmentsOrPacketsSent = m_tcpclV4BundleSource.Virtual_GetTotalBundlesSent();
}


#ifdef OPENSSL_SUPPORT_ENABLED
bool TcpclV4Outduct::VerifyCertificate(bool preverified, boost::asio::ssl::verify_context& ctx) {
    // The verify callback can be used to check whether the certificate that is
    // being presented is valid for the peer. For example, RFC 2818 describes
    // the steps involved in doing this for HTTPS. Consult the OpenSSL
    // documentation for more details. Note that the callback is called once
    // for each certificate in the certificate chain, starting from the root
    // certificate authority.

    // In this example we will simply print the certificate's subject name.
    char subject_name[256];
    X509* cert = X509_STORE_CTX_get_current_cert(ctx.native_handle());
    X509_NAME_oneline(X509_get_subject_name(cert), subject_name, 256);
    std::cout << "Verifying " << subject_name << "  preverified=" << preverified << "\n";

    //return preverified;
    return true;
}
#endif

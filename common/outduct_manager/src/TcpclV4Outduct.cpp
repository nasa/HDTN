#include "TcpclV4Outduct.h"
#include <iostream>
#include <boost/make_unique.hpp>
#include <boost/make_shared.hpp>
#include <boost/lexical_cast.hpp>
#include "Uri.h"

TcpclV4Outduct::TcpclV4Outduct(const outduct_element_config_t & outductConfig, const uint64_t myNodeId, const uint64_t outductUuid,
    const uint64_t maxOpportunisticRxBundleSizeBytes,
    const OutductOpportunisticProcessReceivedBundleCallback_t & outductOpportunisticProcessReceivedBundleCallback) :
    Outduct(outductConfig, outductUuid),

#ifdef OPENSSL_SUPPORT_ENABLED
    m_shareableSslContext((outductConfig.useTlsVersion1_3) ? boost::asio::ssl::context::tlsv13_client : boost::asio::ssl::context::tlsv12_client),
    m_tcpclV4BundleSource(m_shareableSslContext,
#else
    m_tcpclV4BundleSource(
#endif
        outductConfig.tryUseTls, outductConfig.tlsIsRequired,
        outductConfig.keepAliveIntervalSeconds, myNodeId, outductConfig.nextHopEndpointId,
        outductConfig.bundlePipelineLimit + 5, outductConfig.tcpclV4MyMaxRxSegmentSizeBytes, maxOpportunisticRxBundleSizeBytes, outductOpportunisticProcessReceivedBundleCallback)
{
#ifdef OPENSSL_SUPPORT_ENABLED
    if (outductConfig.tryUseTls) {
        try {
            m_shareableSslContext.load_verify_file(outductConfig.certificationAuthorityPemFileForVerification);//"C:/hdtn_ssl_certificates/cert.pem");
            m_shareableSslContext.set_verify_mode(boost::asio::ssl::verify_peer);
        }
        catch (boost::system::system_error & e) {
            std::cout << "error in TcpclV4Outduct constructor: " << e.what() << std::endl;
            return;
        }
        std::string nextHopEndpointIdWithServiceIdZero;
        uint64_t remoteNodeId, remoteServiceId;
        if (!Uri::ParseIpnUriString(outductConfig.nextHopEndpointId, remoteNodeId, remoteServiceId)) {
            std::cerr << "error in TcpclV4Outduct constructor: error parsing remote EID URI string " << outductConfig.nextHopEndpointId << std::endl;
            return;
        }
        else {
            //ion 3.7.2 source code tcpcli.c line 1199 uses service number 0 for contact header:
            //isprintf(eid, sizeof eid, "ipn:" UVAST_FIELDSPEC ".0", getOwnNodeNbr());
            nextHopEndpointIdWithServiceIdZero = Uri::GetIpnUriString(remoteNodeId, 0);
        }
       
        m_shareableSslContext.set_verify_callback(
            boost::bind(&TcpclV4Outduct::VerifyCertificate, this, boost::placeholders::_1, boost::placeholders::_2, nextHopEndpointIdWithServiceIdZero,
                outductConfig.verifySubjectAltNameInX509Certificate, outductConfig.doX509CertificateVerification));
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
static bool VerifySubjectAltNameFromCertificate(X509 *cert, const std::string & expectedIpnEidUri) {

    GENERAL_NAMES* altNames = (GENERAL_NAMES*)X509_get_ext_d2i(cert, NID_subject_alt_name, NULL, NULL);
    const int numAltNames = sk_GENERAL_NAME_num(altNames);

    for (int i = 0; i < numAltNames; ++i) {
        GENERAL_NAME *currentSubjectAltName = sk_GENERAL_NAME_value(altNames, i);

        if (currentSubjectAltName->type == GEN_URI) {
            unsigned char *outBuf = NULL;
            ASN1_STRING_to_UTF8(&outBuf, currentSubjectAltName->d.uniformResourceIdentifier);
            const std::string subjectAltNameString((const char *)outBuf, (std::size_t)ASN1_STRING_length(currentSubjectAltName->d.uniformResourceIdentifier));
            std::cout << "subjectAltNameString=" << subjectAltNameString << std::endl;
            OPENSSL_free(outBuf);
            if (subjectAltNameString == expectedIpnEidUri) {
                return true;
            }
        }
    }
    return false;
}

bool TcpclV4Outduct::VerifyCertificate(bool preverified, boost::asio::ssl::verify_context& ctx, const std::string & nextHopEndpointIdStrWithServiceIdZero, bool doVerifyNextHopEndpointIdStr, bool doX509CertificateVerification) {
    // The verify callback can be used to check whether the certificate that is
    // being presented is valid for the peer. For example, RFC 2818 describes
    // the steps involved in doing this for HTTPS. Consult the OpenSSL
    // documentation for more details. Note that the callback is called once
    // for each certificate in the certificate chain, starting from the root
    // certificate authority.

    // get the certificate's subject name.
    std::vector<char> subjectName(256);
    X509* cert = X509_STORE_CTX_get_current_cert(ctx.native_handle());
    X509_NAME_oneline(X509_get_subject_name(cert), &subjectName[0], (int)subjectName.size());

    //The TCPCL requires Version 3 certificates due to the extensions used
    //by this profile.  TCPCL entities SHALL reject as invalid Version 1
    //and Version 2 end-entity certificates.
   
    //X509_get_version() returns the numerical value of the version field of certificate x.
    //Note: this is defined by standards (X.509 et al) to be one less than the certificate version.
    //So a version 3 certificate will return 2 and a version 1 certificate will return 0.
    const long x509Version = X509_get_version(cert) + 1;
    if (doX509CertificateVerification) {
        std::cout << "Verifying " << subjectName.data() << "  preverified=" << preverified << " x509 version=" << x509Version << "\n";
    }
    else {
        std::cout << "Skipping verification and accepting this certificate: subject=" << subjectName.data() << "  preverified=" << preverified << " x509 version=" << x509Version << "\n";
        return true;
    }
    if (x509Version < 3) {
        std::cout << "error in TcpclV4Outduct::VerifyCertificate: tcpclV4 requires a minimum X.509 certificate of 3 but got " << x509Version << std::endl;
        return false;
    }
    if (!preverified) {
        std::cout << "error in TcpclV4Outduct::VerifyCertificate: X.509 certificate not verified\n";
        return false;
    }
    
    
    if (doVerifyNextHopEndpointIdStr) {
        if (!VerifySubjectAltNameFromCertificate(cert, nextHopEndpointIdStrWithServiceIdZero)) {
            std::cout << "error in TcpclV4Outduct::VerifyCertificate: the subjectAltName URI in the X.509 certificate does not match the next hop endpoint id of " << nextHopEndpointIdStrWithServiceIdZero << std::endl;
            return false;
        }
        else {
            std::cout << "success: X.509 certificate subjectAltName matches the nextHopEndpointIdStr\n";
        }
    }

    return true;
}


#endif

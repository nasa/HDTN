/**
 * @file TcpclV4BundleSource.h
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
 * This TcpclV4BundleSource class encapsulates the appropriate TCPCL version 4 functionality
 * to send a pipeline of bundles (or any other user defined data) over a TCPCL version 4 link (either encrypted or not)
 * and calls the user defined function OnSuccessfulAckCallback_t when the session closes, meaning
 * a bundle is fully sent (i.e. gets acknowledged by the remote receiver).
 */

#ifndef _TCPCLV4_BUNDLE_SOURCE_H
#define _TCPCLV4_BUNDLE_SOURCE_H 1

#include "TcpclV4BidirectionalLink.h"


//tcpcl
class CLASS_VISIBILITY_TCPCL_LIB TcpclV4BundleSource : public TcpclV4BidirectionalLink {
private:
    TcpclV4BundleSource();
public:
    TCPCL_LIB_EXPORT TcpclV4BundleSource(
#ifdef OPENSSL_SUPPORT_ENABLED
        boost::asio::ssl::context & shareableSslContextRef,
#endif
        const bool tryUseTls, const bool tlsIsRequired,
        const uint16_t desiredKeepAliveIntervalSeconds, const uint64_t myNodeId,
        const std::string & expectedRemoteEidUri, const unsigned int maxUnacked, const uint64_t myMaxRxSegmentSizeBytes, const uint64_t myMaxRxBundleSizeBytes,
        const OutductOpportunisticProcessReceivedBundleCallback_t & outductOpportunisticProcessReceivedBundleCallback = OutductOpportunisticProcessReceivedBundleCallback_t());

    TCPCL_LIB_EXPORT virtual ~TcpclV4BundleSource();
    TCPCL_LIB_EXPORT void Stop();
    
    TCPCL_LIB_EXPORT void Connect(const std::string & hostname, const std::string & port);
    TCPCL_LIB_EXPORT bool ReadyToForward() const;
private:
    TCPCL_LIB_NO_EXPORT void OnResolve(const boost::system::error_code & ec, boost::asio::ip::tcp::resolver::results_type results);
    TCPCL_LIB_NO_EXPORT void OnConnect(const boost::system::error_code & ec);
    TCPCL_LIB_NO_EXPORT void OnReconnectAfterOnConnectError_TimerExpired(const boost::system::error_code& e);
    TCPCL_LIB_NO_EXPORT void StartTcpReceiveUnsecure();
    TCPCL_LIB_NO_EXPORT void HandleTcpReceiveSomeUnsecure(const boost::system::error_code & error, std::size_t bytesTransferred);
#ifdef OPENSSL_SUPPORT_ENABLED
    TCPCL_LIB_NO_EXPORT void StartTcpReceiveSecure();
    TCPCL_LIB_NO_EXPORT void HandleTcpReceiveSomeSecure(const boost::system::error_code & error, std::size_t bytesTransferred);
    TCPCL_LIB_NO_EXPORT void HandleSslHandshake(const boost::system::error_code & error);
#endif
    TCPCL_LIB_NO_EXPORT void OnNeedToReconnectAfterShutdown_TimerExpired(const boost::system::error_code& e);

    TCPCL_LIB_NO_EXPORT virtual void Virtual_OnTcpclShutdownComplete_CalledFromIoServiceThread();
    TCPCL_LIB_NO_EXPORT virtual void Virtual_OnSuccessfulWholeBundleAcknowledged();
    TCPCL_LIB_NO_EXPORT virtual void Virtual_WholeBundleReady(padded_vector_uint8_t & wholeBundleVec);

    
    
#ifdef OPENSSL_SUPPORT_ENABLED
    boost::asio::ssl::context & m_shareableSslContextRef;
#endif
    boost::asio::io_service::work m_work;
    boost::asio::ip::tcp::resolver m_resolver;
    boost::asio::deadline_timer m_reconnectAfterShutdownTimer;
    boost::asio::deadline_timer m_reconnectAfterOnConnectErrorTimer;
    
    boost::asio::ip::tcp::resolver::results_type m_resolverResults;
    std::unique_ptr<boost::thread> m_ioServiceThreadPtr;
    

    //opportunistic receive bundles
    const OutductOpportunisticProcessReceivedBundleCallback_t m_outductOpportunisticProcessReceivedBundleCallback;


    std::vector<uint8_t> m_tcpReadSomeBufferVec;

};



#endif //_TCPCLV4_BUNDLE_SOURCE_H

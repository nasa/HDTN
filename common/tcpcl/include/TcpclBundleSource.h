/**
 * @file TcpclBundleSource.h
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
 * This TcpclBundleSource class encapsulates the appropriate TCPCL version 3 functionality
 * to send a pipeline of bundles (or any other user defined data) over a TCPCL version 3 link
 * and calls the user defined function OnSuccessfulAckCallback_t when the session closes, meaning
 * a bundle is fully sent (i.e. gets acknowledged by the remote receiver).
 */

#ifndef _TCPCL_BUNDLE_SOURCE_H
#define _TCPCL_BUNDLE_SOURCE_H 1

#include "TcpclV3BidirectionalLink.h"



//tcpcl
class CLASS_VISIBILITY_TCPCL_LIB TcpclBundleSource : public TcpclV3BidirectionalLink {
private:
    TcpclBundleSource();
public:
    typedef boost::function<void()> OnSuccessfulAckCallback_t;
    TCPCL_LIB_EXPORT TcpclBundleSource(const uint16_t desiredKeepAliveIntervalSeconds, const uint64_t myNodeId,
        const std::string & expectedRemoteEidUri, const unsigned int maxUnacked, const uint64_t maxFragmentSize,
        const OutductOpportunisticProcessReceivedBundleCallback_t & outductOpportunisticProcessReceivedBundleCallback = OutductOpportunisticProcessReceivedBundleCallback_t());

    TCPCL_LIB_EXPORT virtual ~TcpclBundleSource();
    TCPCL_LIB_EXPORT void Stop();
    
    
    TCPCL_LIB_EXPORT void Connect(const std::string & hostname, const std::string & port);
    TCPCL_LIB_EXPORT bool ReadyToForward() const;
    TCPCL_LIB_EXPORT void SetOnSuccessfulAckCallback(const OnSuccessfulAckCallback_t & callback);
private:
    TCPCL_LIB_NO_EXPORT void OnResolve(const boost::system::error_code & ec, boost::asio::ip::tcp::resolver::results_type results);
    TCPCL_LIB_NO_EXPORT void OnConnect(const boost::system::error_code & ec);
    TCPCL_LIB_NO_EXPORT void OnReconnectAfterOnConnectError_TimerExpired(const boost::system::error_code& e);
    TCPCL_LIB_NO_EXPORT void StartTcpReceive();
    TCPCL_LIB_NO_EXPORT void HandleTcpReceiveSome(const boost::system::error_code & error, std::size_t bytesTransferred);
    TCPCL_LIB_NO_EXPORT void OnNeedToReconnectAfterShutdown_TimerExpired(const boost::system::error_code& e);

    TCPCL_LIB_NO_EXPORT virtual void Virtual_OnTcpclShutdownComplete_CalledFromIoServiceThread();
    TCPCL_LIB_NO_EXPORT virtual void Virtual_OnSuccessfulWholeBundleAcknowledged();
    TCPCL_LIB_NO_EXPORT virtual void Virtual_WholeBundleReady(padded_vector_uint8_t & wholeBundleVec);
    
    
    
    boost::asio::io_service::work m_work;
    boost::asio::ip::tcp::resolver m_resolver;
    boost::asio::deadline_timer m_reconnectAfterShutdownTimer;
    boost::asio::deadline_timer m_reconnectAfterOnConnectErrorTimer;
    
    boost::asio::ip::tcp::resolver::results_type m_resolverResults;
    std::unique_ptr<boost::thread> m_ioServiceThreadPtr;
    

    //tcpcl vars

    
    
    
    
    
    
    
    
    
    
    OnSuccessfulAckCallback_t m_onSuccessfulAckCallback;

    //opportunistic receive bundles
    const OutductOpportunisticProcessReceivedBundleCallback_t m_outductOpportunisticProcessReceivedBundleCallback;


    std::vector<uint8_t> m_tcpReadSomeBufferVec;

};



#endif //_TCPCL_BUNDLE_SOURCE_H

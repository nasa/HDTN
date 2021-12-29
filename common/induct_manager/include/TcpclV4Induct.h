#ifndef TCPCLV4_INDUCT_H
#define TCPCLV4_INDUCT_H 1

#include <string>
#include "Induct.h"
#include "TcpclV4BundleSink.h"
#include <list>
#include <boost/make_unique.hpp>

class TcpclV4Induct : public Induct {
public:
    TcpclV4Induct(const InductProcessBundleCallback_t & inductProcessBundleCallback, const induct_element_config_t & inductConfig,
        const uint64_t myNodeId, const uint64_t maxBundleSizeBytes, const OnNewOpportunisticLinkCallback_t & onNewOpportunisticLinkCallback,
        const OnDeletedOpportunisticLinkCallback_t & onDeletedOpportunisticLinkCallback);
    virtual ~TcpclV4Induct();
private:
    


    TcpclV4Induct();
    void StartTcpAccept();
#ifdef OPENSSL_SUPPORT_ENABLED
    boost::asio::ssl::context m_shareableSslContext;
    void HandleTcpAccept(boost::shared_ptr<boost::asio::ssl::stream<boost::asio::ip::tcp::socket> > & newSslStreamSharedPtr, const boost::system::error_code & error);
#else
    void HandleTcpAccept(boost::shared_ptr<boost::asio::ip::tcp::socket> & newTcpSocketPtr, const boost::system::error_code & error);
#endif
    void ConnectionReadyToBeDeletedNotificationReceived();
    void RemoveInactiveTcpConnections();
    void DisableRemoveInactiveTcpConnections();
    void OnContactHeaderCallback_FromIoServiceThread(TcpclV4BundleSink * thisTcpclBundleSinkPtr);
    void NotifyBundleReadyToSend_FromIoServiceThread(const uint64_t remoteNodeId);
    virtual void Virtual_PostNotifyBundleReadyToSend_FromIoServiceThread(const uint64_t remoteNodeId);


    boost::asio::io_service m_ioService;
    boost::asio::ip::tcp::acceptor m_tcpAcceptor;
    std::unique_ptr<boost::asio::io_service::work> m_workPtr;
    std::unique_ptr<boost::thread> m_ioServiceThreadPtr;
    std::list<TcpclV4BundleSink> m_listTcpclV4BundleSinks;
    const uint64_t M_MY_NODE_ID;
    volatile bool m_allowRemoveInactiveTcpConnections;
    const uint64_t M_MAX_BUNDLE_SIZE_BYTES;

};


#endif // TCPCLV4_INDUCT_H


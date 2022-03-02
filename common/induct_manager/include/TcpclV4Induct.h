#ifndef TCPCLV4_INDUCT_H
#define TCPCLV4_INDUCT_H 1

#include <string>
#include "Induct.h"
#include "TcpclV4BundleSink.h"
#include <list>
#include <boost/make_unique.hpp>

class CLASS_VISIBILITY_INDUCT_MANAGER_LIB TcpclV4Induct : public Induct {
public:
    INDUCT_MANAGER_LIB_EXPORT TcpclV4Induct(const InductProcessBundleCallback_t & inductProcessBundleCallback, const induct_element_config_t & inductConfig,
        const uint64_t myNodeId, const uint64_t maxBundleSizeBytes, const OnNewOpportunisticLinkCallback_t & onNewOpportunisticLinkCallback,
        const OnDeletedOpportunisticLinkCallback_t & onDeletedOpportunisticLinkCallback);
    INDUCT_MANAGER_LIB_EXPORT virtual ~TcpclV4Induct();
private:
    


    TcpclV4Induct();
    INDUCT_MANAGER_LIB_EXPORT void StartTcpAccept();
#ifdef OPENSSL_SUPPORT_ENABLED
    boost::asio::ssl::context m_shareableSslContext;
    INDUCT_MANAGER_LIB_EXPORT void HandleTcpAccept(boost::shared_ptr<boost::asio::ssl::stream<boost::asio::ip::tcp::socket> > & newSslStreamSharedPtr, const boost::system::error_code & error);
#else
    INDUCT_MANAGER_LIB_EXPORT void HandleTcpAccept(boost::shared_ptr<boost::asio::ip::tcp::socket> & newTcpSocketPtr, const boost::system::error_code & error);
#endif
    INDUCT_MANAGER_LIB_EXPORT void ConnectionReadyToBeDeletedNotificationReceived();
    INDUCT_MANAGER_LIB_EXPORT void RemoveInactiveTcpConnections();
    INDUCT_MANAGER_LIB_EXPORT void DisableRemoveInactiveTcpConnections();
    INDUCT_MANAGER_LIB_EXPORT void OnContactHeaderCallback_FromIoServiceThread(TcpclV4BundleSink * thisTcpclBundleSinkPtr);
    INDUCT_MANAGER_LIB_EXPORT void NotifyBundleReadyToSend_FromIoServiceThread(const uint64_t remoteNodeId);
    INDUCT_MANAGER_LIB_EXPORT virtual void Virtual_PostNotifyBundleReadyToSend_FromIoServiceThread(const uint64_t remoteNodeId);


    boost::asio::io_service m_ioService;
    boost::asio::ip::tcp::acceptor m_tcpAcceptor;
    std::unique_ptr<boost::asio::io_service::work> m_workPtr;
    std::unique_ptr<boost::thread> m_ioServiceThreadPtr;
    std::list<TcpclV4BundleSink> m_listTcpclV4BundleSinks;
    const uint64_t M_MY_NODE_ID;
    volatile bool m_allowRemoveInactiveTcpConnections;
    const uint64_t M_MAX_BUNDLE_SIZE_BYTES;
    bool m_tlsSuccessfullyConfigured;
};


#endif // TCPCLV4_INDUCT_H


#ifndef TCPCL_INDUCT_H
#define TCPCL_INDUCT_H 1

#include <string>
#include "Induct.h"
#include "TcpclBundleSink.h"
#include <list>
#include <boost/make_unique.hpp>

class TcpclInduct : public Induct {
public:
    TcpclInduct(const InductProcessBundleCallback_t & inductProcessBundleCallback, const induct_element_config_t & inductConfig,
        const uint64_t myNodeId, const uint64_t maxBundleSizeBytes, const OnNewOpportunisticLinkCallback_t & onNewOpportunisticLinkCallback,
        const OnDeletedOpportunisticLinkCallback_t & onDeletedOpportunisticLinkCallback);
    virtual ~TcpclInduct();
    
private:
    

    TcpclInduct();
    void StartTcpAccept();
    void HandleTcpAccept(boost::shared_ptr<boost::asio::ip::tcp::socket> & newTcpSocketPtr, const boost::system::error_code& error);
    void ConnectionReadyToBeDeletedNotificationReceived();
    void RemoveInactiveTcpConnections();
    void DisableRemoveInactiveTcpConnections();
    void OnContactHeaderCallback_FromIoServiceThread(TcpclBundleSink * thisTcpclBundleSinkPtr);
    void NotifyBundleReadyToSend_FromIoServiceThread(const uint64_t remoteNodeId);
    virtual void Virtual_PostNotifyBundleReadyToSend_FromIoServiceThread(const uint64_t remoteNodeId);

    boost::asio::io_service m_ioService;
    boost::asio::ip::tcp::acceptor m_tcpAcceptor;
    std::unique_ptr<boost::asio::io_service::work> m_workPtr;
    std::unique_ptr<boost::thread> m_ioServiceThreadPtr;
    std::list<TcpclBundleSink> m_listTcpclBundleSinks;
    const uint64_t M_MY_NODE_ID;
    volatile bool m_allowRemoveInactiveTcpConnections;
    const uint64_t M_MAX_BUNDLE_SIZE_BYTES;

    

    
};


#endif // TCPCL_INDUCT_H


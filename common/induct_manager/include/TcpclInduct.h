#ifndef TCPCL_INDUCT_H
#define TCPCL_INDUCT_H 1

#include <string>
#include "Induct.h"
#include "TcpclBundleSink.h"
#include <list>

class TcpclInduct : public Induct {
public:
    TcpclInduct(const InductProcessBundleCallback_t & inductProcessBundleCallback, const induct_element_config_t & inductConfig, const uint64_t myNodeId);
    virtual ~TcpclInduct();
    
private:
    TcpclInduct();
    void StartTcpAccept();
    void HandleTcpAccept(boost::shared_ptr<boost::asio::ip::tcp::socket> & newTcpSocketPtr, const boost::system::error_code& error);
    void ConnectionReadyToBeDeletedNotificationReceived();
    void RemoveInactiveTcpConnections();

    boost::asio::io_service m_ioService;
    boost::asio::ip::tcp::acceptor m_tcpAcceptor;
    std::unique_ptr<boost::thread> m_ioServiceThreadPtr;
    std::list<TcpclBundleSink> m_listTcpclBundleSinks;
    const uint64_t M_MY_NODE_ID;
};


#endif // TCPCL_INDUCT_H


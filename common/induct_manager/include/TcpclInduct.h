#ifndef TCPCL_INDUCT_H
#define TCPCL_INDUCT_H 1

#include <string>
#include "Induct.h"
#include "TcpclBundleSink.h"
#include <list>

class TcpclInduct : public Induct {
public:
    TcpclInduct(const InductProcessBundleCallback_t & inductProcessBundleCallback, const induct_element_config_t & inductConfig);
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
};


#endif // TCPCL_INDUCT_H


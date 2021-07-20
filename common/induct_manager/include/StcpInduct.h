#ifndef STCP_INDUCT_H
#define STCP_INDUCT_H 1

#include <string>
#include "Induct.h"
#include "StcpBundleSink.h"
#include <list>

class StcpInduct : public Induct {
public:
    StcpInduct(const InductProcessBundleCallback_t & inductProcessBundleCallback, const induct_element_config_t & inductConfig);
    virtual ~StcpInduct();
    
private:
    StcpInduct();
    void StartTcpAccept();
    void HandleTcpAccept(boost::shared_ptr<boost::asio::ip::tcp::socket> & newTcpSocketPtr, const boost::system::error_code& error);
    void ConnectionReadyToBeDeletedNotificationReceived();
    void RemoveInactiveTcpConnections();

    boost::asio::io_service m_ioService;
    boost::asio::ip::tcp::acceptor m_tcpAcceptor;
    std::unique_ptr<boost::thread> m_ioServiceThreadPtr;
    std::list<StcpBundleSink> m_listStcpBundleSinks;
};


#endif // STCP_INDUCT_H


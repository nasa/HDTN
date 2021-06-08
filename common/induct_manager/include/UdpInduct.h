#ifndef UDP_INDUCT_H
#define UDP_INDUCT_H 1

#include <string>
#include "Induct.h"
#include "UdpBundleSink.h"

class UdpInduct : public Induct {
public:
    UdpInduct(const InductProcessBundleCallback_t & inductProcessBundleCallback, const induct_element_config_t & inductConfig);
    virtual ~UdpInduct();
    
private:
    UdpInduct();
    void ConnectionReadyToBeDeletedNotificationReceived();
    void RemoveInactiveConnection();

    boost::asio::io_service m_ioService;
    std::unique_ptr<boost::thread> m_ioServiceThreadPtr;
    std::unique_ptr<UdpBundleSink> m_udpBundleSinkPtr;
};


#endif // UDP_INDUCT_H


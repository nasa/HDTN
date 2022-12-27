#ifndef UDP_INDUCT_H
#define UDP_INDUCT_H 1

#include <string>
#include "Induct.h"
#include "UdpBundleSink.h"

class CLASS_VISIBILITY_INDUCT_MANAGER_LIB UdpInduct : public Induct {
public:
    INDUCT_MANAGER_LIB_EXPORT UdpInduct(const InductProcessBundleCallback_t & inductProcessBundleCallback, const induct_element_config_t & inductConfig);
    INDUCT_MANAGER_LIB_EXPORT virtual ~UdpInduct() override;
    
private:
    UdpInduct();
    INDUCT_MANAGER_LIB_EXPORT void ConnectionReadyToBeDeletedNotificationReceived();
    INDUCT_MANAGER_LIB_EXPORT void RemoveInactiveConnection();

    boost::asio::io_service m_ioService;
    std::unique_ptr<boost::thread> m_ioServiceThreadPtr;
    std::unique_ptr<UdpBundleSink> m_udpBundleSinkPtr;
};


#endif // UDP_INDUCT_H


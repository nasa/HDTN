/**
 * @file UdpInduct.h
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
 * The UdpInduct class contains the functionality for a UDP induct
 * used by the InductManager.  This class is the interface to udp_lib.
 */

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


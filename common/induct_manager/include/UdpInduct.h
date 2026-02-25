/**
 * @file UdpInduct.h
 * @author  Brian Tomko <brian.j.tomko@nasa.gov>
 *
 * @copyright Copyright (c) 2026 United States Government as represented by
 * the National Aeronautics and Space Administration.
 * No copyright is claimed in the United States under Title 17, U.S.Code.
 * All Other Rights Reserved.
 *
 * @section LICENSE
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
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
    INDUCT_MANAGER_LIB_EXPORT virtual void PopulateInductTelemetry(InductTelemetry_t& inductTelem) override;
private:
    UdpInduct();
    INDUCT_MANAGER_LIB_EXPORT void ConnectionReadyToBeDeletedNotificationReceived();
    INDUCT_MANAGER_LIB_EXPORT void RemoveInactiveConnection();

    boost::asio::io_service m_ioService;
    std::unique_ptr<boost::thread> m_ioServiceThreadPtr;
    std::unique_ptr<UdpBundleSink> m_udpBundleSinkPtr;
};


#endif // UDP_INDUCT_H


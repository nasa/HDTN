/**
 * @file StcpInduct.h
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
 * The StcpInduct class contains the functionality for an STCP induct
 * used by the InductManager.  This class is the interface to stcp_lib.
 */

#ifndef STCP_INDUCT_H
#define STCP_INDUCT_H 1

#include <string>
#include "Induct.h"
#include "StcpBundleSink.h"
#include <list>
#include <memory>
#include <atomic>

class CLASS_VISIBILITY_INDUCT_MANAGER_LIB StcpInduct : public Induct {
public:
    INDUCT_MANAGER_LIB_EXPORT StcpInduct(const InductProcessBundleCallback_t & inductProcessBundleCallback,
        const induct_element_config_t & inductConfig, const uint64_t maxBundleSizeBytes,
        const OnNewOpportunisticLinkCallback_t& onNewOpportunisticLinkCallback, //for telemetry (so know when a new connection is made)
        const OnDeletedOpportunisticLinkCallback_t& onDeletedOpportunisticLinkCallback);
    INDUCT_MANAGER_LIB_EXPORT virtual ~StcpInduct() override;
    INDUCT_MANAGER_LIB_EXPORT virtual void PopulateInductTelemetry(InductTelemetry_t& inductTelem) override;
    
private:
    INDUCT_MANAGER_LIB_EXPORT StcpInduct();
    INDUCT_MANAGER_LIB_EXPORT void StartTcpAccept();
    INDUCT_MANAGER_LIB_EXPORT void HandleTcpAccept(std::shared_ptr<boost::asio::ip::tcp::socket> & newTcpSocketPtr, const boost::system::error_code& error);
    INDUCT_MANAGER_LIB_EXPORT void ConnectionReadyToBeDeletedNotificationReceived();
    INDUCT_MANAGER_LIB_EXPORT void RemoveInactiveTcpConnections();
    INDUCT_MANAGER_LIB_EXPORT void DisableRemoveInactiveTcpConnections();

    boost::asio::io_service m_ioService;
    boost::asio::ip::tcp::acceptor m_tcpAcceptor;
    std::unique_ptr<boost::asio::io_service::work> m_workPtr;
    std::unique_ptr<boost::thread> m_ioServiceThreadPtr;
    std::list<StcpBundleSink> m_listStcpBundleSinks;
    boost::mutex m_listStcpBundleSinksMutex;
    std::atomic<bool> m_allowRemoveInactiveTcpConnections;
    const uint64_t M_MAX_BUNDLE_SIZE_BYTES;
};


#endif // STCP_INDUCT_H


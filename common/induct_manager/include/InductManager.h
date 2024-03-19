/**
 * @file InductManager.h
 * @author  Brian Tomko <brian.j.tomko@nasa.gov>
 *
 * @copyright Copyright (c) 2021 United States Government as represented by
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
 * The InductManager class contains the collection of inducts loaded from a
 * configuration file for the running instance of HDTN.
 */

#ifndef INDUCT_MANAGER_H
#define INDUCT_MANAGER_H 1

#include "Induct.h"
#include <list>

class InductManager {
public:
    INDUCT_MANAGER_LIB_EXPORT InductManager();
    INDUCT_MANAGER_LIB_EXPORT ~InductManager();
    INDUCT_MANAGER_LIB_EXPORT bool LoadInductsFromConfig(const InductProcessBundleCallback_t & inductProcessBundleCallback, const InductsConfig & inductsConfig,
        const uint64_t myNodeId, const uint64_t maxUdpRxPacketSizeBytesForAllLtp, const uint64_t maxBundleSizeBytes,
        const OnNewOpportunisticLinkCallback_t & onNewOpportunisticLinkCallback, const OnDeletedOpportunisticLinkCallback_t & onDeletedOpportunisticLinkCallback);
    INDUCT_MANAGER_LIB_EXPORT void Clear();
    INDUCT_MANAGER_LIB_EXPORT void PopulateAllInductTelemetry(AllInductTelemetry_t& allInductTelem);
public:

    std::list<std::unique_ptr<Induct> > m_inductsList;
};

#endif // INDUCT_MANAGER_H


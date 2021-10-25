#ifndef INDUCT_MANAGER_H
#define INDUCT_MANAGER_H 1

#include "Induct.h"
#include <list>

class InductManager {
public:
    InductManager();
    ~InductManager();
    void LoadInductsFromConfig(const InductProcessBundleCallback_t & inductProcessBundleCallback, const InductsConfig & inductsConfig,
        const uint64_t myNodeId, const uint64_t maxUdpRxPacketSizeBytesForAllLtp, const uint64_t maxBundleSizeBytes,
        const OnNewOpportunisticLinkCallback_t & onNewOpportunisticLinkCallback, const OnDeletedOpportunisticLinkCallback_t & onDeletedOpportunisticLinkCallback);
    void Clear();
public:

    std::list<std::unique_ptr<Induct> > m_inductsList;
};

#endif // INDUCT_MANAGER_H


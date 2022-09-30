#ifndef OUTDUCT_MANAGER_H
#define OUTDUCT_MANAGER_H 1

#include "Outduct.h"
#include <map>
#include <memory>
#include <vector>
#include <zmq.hpp>
#include <boost/thread.hpp>
#include "codec/bpv6.h"
#include "TcpclBundleSource.h" //for OutductOpportunisticProcessReceivedBundleCallback_t
#include "BundleCallbackFunctionDefines.h"

class OutductManager {
public:
    OUTDUCT_MANAGER_LIB_EXPORT OutductManager();
    OUTDUCT_MANAGER_LIB_EXPORT ~OutductManager();
    OUTDUCT_MANAGER_LIB_EXPORT bool LoadOutductsFromConfig(const OutductsConfig & outductsConfig, const uint64_t myNodeId, const uint64_t maxUdpRxPacketSizeBytesForAllLtp, const uint64_t maxOpportunisticRxBundleSizeBytes,
        const OutductOpportunisticProcessReceivedBundleCallback_t & outductOpportunisticProcessReceivedBundleCallback = OutductOpportunisticProcessReceivedBundleCallback_t(),
        const OnFailedBundleVecSendCallback_t & outductOnFailedBundleVecSendCallback = OnFailedBundleVecSendCallback_t(),
        const OnFailedBundleZmqSendCallback_t & outductOnFailedBundleZmqSendCallback = OnFailedBundleZmqSendCallback_t(),
        const OnSuccessfulBundleSendCallback_t& onSuccessfulBundleSendCallback = OnSuccessfulBundleSendCallback_t(),
        const OnOutductLinkStatusChangedCallback_t& onOutductLinkStatusChangedCallback = OnOutductLinkStatusChangedCallback_t());
    OUTDUCT_MANAGER_LIB_EXPORT bool AllReadyToForward() const;
    OUTDUCT_MANAGER_LIB_EXPORT void StopAllOutducts();
    OUTDUCT_MANAGER_LIB_EXPORT Outduct * GetOutductByFinalDestinationEid_ThreadSafe(const cbhe_eid_t & finalDestEid);
    OUTDUCT_MANAGER_LIB_EXPORT Outduct * GetOutductByOutductUuid(const uint64_t uuid);
    OUTDUCT_MANAGER_LIB_EXPORT bool Reroute_ThreadSafe(const uint64_t finalDestNodeId, const uint64_t newNextHopNodeId);
    OUTDUCT_MANAGER_LIB_EXPORT std::shared_ptr<Outduct> GetOutductSharedPtrByOutductUuid(const uint64_t uuid);
    OUTDUCT_MANAGER_LIB_EXPORT Outduct * GetOutductByNextHopNodeId(const uint64_t nextHopNodeId);

    OUTDUCT_MANAGER_LIB_EXPORT bool Forward(const cbhe_eid_t & finalDestEid, const uint8_t* bundleData, const std::size_t size, std::vector<uint8_t>&& userData);
    OUTDUCT_MANAGER_LIB_EXPORT bool Forward(const cbhe_eid_t & finalDestEid, zmq::message_t & movableDataZmq, std::vector<uint8_t>&& userData);
    OUTDUCT_MANAGER_LIB_EXPORT bool Forward(const cbhe_eid_t & finalDestEid, std::vector<uint8_t> & movableDataVec, std::vector<uint8_t>&& userData);

    OUTDUCT_MANAGER_LIB_EXPORT uint64_t GetAllOutductTelemetry(uint8_t* serialization, uint64_t bufferSize) const;
private:

    std::map<cbhe_eid_t, std::shared_ptr<Outduct> > m_finalDestEidToOutductMap;
    std::map<uint64_t, std::shared_ptr<Outduct> > m_finalDestNodeIdToOutductMap;
    boost::mutex m_finalDestEidToOutductMapMutex;
    boost::mutex m_finalDestNodeIdToOutductMapMutex;
    std::map<uint64_t, std::shared_ptr<Outduct> > m_nextHopNodeIdToOutductMap;
    std::vector<std::shared_ptr<Outduct> > m_outductsVec;
    uint64_t m_numEventsTooManyUnackedBundles;
};

#endif // OUTDUCT_MANAGER_H


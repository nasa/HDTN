#ifndef OUTDUCT_MANAGER_H
#define OUTDUCT_MANAGER_H 1

#include "Outduct.h"
#include <map>
#include <boost/shared_ptr.hpp>
#include <vector>
#include <zmq.hpp>
#include <boost/thread.hpp>
#include "codec/bpv6.h"
#include "TcpclBundleSource.h" //for OutductOpportunisticProcessReceivedBundleCallback_t

class OutductManager {
public:
    typedef boost::function<void(uint64_t outductUuidIndex)> OutductManager_OnSuccessfulOutductAckCallback_t;

    OUTDUCT_MANAGER_LIB_EXPORT OutductManager();
    OUTDUCT_MANAGER_LIB_EXPORT ~OutductManager();
    OUTDUCT_MANAGER_LIB_EXPORT bool LoadOutductsFromConfig(const OutductsConfig & outductsConfig, const uint64_t myNodeId, const uint64_t maxUdpRxPacketSizeBytesForAllLtp, const uint64_t maxOpportunisticRxBundleSizeBytes,
        const OutductOpportunisticProcessReceivedBundleCallback_t & outductOpportunisticProcessReceivedBundleCallback = OutductOpportunisticProcessReceivedBundleCallback_t());
    OUTDUCT_MANAGER_LIB_EXPORT void Clear();
    OUTDUCT_MANAGER_LIB_EXPORT bool AllReadyToForward() const;
    OUTDUCT_MANAGER_LIB_EXPORT void StopAllOutducts();
    OUTDUCT_MANAGER_LIB_EXPORT Outduct * GetOutductByFinalDestinationEid_ThreadSafe(const cbhe_eid_t & finalDestEid);
    OUTDUCT_MANAGER_LIB_EXPORT Outduct * GetOutductByOutductUuid(const uint64_t uuid);
    OUTDUCT_MANAGER_LIB_EXPORT void SetOutductForFinalDestinationEid_ThreadSafe(const cbhe_eid_t finalDestEid, boost::shared_ptr<Outduct> & outductPtr);
    OUTDUCT_MANAGER_LIB_EXPORT boost::shared_ptr<Outduct> GetOutductSharedPtrByOutductUuid(const uint64_t uuid);
    OUTDUCT_MANAGER_LIB_EXPORT Outduct * GetOutductByNextHopEid(const cbhe_eid_t & nextHopEid);
    OUTDUCT_MANAGER_LIB_EXPORT void SetOutductManagerOnSuccessfulOutductAckCallback(const OutductManager_OnSuccessfulOutductAckCallback_t & callback);

    OUTDUCT_MANAGER_LIB_EXPORT bool Forward(const cbhe_eid_t & finalDestEid, const uint8_t* bundleData, const std::size_t size);
    OUTDUCT_MANAGER_LIB_EXPORT bool Forward(const cbhe_eid_t & finalDestEid, zmq::message_t & movableDataZmq);
    OUTDUCT_MANAGER_LIB_EXPORT bool Forward(const cbhe_eid_t & finalDestEid, std::vector<uint8_t> & movableDataVec);

    OUTDUCT_MANAGER_LIB_EXPORT bool Forward_Blocking(const cbhe_eid_t & finalDestEid, const uint8_t* bundleData, const std::size_t size, const uint32_t timeoutSeconds);
    OUTDUCT_MANAGER_LIB_EXPORT bool Forward_Blocking(const cbhe_eid_t & finalDestEid, zmq::message_t & movableDataZmq, const uint32_t timeoutSeconds);
    OUTDUCT_MANAGER_LIB_EXPORT bool Forward_Blocking(const cbhe_eid_t & finalDestEid, std::vector<uint8_t> & movableDataVec, const uint32_t timeoutSeconds);
private:
    OUTDUCT_MANAGER_LIB_NO_EXPORT void OnSuccessfulBundleAck(uint64_t uuidIndex);

    struct thread_communication_t {
        boost::condition_variable m_cv;
        boost::mutex m_mutex;
        boost::mutex::scoped_lock m_lock;

        thread_communication_t() : m_lock(m_mutex) {}
        void Notify() {
            m_cv.notify_one();
        }
        void Wait250msOrUntilNotified() {
            m_cv.timed_wait(m_lock, boost::posix_time::milliseconds(250));
        }
    };

    std::map<cbhe_eid_t, boost::shared_ptr<Outduct> > m_finalDestEidToOutductMap;
    boost::mutex m_finalDestEidToOutductMapMutex;
    std::map<cbhe_eid_t, boost::shared_ptr<Outduct> > m_nextHopEidToOutductMap;
    std::vector<boost::shared_ptr<Outduct> > m_outductsVec;
    std::vector<std::unique_ptr<thread_communication_t> > m_threadCommunicationVec;
    uint64_t m_numEventsTooManyUnackedBundles;
    OutductManager_OnSuccessfulOutductAckCallback_t m_outductManager_onSuccessfulOutductAckCallback;
};

#endif // OUTDUCT_MANAGER_H


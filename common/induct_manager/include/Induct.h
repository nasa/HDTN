#ifndef INDUCT_H
#define INDUCT_H 1

#include <string>
#include <boost/integer.hpp>
#include <boost/function.hpp>
#include "InductsConfig.h"
#include <list>
#include <boost/thread.hpp>
#include <queue>
#include <zmq.hpp>
#include "BidirectionalLink.h"
#include "PaddedVectorUint8.h"
#include "induct_manager_lib_export.h"

class Induct;
typedef boost::function<void(padded_vector_uint8_t & movableBundle)> InductProcessBundleCallback_t;
typedef boost::function<void(const uint64_t remoteNodeId, Induct* thisInductPtr)> OnNewOpportunisticLinkCallback_t;
typedef boost::function<void(const uint64_t remoteNodeId)> OnDeletedOpportunisticLinkCallback_t;

class Induct {
private:
    Induct();
public:
    INDUCT_MANAGER_LIB_EXPORT Induct(const InductProcessBundleCallback_t & inductProcessBundleCallback, const induct_element_config_t & inductConfig);
    INDUCT_MANAGER_LIB_EXPORT virtual ~Induct();

    //tcpcl only
    INDUCT_MANAGER_LIB_EXPORT bool ForwardOnOpportunisticLink(const uint64_t remoteNodeId, std::vector<uint8_t> & dataVec, const uint32_t timeoutSeconds);
    INDUCT_MANAGER_LIB_EXPORT bool ForwardOnOpportunisticLink(const uint64_t remoteNodeId, zmq::message_t & dataZmq, const uint32_t timeoutSeconds);
    INDUCT_MANAGER_LIB_EXPORT bool ForwardOnOpportunisticLink(const uint64_t remoteNodeId, const uint8_t* bundleData, const std::size_t size, const uint32_t timeoutSeconds);

protected:
    struct OpportunisticBundleQueue { //tcpcl only
        INDUCT_MANAGER_LIB_EXPORT OpportunisticBundleQueue();
        INDUCT_MANAGER_LIB_EXPORT ~OpportunisticBundleQueue();
        INDUCT_MANAGER_LIB_EXPORT std::size_t GetQueueSize();
        INDUCT_MANAGER_LIB_EXPORT void PushMove_ThreadSafe(zmq::message_t & msg);
        INDUCT_MANAGER_LIB_EXPORT void PushMove_ThreadSafe(std::vector<uint8_t> & msg);
        INDUCT_MANAGER_LIB_EXPORT void PushMove_ThreadSafe(std::pair<std::unique_ptr<zmq::message_t>, std::vector<uint8_t> > & msgPair);
        INDUCT_MANAGER_LIB_EXPORT bool TryPop_ThreadSafe(std::pair<std::unique_ptr<zmq::message_t>, std::vector<uint8_t> > & msgPair);
        INDUCT_MANAGER_LIB_EXPORT void WaitUntilNotifiedOr250MsTimeout();
        INDUCT_MANAGER_LIB_EXPORT void NotifyAll();

        boost::mutex m_mutex;
        boost::condition_variable m_conditionVariable;
        std::queue<std::pair<std::unique_ptr<zmq::message_t>, std::vector<uint8_t> > > m_dataToSendQueue;
        //BidirectionalLink * m_bidirectionalLinkPtr;
        uint64_t m_remoteNodeId;
        unsigned int m_maxTxBundlesInPipeline;
    };
    //tcpcl only
    INDUCT_MANAGER_LIB_EXPORT bool BundleSinkTryGetData_FromIoServiceThread(OpportunisticBundleQueue & opportunisticBundleQueue, std::pair<std::unique_ptr<zmq::message_t>, std::vector<uint8_t> > & bundleDataPair);
    INDUCT_MANAGER_LIB_EXPORT void BundleSinkNotifyOpportunisticDataAcked_FromIoServiceThread(OpportunisticBundleQueue & opportunisticBundleQueue);
    INDUCT_MANAGER_LIB_EXPORT bool ForwardOnOpportunisticLink(const uint64_t remoteNodeId, zmq::message_t * zmqMsgPtr, std::vector<uint8_t> * vec8Ptr, const uint32_t timeoutSeconds);
    INDUCT_MANAGER_LIB_EXPORT virtual void Virtual_PostNotifyBundleReadyToSend_FromIoServiceThread(const uint64_t remoteNodeId);

    const InductProcessBundleCallback_t m_inductProcessBundleCallback;
    const induct_element_config_t m_inductConfig;

    //tcpcl only
    std::map<uint64_t, OpportunisticBundleQueue> m_mapNodeIdToOpportunisticBundleQueue;
    boost::mutex m_mapNodeIdToOpportunisticBundleQueueMutex;
    OnNewOpportunisticLinkCallback_t m_onNewOpportunisticLinkCallback;
    OnDeletedOpportunisticLinkCallback_t m_onDeletedOpportunisticLinkCallback;
};

#endif // INDUCT_H


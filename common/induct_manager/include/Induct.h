/**
 * @file Induct.h
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
 * The Induct class is the base class for all HDTN inducts
 * which are used by the InductManager.
 */

#ifndef INDUCT_H
#define INDUCT_H 1
#include "induct_manager_lib_export.h"
#ifndef CLASS_VISIBILITY_INDUCT_MANAGER_LIB
#  ifdef _WIN32
#    define CLASS_VISIBILITY_INDUCT_MANAGER_LIB
#  else
#    define CLASS_VISIBILITY_INDUCT_MANAGER_LIB INDUCT_MANAGER_LIB_EXPORT
#  endif
#endif

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
#include "TelemetryDefinitions.h"


class Induct;
typedef boost::function<void(padded_vector_uint8_t & movableBundle)> InductProcessBundleCallback_t;
typedef boost::function<void(const uint64_t remoteNodeId, Induct* thisInductPtr, void* sinkPtr)> OnNewOpportunisticLinkCallback_t;
typedef boost::function<void(const uint64_t remoteNodeId, Induct* thisInductPtr, void* sinkPtrAboutToBeDeleted)> OnDeletedOpportunisticLinkCallback_t;

class CLASS_VISIBILITY_INDUCT_MANAGER_LIB Induct {
private:
    Induct();
public:
    INDUCT_MANAGER_LIB_EXPORT Induct(const InductProcessBundleCallback_t & inductProcessBundleCallback, const induct_element_config_t & inductConfig);
    INDUCT_MANAGER_LIB_EXPORT virtual ~Induct();
    INDUCT_MANAGER_LIB_EXPORT virtual bool Init(); //optional
    virtual void PopulateInductTelemetry(InductTelemetry_t& inductTelem) = 0;

    //tcpcl only
    INDUCT_MANAGER_LIB_EXPORT bool ForwardOnOpportunisticLink(const uint64_t remoteNodeId, padded_vector_uint8_t& dataVec, const uint32_t timeoutSeconds);
    INDUCT_MANAGER_LIB_EXPORT bool ForwardOnOpportunisticLink(const uint64_t remoteNodeId, zmq::message_t & dataZmq, const uint32_t timeoutSeconds);
    INDUCT_MANAGER_LIB_EXPORT bool ForwardOnOpportunisticLink(const uint64_t remoteNodeId, const uint8_t* bundleData, const std::size_t size, const uint32_t timeoutSeconds);

protected:
    struct OpportunisticBundleQueue { //tcpcl only
        INDUCT_MANAGER_LIB_EXPORT OpportunisticBundleQueue();
        INDUCT_MANAGER_LIB_EXPORT ~OpportunisticBundleQueue();
        INDUCT_MANAGER_LIB_EXPORT std::size_t GetQueueSize() const noexcept;
        INDUCT_MANAGER_LIB_EXPORT void PushMove_ThreadSafe(zmq::message_t & msg);
        INDUCT_MANAGER_LIB_EXPORT void PushMove_ThreadSafe(padded_vector_uint8_t& msg);
        INDUCT_MANAGER_LIB_EXPORT void PushMove_ThreadSafe(std::pair<std::unique_ptr<zmq::message_t>, padded_vector_uint8_t> & msgPair);
        INDUCT_MANAGER_LIB_EXPORT bool TryPop_ThreadSafe(std::pair<std::unique_ptr<zmq::message_t>, padded_vector_uint8_t> & msgPair);
        INDUCT_MANAGER_LIB_EXPORT void WaitUntilNotifiedOr250MsTimeout(const uint64_t waitWhileSizeGeThisValue);
        INDUCT_MANAGER_LIB_EXPORT void NotifyAll();

        boost::mutex m_mutex;
        boost::condition_variable m_conditionVariable;
        std::queue<std::pair<std::unique_ptr<zmq::message_t>, padded_vector_uint8_t> > m_dataToSendQueue;
        //BidirectionalLink * m_bidirectionalLinkPtr;
        uint64_t m_remoteNodeId;
        unsigned int m_maxTxBundlesInPipeline;
    };
    //tcpcl only
    INDUCT_MANAGER_LIB_EXPORT bool BundleSinkTryGetData_FromIoServiceThread(OpportunisticBundleQueue & opportunisticBundleQueue, std::pair<std::unique_ptr<zmq::message_t>, padded_vector_uint8_t> & bundleDataPair);
    INDUCT_MANAGER_LIB_EXPORT void BundleSinkNotifyOpportunisticDataAcked_FromIoServiceThread(OpportunisticBundleQueue & opportunisticBundleQueue);
    INDUCT_MANAGER_LIB_EXPORT bool ForwardOnOpportunisticLink(const uint64_t remoteNodeId, zmq::message_t * zmqMsgPtr, padded_vector_uint8_t* vec8Ptr, const uint32_t timeoutSeconds);
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


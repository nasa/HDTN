/**
 * @file EgressAsync.h
 * @author  Brian Tomko <brian.j.tomko@nasa.gov>
 * @author  Gilbert Clark
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
 * The egress module of HDTN is responsible for receiving bundles from
 * either the egress or storage modules and then sending those bundles
 * out of the various convergence layer outducts.
 */

#ifndef _HDTN_EGRESS_ASYNC_H
#define _HDTN_EGRESS_ASYNC_H


#include <iostream>
#include <string>

#include "message.hpp"
#include "zmq.hpp"
#include <boost/thread.hpp>
#include <boost/asio.hpp>
#include <map>
#include <queue>
#include "HdtnConfig.h"
#include "OutductManager.h"
#include "CircularIndexBufferSingleProducerSingleConsumerConfigurable.h"
#include "Logger.h"
#include "Telemetry.h"
#include "egress_async_lib_export.h"


namespace hdtn {


class HegrManagerAsync {
public:
    EGRESS_ASYNC_LIB_EXPORT HegrManagerAsync();
    EGRESS_ASYNC_LIB_EXPORT ~HegrManagerAsync();
    EGRESS_ASYNC_LIB_EXPORT void Stop();
    EGRESS_ASYNC_LIB_EXPORT void Init(const HdtnConfig & hdtnConfig, zmq::context_t * hdtnOneProcessZmqInprocContextPtr = NULL);

    //telemetry
    EgressTelemetry_t m_telemetry;
    std::size_t m_totalCustodyTransfersSentToStorage;
    std::size_t m_totalCustodyTransfersSentToIngress;

    std::unique_ptr<zmq::context_t> m_zmqCtxPtr;
    std::unique_ptr<zmq::socket_t> m_zmqPullSock_boundIngressToConnectingEgressPtr;
    std::unique_ptr<zmq::socket_t> m_zmqPushSock_connectingEgressToBoundIngressPtr;
    boost::mutex m_mutex_zmqPushSock_connectingEgressToBoundIngress;
    std::unique_ptr<zmq::socket_t> m_zmqPushSock_connectingEgressBundlesOnlyToBoundIngressPtr;
    std::unique_ptr<zmq::socket_t> m_zmqPullSock_connectingStorageToBoundEgressPtr;
    std::unique_ptr<zmq::socket_t> m_zmqPushSock_boundEgressToConnectingStoragePtr;
    boost::mutex m_mutex_zmqPushSock_boundEgressToConnectingStorage;
    std::unique_ptr<zmq::socket_t> m_zmqSubSock_boundRouterToConnectingEgressPtr;
    std::unique_ptr<zmq::socket_t> m_zmqPubSock_boundEgressToConnectingSchedulerPtr;

    std::unique_ptr<zmq::socket_t> m_zmqRepSock_connectingGuiToFromBoundEgressPtr;

    EGRESS_ASYNC_LIB_EXPORT void RouterEventHandler();
private:
    EGRESS_ASYNC_LIB_NO_EXPORT void ReadZmqThreadFunc();
    EGRESS_ASYNC_LIB_NO_EXPORT void WholeBundleReadyCallback(padded_vector_uint8_t & wholeBundleVec);
    EGRESS_ASYNC_LIB_NO_EXPORT void OnFailedBundleZmqSendCallback(zmq::message_t& movableBundle, std::vector<uint8_t>& userData, uint64_t outductUuid);
    EGRESS_ASYNC_LIB_NO_EXPORT void OnSuccessfulBundleSendCallback(std::vector<uint8_t>& userData, uint64_t outductUuid);
    EGRESS_ASYNC_LIB_NO_EXPORT void OnOutductLinkStatusChangedCallback(bool isLinkDownEvent, uint64_t outductUuid);
    EGRESS_ASYNC_LIB_NO_EXPORT void ResendOutductCapabilities();

    EGRESS_ASYNC_LIB_EXPORT void DoLinkStatusUpdate(bool isLinkDownEvent, uint64_t outductUuid);

    OutductManager m_outductManager;
    HdtnConfig m_hdtnConfig;

    boost::mutex m_mutexPushBundleToIngress;
    boost::mutex m_mutexLinkStatusUpdate;

    std::unique_ptr<boost::thread> m_threadZmqReaderPtr;
    volatile bool m_running;
};

}  // namespace hdtn

#endif

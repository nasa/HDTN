/**
 * @file ZmqStorageInterface.h
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
 * This ZmqStorageInterface class is the HDTN storage module,
 * and controls all the threads and ZMQ sockets.
 */

#ifndef _ZMQ_STORAGE_INTERFACE_H
#define _ZMQ_STORAGE_INTERFACE_H 1

#include <map>
#include <string>
#include <vector>
#include <memory>
#include <boost/thread.hpp>
#include "HdtnConfig.h"
#include "stats.hpp"
#include "zmq.hpp"
#include "codec/bpv6.h"
#include "Telemetry.h"
#include "storage_lib_export.h"

//addresses for ZMQ IPC transport
#define HDTN_STORAGE_TELEM_PATH "tcp://127.0.0.1:10460"
#define HDTN_RELEASE_TELEM_PATH "tcp://127.0.0.1:10461"


class ZmqStorageInterface {
public:
    STORAGE_LIB_EXPORT ZmqStorageInterface();
    STORAGE_LIB_EXPORT ~ZmqStorageInterface();
    STORAGE_LIB_EXPORT void Stop();
    STORAGE_LIB_EXPORT bool Init(const HdtnConfig & hdtnConfig, zmq::context_t * hdtnOneProcessZmqInprocContextPtr = NULL);
    STORAGE_LIB_EXPORT std::size_t GetCurrentNumberOfBundlesDeletedFromStorage();

    hdtn::WorkerStats stats() { return m_workerStats; }

    std::size_t m_totalBundlesErasedFromStorageNoCustodyTransfer;
    std::size_t m_totalBundlesErasedFromStorageWithCustodyTransfer;
    std::size_t m_totalBundlesSentToEgressFromStorage;
    uint64_t m_numRfc5050CustodyTransfers;
    uint64_t m_numAcsCustodyTransfers;
    uint64_t m_numAcsPacketsReceived;
    cbhe_eid_t M_HDTN_EID_CUSTODY;

private:
    std::unique_ptr<zmq::context_t> m_zmqContextPtr;
    std::unique_ptr<zmq::socket_t> m_zmqPullSock_boundIngressToConnectingStoragePtr;
    std::unique_ptr<zmq::socket_t> m_zmqSubSock_boundReleaseToConnectingStoragePtr;
    std::unique_ptr<zmq::socket_t> m_zmqPushSock_connectingStorageToBoundEgressPtr;
    std::unique_ptr<zmq::socket_t> m_zmqPullSock_boundEgressToConnectingStoragePtr;
    std::unique_ptr<zmq::socket_t> m_zmqPushSock_connectingStorageToBoundIngressPtr;

    std::unique_ptr<zmq::socket_t> m_zmqRepSock_connectingGuiToFromBoundStoragePtr;

    hdtn::StorageStats storageStats;
    HdtnConfig m_hdtnConfig;

    zmq::context_t * m_hdtnOneProcessZmqInprocContextPtr;
    std::unique_ptr<boost::thread> m_threadPtr;
    volatile bool m_running;
    volatile bool m_threadStartupComplete;
    hdtn::WorkerStats m_workerStats;

private:
    STORAGE_LIB_NO_EXPORT void ThreadFunc();
    //void Write(hdtn::block_hdr *hdr, zmq::message_t *message);
    //void ReleaseData(uint32_t flow, uint64_t rate, uint64_t duration, zmq::socket_t *egressSock);

private:



};



#endif //_ZMQ_STORAGE_INTERFACE_H

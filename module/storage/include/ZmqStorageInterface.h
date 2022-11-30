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

#include <cstdint>
#include "zmq.hpp"
#include <memory>
#include "HdtnConfig.h"
#include <boost/core/noncopyable.hpp>


#include "storage_lib_export.h"

//addresses for ZMQ IPC transport
#define HDTN_STORAGE_TELEM_PATH "tcp://127.0.0.1:10460"
#define HDTN_RELEASE_TELEM_PATH "tcp://127.0.0.1:10461"


class ZmqStorageInterface : private boost::noncopyable {
public:
    STORAGE_LIB_EXPORT ZmqStorageInterface();
    STORAGE_LIB_EXPORT ~ZmqStorageInterface();
    STORAGE_LIB_EXPORT void Stop();
    STORAGE_LIB_EXPORT bool Init(const HdtnConfig & hdtnConfig, zmq::context_t * hdtnOneProcessZmqInprocContextPtr = NULL);
    STORAGE_LIB_EXPORT std::size_t GetCurrentNumberOfBundlesDeletedFromStorage();



    // Internal implementation class
    struct Impl; //public for ostream operators
private:
    // Pointer to the internal implementation
    std::unique_ptr<Impl> m_pimpl;



public:
    std::size_t& m_totalBundlesErasedFromStorageNoCustodyTransfer;
    std::size_t& m_totalBundlesRewrittenToStorageFromFailedEgressSend;
    std::size_t& m_totalBundlesErasedFromStorageWithCustodyTransfer;
    std::size_t& m_totalBundlesSentToEgressFromStorageReadFromDisk;
    std::size_t& m_totalBundlesSentToEgressFromStorageForwardCutThrough;
    uint64_t& m_numRfc5050CustodyTransfers;
    uint64_t& m_numAcsCustodyTransfers;
    uint64_t& m_numAcsPacketsReceived;


};



#endif //_ZMQ_STORAGE_INTERFACE_H

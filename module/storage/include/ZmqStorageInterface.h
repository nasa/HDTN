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
#include "HdtnDistributedConfig.h"
#include "TelemetryDefinitions.h"
#include <boost/core/noncopyable.hpp>
#include "storage_lib_export.h"


class ZmqStorageInterface : private boost::noncopyable {
public:
    STORAGE_LIB_EXPORT ZmqStorageInterface();
    STORAGE_LIB_EXPORT ~ZmqStorageInterface();
    STORAGE_LIB_EXPORT void Stop();
    STORAGE_LIB_EXPORT bool Init(const HdtnConfig& hdtnConfig,
        const HdtnDistributedConfig& hdtnDistributedConfig,
        zmq::context_t* hdtnOneProcessZmqInprocContextPtr = NULL);
    STORAGE_LIB_EXPORT std::size_t GetCurrentNumberOfBundlesDeletedFromStorage();



    // Internal implementation class
    struct Impl; //public for ostream operators
private:
    // Pointer to the internal implementation
    std::unique_ptr<Impl> m_pimpl;



public:
    StorageTelemetry_t& m_telemRef;
};



#endif //_ZMQ_STORAGE_INTERFACE_H

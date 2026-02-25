/**
 * @file ZmqStorageInterface.h
 * @author  Brian Tomko <brian.j.tomko@nasa.gov>
 *
 * @copyright Copyright (c) 2026 United States Government as represented by
 * the National Aeronautics and Space Administration.
 * No copyright is claimed in the United States under Title 17, U.S.Code.
 * All Other Rights Reserved.
 *
 * @section LICENSE
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
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

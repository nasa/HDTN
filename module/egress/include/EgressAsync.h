/**
 * @file EgressAsync.h
 * @author  Brian Tomko <brian.j.tomko@nasa.gov>
 * @author  Gilbert Clark
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
 * The egress module of HDTN is responsible for receiving bundles from
 * either the egress or storage modules and then sending those bundles
 * out of the various convergence layer outducts.
 */

#ifndef _HDTN_EGRESS_ASYNC_H
#define _HDTN_EGRESS_ASYNC_H


#include <cstdint>
#include "zmq.hpp"
#include <memory>
#include "HdtnConfig.h"
#include "HdtnDistributedConfig.h"
#include "TelemetryDefinitions.h"
#include <boost/core/noncopyable.hpp>
#include "egress_async_lib_export.h"


namespace hdtn {


class Egress : private boost::noncopyable {
public:
    EGRESS_ASYNC_LIB_EXPORT Egress();
    EGRESS_ASYNC_LIB_EXPORT ~Egress();
    EGRESS_ASYNC_LIB_EXPORT void Stop();
    EGRESS_ASYNC_LIB_EXPORT bool Init(const HdtnConfig& hdtnConfig,
        const HdtnDistributedConfig& hdtnDistributedConfig,
        zmq::context_t * hdtnOneProcessZmqInprocContextPtr = NULL);

private:

    // Internal implementation class
    struct Impl;
private:
    // Pointer to the internal implementation
    std::unique_ptr<Impl> m_pimpl;
    
public:
    //telemetry
    AllOutductTelemetry_t& m_allOutductTelemRef;
    std::size_t& m_totalCustodyTransfersSentToStorage;
    std::size_t& m_totalCustodyTransfersSentToIngress;
};

}  // namespace hdtn

#endif

/**
 * @file ingress.h
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
 * The ingress module of HDTN is responsible for receiving bundles, decoding them, and
 * forwarding them to either the egress or storage modules.
 */

#ifndef _HDTN_INGRESS_H
#define _HDTN_INGRESS_H

#include <cstdint>
#include "zmq.hpp"
#include <memory>
#include "HdtnConfig.h"
#include "HdtnDistributedConfig.h"
#include <boost/atomic.hpp>
#include <boost/core/noncopyable.hpp>
#include "ingress_async_lib_export.h"

namespace hdtn {


class Ingress : private boost::noncopyable {
public:
    INGRESS_ASYNC_LIB_EXPORT Ingress();  // initialize message buffers
    INGRESS_ASYNC_LIB_EXPORT ~Ingress();
    INGRESS_ASYNC_LIB_EXPORT void Stop();
    INGRESS_ASYNC_LIB_EXPORT bool Stopped() noexcept;
    INGRESS_ASYNC_LIB_EXPORT bool Init(const HdtnConfig& hdtnConfig,
        const boost::filesystem::path& bpSecConfigFilePath, const HdtnDistributedConfig& hdtnDistributedConfig,
        zmq::context_t* hdtnOneProcessZmqInprocContextPtr = NULL, const std::string& maskerImpl = "");
private:

    // Internal implementation class
    struct Impl;
private:
    // Pointer to the internal implementation
    std::unique_ptr<Impl> m_pimpl;

public:
    uint64_t& m_bundleCountStorage;
    uint64_t& m_bundleByteCountStorage;
    uint64_t& m_bundleCountEgress;
    uint64_t& m_bundleByteCountEgress;
};


}  // namespace hdtn

#endif  //_HDTN_INGRESS_H

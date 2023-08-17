/**
 * @file ingress.h
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
    INGRESS_ASYNC_LIB_EXPORT bool Init(const HdtnConfig& hdtnConfig,
        const boost::filesystem::path& bpSecConfigFilePath, const HdtnDistributedConfig& hdtnDistributedConfig,
        zmq::context_t* hdtnOneProcessZmqInprocContextPtr = NULL, std::string leiderImpl = "redundant");
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

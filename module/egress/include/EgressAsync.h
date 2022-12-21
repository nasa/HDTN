/**
 * @file EgressAsync.h
 * @author  Brian Tomko <brian.j.tomko@nasa.gov>
 * @author  Gilbert Clark
 *
 * @copyright Copyright � 2021 United States Government as represented by
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


#include <cstdint>
#include "zmq.hpp"
#include <memory>
#include "HdtnConfig.h"
#include "TelemetryDefinitions.h"
#include <boost/core/noncopyable.hpp>
#include "egress_async_lib_export.h"


namespace hdtn {


class Egress : private boost::noncopyable {
public:
    EGRESS_ASYNC_LIB_EXPORT Egress();
    EGRESS_ASYNC_LIB_EXPORT ~Egress();
    EGRESS_ASYNC_LIB_EXPORT void Stop();
    EGRESS_ASYNC_LIB_EXPORT bool Init(const HdtnConfig & hdtnConfig, zmq::context_t * hdtnOneProcessZmqInprocContextPtr = NULL);

private:

    // Internal implementation class
    struct Impl;
private:
    // Pointer to the internal implementation
    std::unique_ptr<Impl> m_pimpl;
    
public:
    //telemetry
    EgressTelemetry_t& m_telemetry;
    std::size_t& m_totalCustodyTransfersSentToStorage;
    std::size_t& m_totalCustodyTransfersSentToIngress;
};

}  // namespace hdtn

#endif

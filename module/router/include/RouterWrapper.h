/**
 * @file router.h
 * @author Nadia Kortas <nadia.kortas@nasa.gov>
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
 * Router - sends events to egress on the optimal route and next hop.
 */

#ifndef ROUTERWRAPPER_H
#define ROUTERWRAPPER_H 1

#include <cstdint>
#include "zmq.hpp"
#include <memory>
#include "HdtnConfig.h"
#include "HdtnDistributedConfig.h"
#include <boost/core/noncopyable.hpp>
#include <boost/filesystem.hpp>
#include "router_lib_export.h"

#include "router.h"


class RouterWrapper : private boost::noncopyable {
public:
    ROUTER_LIB_EXPORT RouterWrapper();
    ROUTER_LIB_EXPORT ~RouterWrapper();
    ROUTER_LIB_EXPORT void Stop();
    ROUTER_LIB_EXPORT bool Init(const HdtnConfig& hdtnConfig,
        const HdtnDistributedConfig& hdtnDistributedConfig,
        const boost::filesystem::path& contactPlanFilePath,
        bool usingUnixTimestamp,
        bool useMgr,
        zmq::context_t* hdtnOneProcessZmqInprocContextPtr = NULL);

    ROUTER_LIB_EXPORT static boost::filesystem::path GetFullyQualifiedFilename(const boost::filesystem::path& filename);


private:
    Router m_router;
};


#endif // ROUTERWRAPPER_H

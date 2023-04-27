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
 * This Router class is a process which sends LINKUP or LINKDOWN events to Ingress and Storage modules to determine if a given bundle 
 * should be forwarded immediately to Egress or should be stored. 
 * To determine a given link availability, i.e. when a contact to a particular neighbor node exists, the router reads a contact plan 
 * which is a JSON file that defines all the connections between all the nodes in the network.
 */


#ifndef ROUTER_H 
#define ROUTER_H 1



#include <cstdint>
#include "zmq.hpp"
#include <memory>
#include <boost/bimap.hpp>
#include <boost/asio.hpp>
#include "HdtnConfig.h"
#include "HdtnDistributedConfig.h"
#include <boost/core/noncopyable.hpp>
#include <boost/filesystem.hpp>
#include <boost/property_tree/ptree.hpp>
#include "router_lib_export.h"

#include "TimestampUtil.h"
#include "Logger.h"
#include <fstream>
#include "message.hpp"
#include <boost/filesystem.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/date_time.hpp>
#include <boost/make_unique.hpp>
#include <memory>
#include <fstream>
#include "TelemetryDefinitions.h"
#include <boost/asio.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/foreach.hpp>
#include <boost/bimap.hpp>
#include <cstdlib>
#include "Environment.h"
#include "JsonSerializable.h"
#include "codec/BundleViewV6.h"
#include "codec/BundleViewV7.h"

class Router : private boost::noncopyable {
public:
    ROUTER_LIB_EXPORT Router();
    ROUTER_LIB_EXPORT ~Router();
    ROUTER_LIB_EXPORT void Stop();
    ROUTER_LIB_EXPORT bool Init(const HdtnConfig& hdtnConfig,
        const HdtnDistributedConfig& hdtnDistributedConfig,
        const boost::filesystem::path& contactPlanFilePath,
        bool usingUnixTimestamp,
        bool useMgr,
        zmq::context_t* hdtnOneProcessZmqInprocContextPtr = NULL);

    ROUTER_LIB_EXPORT static boost::filesystem::path GetFullyQualifiedFilename(const boost::filesystem::path& filename);
    ROUTER_LIB_EXPORT static uint64_t GetRateBpsFromPtree(const boost::property_tree::ptree::value_type& eventPtr);

    // Internal implementation class
    class Impl; //public for ostream operators

private:
    // Pointer to the internal implementation
    std::unique_ptr<Impl> m_pimpl;


};

#endif // ROUTER_H

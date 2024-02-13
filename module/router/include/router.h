/**
 * @file router.h
 * @author Nadia Kortas <nadia.kortas@nasa.gov>
 *
 * @copyright Copyright (c) 2021 United States Government as represented by
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
#include "HdtnConfig.h"
#include "HdtnDistributedConfig.h"
#include <boost/core/noncopyable.hpp>
#include <boost/filesystem/path.hpp>
#include <boost/property_tree/ptree.hpp>
#include "router_lib_export.h"

/** HDTN Router.
 *
 * Notifies other modules of link up/down events
 * and provides them with new routes on link change
 */
class Router : private boost::noncopyable {
public:
    /** Constructs a Router instance */
    ROUTER_LIB_EXPORT Router();
    /** Destroys a Router instance. Stops the router if running */
    ROUTER_LIB_EXPORT ~Router();
    /** Stops a running router instance */
    ROUTER_LIB_EXPORT void Stop();
    /** Starts the router
     *
     * @param hdtnConfig The HDTN Config
     * @param hdtnDistributedConfig HDTN config for running in distributed mode
     * @param usingUnixTimestamp If true, interpret times in contact file as unix time stamps
     * @param useMgr if true, use the MGR routing algorithm; otherwise use CGR
     * @param hdtnOneProcessZmqInprocContextPtr ZMQ context for one-process mode
     * 
     * @returns true on successful start, false on error
     *
     * Starts the router on a thread; returns once initial setup is complete 
     */
    ROUTER_LIB_EXPORT bool Init(const HdtnConfig& hdtnConfig,
        const HdtnDistributedConfig& hdtnDistributedConfig,
        const boost::filesystem::path& contactPlanFilePath,
        bool usingUnixTimestamp,
        bool useMgr,
        zmq::context_t* hdtnOneProcessZmqInprocContextPtr = NULL);

    /** Get absolute path to contact plan from relative path
     *
     * @param filename the name of the file to find an absolute path for
     *
     * @returns the absolute path to the file
     *
     * This path is relative to the router/contact_plans directory
     */
    ROUTER_LIB_EXPORT static boost::filesystem::path GetFullyQualifiedFilename(const boost::filesystem::path& filename);

    /** Get the contact rate in bits per second from its property tree 
     *
     * @param eventPtr The contact plan property tree to extract the bits per second from 
     *
     * @returns the bit rate on success, 0 on error
     */
    ROUTER_LIB_EXPORT static uint64_t GetRateBpsFromPtree(const boost::property_tree::ptree::value_type& eventPtr);

    // Internal implementation class
    class Impl; //public for ostream operators

private:
    // Pointer to the internal implementation
    std::unique_ptr<Impl> m_pimpl;


};

#endif // ROUTER_H

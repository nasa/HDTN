/**
 * @file scheduler.h
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
 * This Scheduler class is a process which sends LINKUP or LINKDOWN events to Ingress and Storage modules to determine if a given bundle 
 * should be forwarded immediately to Egress or should be stored. 
 * To determine a given link availability, i.e. when a contact to a particular neighbor node exists, the scheduler reads a contact plan 
 * which is a JSON file that defines all the connections between all the nodes in the network.
 */


#ifndef SCHEDULER_H
#define SCHEDULER_H 



#include <cstdint>
#include "zmq.hpp"
#include <memory>
#include "HdtnConfig.h"
#include <boost/core/noncopyable.hpp>
#include <boost/filesystem.hpp>
#include "scheduler_lib_export.h"


class Scheduler : private boost::noncopyable {
public:
    SCHEDULER_LIB_EXPORT Scheduler();
    SCHEDULER_LIB_EXPORT ~Scheduler();
    SCHEDULER_LIB_EXPORT void Stop();
    SCHEDULER_LIB_EXPORT bool Init(const HdtnConfig& hdtnConfig,
        const boost::filesystem::path & contactPlanFilePath,
        zmq::context_t* hdtnOneProcessZmqInprocContextPtr = NULL);

    SCHEDULER_LIB_EXPORT static boost::filesystem::path GetFullyQualifiedFilename(const boost::filesystem::path& filename);


    // Internal implementation class
    class Impl; //public for ostream operators
private:
    // Pointer to the internal implementation
    std::unique_ptr<Impl> m_pimpl;


};

#endif // SCHEDULER_H

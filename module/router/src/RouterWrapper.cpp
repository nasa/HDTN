/**
 * @file router.cpp
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
 */

#include "RouterWrapper.h"
#include "Logger.h"
#include "Environment.h"
#include <boost/filesystem.hpp>

static constexpr hdtn::Logger::SubProcess subprocess = hdtn::Logger::SubProcess::router;

boost::filesystem::path RouterWrapper::GetFullyQualifiedFilename(const boost::filesystem::path& filename) {
    return (Environment::GetPathHdtnSourceRoot() / "module/router/contact_plans/") / filename;
}

RouterWrapper::RouterWrapper() {}

RouterWrapper::~RouterWrapper() {
    Stop();
}

bool RouterWrapper::Init(const HdtnConfig& hdtnConfig,
    const HdtnDistributedConfig& hdtnDistributedConfig,
    const boost::filesystem::path& contactPlanFilePath,
    bool usingUnixTimestamp,
    bool useMgr,
    zmq::context_t* hdtnOneProcessZmqInprocContextPtr)
{
    if(!m_router.Init(hdtnConfig, hdtnDistributedConfig, contactPlanFilePath, usingUnixTimestamp, useMgr, hdtnOneProcessZmqInprocContextPtr)) {
        LOG_ERROR(subprocess) << "Failed to start m_router";
        return false;
    }
    m_scheduler.m_router = &m_router;
    if(!m_scheduler.Init(hdtnConfig, hdtnDistributedConfig, contactPlanFilePath, usingUnixTimestamp, hdtnOneProcessZmqInprocContextPtr)) {
        LOG_ERROR(subprocess) << "Failed to start m_router";
        return false;
    }
    return true;
}

void RouterWrapper::Stop() {
    m_router.Stop();
    m_scheduler.Stop();
}

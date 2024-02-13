/**
 * @file ingress.cpp
 * @author  Brian Tomko <brian.j.tomko@nasa.gov>
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
 * This file provides the "int main()" function to wrap IngressAsyncRunner
 * and forward command line arguments to IngressAsyncRunner.
 * This file is only used when running HDTN in distributed mode in which there
 * is a single process dedicated to the Ingress module.
 */

#include <iostream>
#include "IngressAsyncRunner.h"
#include "Logger.h"
#include "ThreadNamer.h"

static constexpr hdtn::Logger::SubProcess subprocess = hdtn::Logger::SubProcess::ingress;

int main(int argc, const char* argv[]) {


    hdtn::Logger::initializeWithProcess(hdtn::Logger::Process::ingress);
    ThreadNamer::SetThisThreadName("IngressMain");
    IngressAsyncRunner runner;
    std::atomic<bool> running;
    runner.Run(argc, argv, running, true);
    LOG_DEBUG(subprocess) << "m_bundleCountStorage: " << runner.m_bundleCountStorage;
    LOG_DEBUG(subprocess) << "m_bundleCountEgress: " << runner.m_bundleCountEgress;
    LOG_DEBUG(subprocess) << "m_bundleCount: " << runner.m_bundleCount;
    LOG_DEBUG(subprocess) << "m_bundleData: " << runner.m_bundleData;

    return 0;

}

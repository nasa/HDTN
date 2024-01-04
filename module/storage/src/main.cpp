/**
 * @file main.cpp
 * @author  Brian Tomko <brian.j.tomko@nasa.gov>
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
 * This file provides the "int main()" function to wrap StorageRunner
 * and forward command line arguments to StorageRunner.
 * This file is only used when running HDTN in distributed mode in which there
 * is a single process dedicated to the Storage module.
 */

#include "StorageRunner.h"
#include "Logger.h"
#include "ThreadNamer.h"

int main(int argc, const char* argv[]) {


    hdtn::Logger::initializeWithProcess(hdtn::Logger::Process::storage);
    ThreadNamer::SetThisThreadName("StorageMain");
    StorageRunner runner;
    std::atomic<bool> running;
    runner.Run(argc, argv, running, true);
    LOG_DEBUG(hdtn::Logger::SubProcess::storage) << "totalBundlesErasedFromStorage: " << runner.m_totalBundlesErasedFromStorage;
    LOG_DEBUG(hdtn::Logger::SubProcess::storage) << "totalBundlesSentToEgressFromStorage: " << runner.m_totalBundlesSentToEgressFromStorage;
    return 0;

}

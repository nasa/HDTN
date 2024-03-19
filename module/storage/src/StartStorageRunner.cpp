/**
 * @file StartStorageRunner.cpp
 * @author Tad Kollar <tad.kollar@nasa.gov>
 *
 * @copyright Copyright (c) 2024 United States Government as represented by
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
#include "StartStorageRunner.h"

int startStorageRunner(int argc, const char* argv[]) {
    hdtn::Logger::initializeWithProcess(hdtn::Logger::Process::storage);
    ThreadNamer::SetThisThreadName("StorageMain");
    StorageRunner runner;
    std::atomic<bool> running;
    try {
        runner.Run(argc, argv, running, true);
    }
    catch (const zmq::error_t& ex) {
        LOG_ERROR(hdtn::Logger::SubProcess::storage) << "ZeroMQ error: " << ex.what();
        return 1;
    }

    LOG_DEBUG(hdtn::Logger::SubProcess::storage) << "totalBundlesErasedFromStorage: " << runner.m_totalBundlesErasedFromStorage;
    LOG_DEBUG(hdtn::Logger::SubProcess::storage) << "totalBundlesSentToEgressFromStorage: " << runner.m_totalBundlesSentToEgressFromStorage;
    return 0;
}

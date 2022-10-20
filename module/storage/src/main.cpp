/**
 * @file main.cpp
 * @author  Brian Tomko <brian.j.tomko@nasa.gov>
 *
 * @copyright Copyright � 2021 United States Government as represented by
 * the National Aeronautics and Space Administration.
 * No copyright is claimed in the United States under Title 17, U.S.Code.
 * All Other Rights Reserved.
 *
 * @section LICENSE
 * Released under the NASA Open Source Agreement (NOSA)
 * See LICENSE.md in the source root directory for more information.
 */

#include <iostream>
#include "StorageRunner.h"
#include "Logger.h"


int main(int argc, const char* argv[]) {


    hdtn::Logger::initializeWithProcess(hdtn::Logger::Process::storage);
    StorageRunner runner;
    volatile bool running;
    runner.Run(argc, argv, running, true);
    LOG_DEBUG(hdtn::Logger::Module::storage) << "totalBundlesErasedFromStorage: " << runner.m_totalBundlesErasedFromStorage;
    LOG_DEBUG(hdtn::Logger::Module::storage) << "totalBundlesSentToEgressFromStorage: " << runner.m_totalBundlesSentToEgressFromStorage;
    return 0;

}

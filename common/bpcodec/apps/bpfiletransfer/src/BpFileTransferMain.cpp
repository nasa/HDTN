/**
 * @file BpFileTransferMain.cpp
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
 */

#include <iostream>
#include "BpFileTransferRunner.h"
#include "Logger.h"
#include "ThreadNamer.h"

int main(int argc, const char* argv[]) {
    
    hdtn::Logger::initializeWithProcess(hdtn::Logger::Process::bpsendfile);
    ThreadNamer::SetThisThreadName("BpFileTransferMain");
    BpFileTransferRunner runner;
    std::atomic<bool> running;
    runner.Run(argc, argv, running, true);
    //LOG_INFO(hdtn::Logger::SubProcess::none) << "bundle count main: " << runner.m_bundleCount;
    return 0;

}

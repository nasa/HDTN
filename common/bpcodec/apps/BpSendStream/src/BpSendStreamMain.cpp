/**
 * @file BpSendStreamMain.cpp
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
#include "BpSendStreamRunner.h"
#include "Logger.h"
#include "ThreadNamer.h"

int main(int argc, const char* argv[]) {

#if 0
    const char * manualArgv[5] = { "bpgen", "--bundle-rate=200", "--use-tcpcl", "--flow-id=2", NULL };
    argv = manualArgv;
    argc = 4;
#endif

    hdtn::Logger::initializeWithProcess(hdtn::Logger::Process::bpsendstream);
    ThreadNamer::SetThisThreadName("BpSendStream");
    BpSendStreamRunner runner;
    std::atomic<bool> running;
    runner.Run(argc, argv, running, true);
    LOG_INFO(hdtn::Logger::SubProcess::none) << "bundle count main: " << runner.m_bundleCount;
    return 0;

}

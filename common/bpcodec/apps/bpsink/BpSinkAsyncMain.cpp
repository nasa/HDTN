/**
 * @file BpSinkAsyncMain.cpp
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
#include "BpSinkAsyncRunner.h"
#include "Logger.h"
#include "ThreadNamer.h"

int main(int argc, const char* argv[]) {


    hdtn::Logger::initializeWithProcess(hdtn::Logger::Process::bpsink);
    ThreadNamer::SetThisThreadName("BpSinkMain");
    BpSinkAsyncRunner runner;
    std::atomic<bool> running;
    runner.Run(argc, argv, running, true);
    LOG_INFO(hdtn::Logger::SubProcess::none) << "Rx Count, Duplicate Count, Total Count, Total bytes Rx";
    LOG_INFO(hdtn::Logger::SubProcess::none) << runner.m_receivedCount 
        << "," << runner.m_duplicateCount 
        << "," << (runner.m_receivedCount + runner.m_duplicateCount)
        << "," << runner.m_totalBytesRx;
    return 0;

}

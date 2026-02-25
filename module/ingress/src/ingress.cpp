/**
 * @file ingress.cpp
 * @author  Brian Tomko <brian.j.tomko@nasa.gov>
 *
 * @copyright Copyright (c) 2026 United States Government as represented by
 * the National Aeronautics and Space Administration.
 * No copyright is claimed in the United States under Title 17, U.S.Code.
 * All Other Rights Reserved.
 *
 * @section LICENSE
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
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


int main(int argc, const char* argv[]) {


    hdtn::Logger::initializeWithProcess(hdtn::Logger::Process::ingress);
    ThreadNamer::SetThisThreadName("IngressMain");
    IngressAsyncRunner runner;
    std::atomic<bool> running;
    runner.Run(argc, argv, running, true);
    LOG_DEBUG(hdtn::Logger::SubProcess::ingress) << "m_bundleCountStorage: " << runner.m_bundleCountStorage;
    LOG_DEBUG(hdtn::Logger::SubProcess::ingress) << "m_bundleCountEgress: " << runner.m_bundleCountEgress;
    LOG_DEBUG(hdtn::Logger::SubProcess::ingress) << "m_bundleCount: " << runner.m_bundleCount;
    LOG_DEBUG(hdtn::Logger::SubProcess::ingress) << "m_bundleData: " << runner.m_bundleData;

    return 0;

}

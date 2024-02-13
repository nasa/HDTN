/**
 * @file HdtnOneProcessMain.cpp
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
 * This file provides the "int main()" function to wrap HdtnOneProcessRunner
 * and forward command line arguments to HdtnOneProcessRunner.
 * This file is only used when running HDTN in single-process mode in which there
 * is a single process dedicated to the Ingress, Egress, Storage, and GUI modules.
 */


#include <iostream>
#include "HdtnOneProcessRunner.h"
#include "Logger.h"
#include "ThreadNamer.h"

static constexpr hdtn::Logger::SubProcess subprocess = hdtn::Logger::SubProcess::none;

int main(int argc, const char* argv[]) {


    hdtn::Logger::initializeWithProcess(hdtn::Logger::Process::hdtnoneprocess);
    ThreadNamer::SetThisThreadName("HdtnOneProcessMain");
    HdtnOneProcessRunner runner;
    std::atomic<bool> running;
    runner.Run(argc, argv, running, true);
    LOG_INFO(subprocess) << "m_ingressBundleCountStorage: " << runner.m_ingressBundleCountStorage;
    LOG_INFO(subprocess) << "m_ingressBundleCountEgress: " << runner.m_ingressBundleCountEgress;
    LOG_INFO(subprocess) << "m_ingressBundleCount: " << runner.m_ingressBundleCount;
    LOG_INFO(subprocess) << "m_ingressBundleData: " << runner.m_ingressBundleData;

    LOG_INFO(subprocess) << "Egress: Bundle Count, Bundle data bytes";
    LOG_INFO(subprocess) << runner.m_egressTotalBundlesGivenToOutducts << "," << runner.m_egressTotalBundleBytesGivenToOutducts;

    LOG_INFO(subprocess) << "totalBundlesErasedFromStorage: " << runner.m_totalBundlesErasedFromStorage;
    LOG_INFO(subprocess) << "totalBundlesSentToEgressFromStorage: " << runner.m_totalBundlesSentToEgressFromStorage;

    return 0;

}

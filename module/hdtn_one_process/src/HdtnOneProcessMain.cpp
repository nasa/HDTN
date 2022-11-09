/***************************************************************************
 * NASA Glenn Research Center, Cleveland, OH
 * Released under the NASA Open Source Agreement (NOSA)
 * May  2021
 *
 * Ingress- CL adapter that accepts traffic in bundle format
 ****************************************************************************
 */


#include <iostream>
#include "HdtnOneProcessRunner.h"
#include "Logger.h"

static constexpr hdtn::Logger::SubProcess subprocess = hdtn::Logger::SubProcess::none;

int main(int argc, const char* argv[]) {


    hdtn::Logger::initializeWithProcess(hdtn::Logger::Process::hdtnoneprocess);
    HdtnOneProcessRunner runner;
    volatile bool running;
    runner.Run(argc, argv, running, true);
    LOG_INFO(subprocess) << "m_ingressBundleCountStorage: " << runner.m_ingressBundleCountStorage;
    LOG_INFO(subprocess) << "m_ingressBundleCountEgress: " << runner.m_ingressBundleCountEgress;
    LOG_INFO(subprocess) << "m_ingressBundleCount: " << runner.m_ingressBundleCount;
    LOG_INFO(subprocess) << "m_ingressBundleData: " << runner.m_ingressBundleData;

    LOG_INFO(subprocess) << "Egress: Msg Count, Bundle Count, Bundle data bytes";
    LOG_INFO(subprocess) << runner.m_egressMessageCount << "," << runner.m_egressBundleCount << "," << runner.m_egressBundleData;

    LOG_INFO(subprocess) << "totalBundlesErasedFromStorage: " << runner.m_totalBundlesErasedFromStorage;
    LOG_INFO(subprocess) << "totalBundlesSentToEgressFromStorage: " << runner.m_totalBundlesSentToEgressFromStorage;

    return 0;

}

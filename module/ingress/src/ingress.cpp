/***************************************************************************
 * NASA Glenn Research Center, Cleveland, OH
 * Released under the NASA Open Source Agreement (NOSA)
 * May  2021
 *
 * Ingress- CL adapter that accepts traffic in bundle format
 ****************************************************************************
 */


#include <iostream>
#include "IngressAsyncRunner.h"
#include "Logger.h"

static constexpr hdtn::Logger::SubProcess subprocess = hdtn::Logger::SubProcess::ingress;

int main(int argc, const char* argv[]) {


    hdtn::Logger::initializeWithProcess(hdtn::Logger::Process::ingress);
    IngressAsyncRunner runner;
    volatile bool running;
    runner.Run(argc, argv, running, true);
    LOG_DEBUG(subprocess) << "m_bundleCountStorage: " << runner.m_bundleCountStorage;
    LOG_DEBUG(subprocess) << "m_bundleCountEgress: " << runner.m_bundleCountEgress;
    LOG_DEBUG(subprocess) << "m_bundleCount: " << runner.m_bundleCount;
    LOG_DEBUG(subprocess) << "m_bundleData: " << runner.m_bundleData;

    return 0;

}

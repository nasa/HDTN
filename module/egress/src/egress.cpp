/***************************************************************************
 * NASA Glenn Research Center, Cleveland, OH
 * May, 2021
 *
 * Egress - CL adapter that forwards bundle traffic
 ****************************************************************************
 */

#include "EgressAsyncRunner.h"
#include "Logger.h"

int main(int argc, const char* argv[]) {
    hdtn::Logger::initializeWithProcess(hdtn::Logger::Process::egress);
    EgressAsyncRunner runner;
    volatile bool running;
    runner.Run(argc, argv, running, true);
    LOG_DEBUG(hdtn::Logger::SubProcess::egress) << "Msg Count, Bundle Count, Bundle data bytes";
    LOG_DEBUG(hdtn::Logger::SubProcess::egress) << runner.m_messageCount << "," << runner.m_bundleCount << "," << runner.m_bundleData;
    return 0;
}

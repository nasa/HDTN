/***************************************************************************
 * NASA Glenn Research Center, Cleveland, OH
 * May, 2021
 *
 * Egress - CL adapter that forwards bundle traffic
 ****************************************************************************
 */

#include <iostream>
#include "EgressAsyncRunner.h"

int main(int argc, const char* argv[]) {
    hdtn::Logger::initializeWithProcess(hdtn::Logger::Process::egress);
    EgressAsyncRunner runner;
    volatile bool running;
    runner.Run(argc, argv, running, true);
    std::cout << "Msg Count, Bundle Count, Bundle data bytes\n";
    std::cout << runner.m_messageCount << "," << runner.m_bundleCount << "," << runner.m_bundleData << "\n";
    hdtn::Logger::getInstance()->logInfo("egress", "Msg Count: " + std::to_string(runner.m_messageCount));
    hdtn::Logger::getInstance()->logInfo("egress", "Bundle Count: " + std::to_string(runner.m_bundleCount));
    hdtn::Logger::getInstance()->logInfo("egress", "Bundle data bytes: " + std::to_string(runner.m_bundleData));
    return 0;
}

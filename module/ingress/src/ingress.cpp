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

int main(int argc, const char* argv[]) {


    hdtn::Logger::initializeWithProcess(hdtn::Logger::Process::ingress);
    IngressAsyncRunner runner;
    volatile bool running;
    runner.Run(argc, argv, running, true);
    std::cout << "m_bundleCountStorage: " << runner.m_bundleCountStorage << std::endl;
    std::cout << "m_bundleCountEgress: " << runner.m_bundleCountEgress << std::endl;
    std::cout << "m_bundleCount: " << runner.m_bundleCount << std::endl;
    std::cout << "m_bundleData: " << runner.m_bundleData << std::endl;
    hdtn::Logger::getInstance()->logInfo("ingress", "m_bundleCountStorage: " + std::to_string(runner.m_bundleCountStorage));
    hdtn::Logger::getInstance()->logInfo("ingress", "m_bundleCountEgress: " + std::to_string(runner.m_bundleCountEgress));
    hdtn::Logger::getInstance()->logInfo("ingress", "m_bundleCount: " + std::to_string(runner.m_bundleCount));
    hdtn::Logger::getInstance()->logInfo("ingress", "m_bundleData: " + std::to_string(runner.m_bundleData));

    return 0;

}

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

int main(int argc, const char* argv[]) {


    hdtn::Logger::initializeWithProcess(hdtn::Logger::Process::hdtnoneprocess);
    HdtnOneProcessRunner runner;
    volatile bool running;
    runner.Run(argc, argv, running, true);
    std::cout << "m_ingressBundleCountStorage: " << runner.m_ingressBundleCountStorage << std::endl;
    std::cout << "m_ingressBundleCountEgress: " << runner.m_ingressBundleCountEgress << std::endl;
    std::cout << "m_ingressBundleCount: " << runner.m_ingressBundleCount << std::endl;
    std::cout << "m_ingressBundleData: " << runner.m_ingressBundleData << std::endl;

    std::cout << "Egress: Msg Count, Bundle Count, Bundle data bytes\n";
    std::cout << runner.m_egressMessageCount << "," << runner.m_egressBundleCount << "," << runner.m_egressBundleData << "\n";

    std::cout << "totalBundlesErasedFromStorage: " << runner.m_totalBundlesErasedFromStorage << std::endl;
    std::cout << "totalBundlesSentToEgressFromStorage: " << runner.m_totalBundlesSentToEgressFromStorage << std::endl;

    //hdtn::Logger::getInstance()->logInfo("ingress", "m_ingressBundleCountStorage: " + std::to_string(runner.m_ingressBundleCountStorage));
    //hdtn::Logger::getInstance()->logInfo("ingress", "m_ingressBundleCountEgress: " + std::to_string(runner.m_ingressBundleCountEgress));
    //hdtn::Logger::getInstance()->logInfo("ingress", "m_ingressBundleCount: " + std::to_string(runner.m_ingressBundleCount));
    //hdtn::Logger::getInstance()->logInfo("ingress", "m_ingressBundleData: " + std::to_string(runner.m_ingressBundleData));

    return 0;

}

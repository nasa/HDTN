/***************************************************************************
 * NASA Glenn Research Center, Cleveland, OH
 * May, 2021
 *
 * Storage main - manages storage and release of bundles for
 * which a forward link is not immediately available
 ****************************************************************************
 */

#include <iostream>
#include "StorageRunner.h"


int main(int argc, const char* argv[]) {


    StorageRunner runner;
    volatile bool running;
    runner.Run(argc, argv, running, true);
    std::cout << "totalBundlesErasedFromStorage: " << runner.m_totalBundlesErasedFromStorage << std::endl;
    std::cout << "totalBundlesSentToEgressFromStorage: " << runner.m_totalBundlesSentToEgressFromStorage << std::endl;
    hdtn::Logger::getInstance()->logInfo("storage", "totalBundlesErasedFromStorage: " + 
        std::to_string(runner.m_totalBundlesErasedFromStorage));
    hdtn::Logger::getInstance()->logInfo("storage", "totalBundlesSentToEgressFromStorage: " + 
        std::to_string(runner.m_totalBundlesSentToEgressFromStorage));
    return 0;

}

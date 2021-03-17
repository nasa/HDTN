#include <iostream>
#include "StorageRunner.h"


int main(int argc, const char* argv[]) {


    StorageRunner runner;
    volatile bool running;
    runner.Run(argc, argv, running, true);
    std::cout << "totalBundlesErasedFromStorage: " << runner.m_totalBundlesErasedFromStorage << std::endl;
    std::cout << "totalBundlesSentToEgressFromStorage: " << runner.m_totalBundlesSentToEgressFromStorage << std::endl;
    return 0;

}

#include <iostream>
#include "EgressAsyncRunner.h"


int main(int argc, const char* argv[]) {
    
    EgressAsyncRunner runner;
    volatile bool running;
    runner.Run(argc, argv, running, true);
    std::cout << "Msg Count, Bundle Count, Bundle data bytes\n";
    std::cout << runner.m_messageCount << "," << runner.m_bundleCount << "," << runner.m_bundleData << "\n";
    return 0;

}

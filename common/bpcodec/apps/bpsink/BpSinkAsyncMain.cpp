#include <iostream>
#include "BpSinkAsyncRunner.h"
#include "Logger.h"


int main(int argc, const char* argv[]) {


    hdtn::Logger::initializeWithProcess(hdtn::Logger::Process::bpsink);
    BpSinkAsyncRunner runner;
    volatile bool running;
    runner.Run(argc, argv, running, true);
    std::cout << "Rx Count, Duplicate Count, Total bytes Rx\n";
    std::cout << runner.m_receivedCount << "," << runner.m_duplicateCount << "," << runner.m_totalBytesRx << "\n";
    return 0;

}

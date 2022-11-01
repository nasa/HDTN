#include <iostream>
#include "BpSinkAsyncRunner.h"
#include "Logger.h"


int main(int argc, const char* argv[]) {


    hdtn::Logger::initializeWithProcess(hdtn::Logger::Process::bpsink);
    BpSinkAsyncRunner runner;
    volatile bool running;
    runner.Run(argc, argv, running, true);
    LOG_INFO(hdtn:Logger::SubProcess::none) << "Rx Count, Duplicate Count, Total bytes Rx";
    LOG_INFO(hdtn:Logger::SubProcess::none) << runner.m_receivedCount << "," << runner.m_duplicateCount << "," << runner.m_totalBytesRx;
    return 0;

}

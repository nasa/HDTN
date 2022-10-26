#include <iostream>
#include "BpReceiveFileRunner.h"
#include "Logger.h"


int main(int argc, const char* argv[]) {

    hdtn::Logger::initializeWithProcess(hdtn::Logger::Process::bpreceivefile);
    BpReceiveFileRunner runner;
    volatile bool running;
    runner.Run(argc, argv, running, true);
    return 0;

}

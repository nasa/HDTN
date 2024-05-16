#include <iostream>
#include "BpReceiveStreamRunner.h"
#include "Logger.h"
#include "ThreadNamer.h"


int main(int argc, const char* argv[]) {

    hdtn::Logger::initializeWithProcess(hdtn::Logger::Process::bpreceivefile);
    ThreadNamer::SetThisThreadName("BpRecvStream");
    BpReceiveStreamRunner runner;
    std::atomic<bool> running;
    runner.Run(argc, argv, running, true);
    return 0;

}

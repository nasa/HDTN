#include <iostream>
#include "BpSendStreamRunner.h"
#include "Logger.h"
#include "ThreadNamer.h"

int main(int argc, const char* argv[]) {

#if 0
    const char * manualArgv[5] = { "bpgen", "--bundle-rate=200", "--use-tcpcl", "--flow-id=2", NULL };
    argv = manualArgv;
    argc = 4;
#endif

    hdtn::Logger::initializeWithProcess(hdtn::Logger::Process::bpsendfile);
    ThreadNamer::SetThisThreadName("BpSendStream");
    BpSendStreamRunner runner;
    std::atomic<bool> running;
    runner.Run(argc, argv, running, true);
    LOG_INFO(hdtn::Logger::SubProcess::none) << "bundle count main: " << runner.m_bundleCount;
    return 0;

}

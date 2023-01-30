#include <iostream>
#include "LtpFileTransferRunner.h"
#include "Logger.h"
#include "ThreadNamer.h"

int main(int argc, const char* argv[]) {
    
    hdtn::Logger::initializeWithProcess(hdtn::Logger::Process::ltpfiletransfer);
    ThreadNamer::SetThisThreadName("LtpFileTransferMain");
    LtpFileTransferRunner runner;
    volatile bool running;
    runner.Run(argc, argv, running, true);
    return 0;

}

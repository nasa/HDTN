#include <iostream>
#include "LtpFileTransferRunner.h"
#include "Logger.h"


int main(int argc, const char* argv[]) {
    
    hdtn::Logger::initializeWithProcess(hdtn::Logger::Process::ltpfiletransfer);
    LtpFileTransferRunner runner;
    volatile bool running;
    runner.Run(argc, argv, running, true);
    //std::cout << "bundle count main: " << runner.m_bundleCount << std::endl;
    return 0;

}

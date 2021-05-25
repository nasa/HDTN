#include <iostream>
#include "LtpFileTransferRunner.h"


int main(int argc, const char* argv[]) {
    
    LtpFileTransferRunner runner;
    volatile bool running;
    runner.Run(argc, argv, running, true);
    //std::cout << "bundle count main: " << runner.m_bundleCount << std::endl;
    return 0;

}

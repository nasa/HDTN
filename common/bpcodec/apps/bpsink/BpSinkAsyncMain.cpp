#include <iostream>
#include "BpSinkAsyncRunner.h"


int main(int argc, const char* argv[]) {


    BpSinkAsyncRunner runner;
    volatile bool running;
    runner.Run(argc, argv, running, true);
    std::cout << "Rx Count, Duplicate Count, Total bytes Rx\n";
    std::cout << runner.m_receivedCount << "," << runner.m_duplicateCount << "," << runner.m_totalBytesRx << "\n";
    return 0;

}

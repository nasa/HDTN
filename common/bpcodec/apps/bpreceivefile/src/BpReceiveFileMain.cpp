#include <iostream>
#include "BpReceiveFileRunner.h"


int main(int argc, const char* argv[]) {


    BpReceiveFileRunner runner;
    volatile bool running;
    runner.Run(argc, argv, running, true);
    return 0;

}

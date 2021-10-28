#include <iostream>
#include "BPingRunner.h"


int main(int argc, const char* argv[]) {

    
    BPingRunner runner;
    volatile bool running;
    runner.Run(argc, argv, running, true);
    return 0;

}

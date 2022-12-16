#include "TelemetryRunner.h"
#include "Logger.h"

int main(int argc, const char* argv[]) {
    {
        hdtn::Logger::initializeWithProcess(hdtn::Logger::Process::telem);

        TelemetryRunner runner;
        volatile bool running;
        runner.Run(argc, argv, running);
    }
    return 0;
}

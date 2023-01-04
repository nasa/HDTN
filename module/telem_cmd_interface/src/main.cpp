#include "Telemetry.h"
#include "Logger.h"

int main(int argc, const char* argv[]) {
    {
        hdtn::Logger::initializeWithProcess(hdtn::Logger::Process::telem);

        Telemetry telemetry;
        volatile bool running;
        telemetry.Run(argc, argv, running);
    }
    return 0;
}

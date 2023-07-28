#pragma once

#include "BpReceiveStream.h"


class BpReceiveStreamRunner {
public:
    BpReceiveStreamRunner();
    ~BpReceiveStreamRunner();
    bool Run(int argc, const char* const argv[], volatile bool & running, bool useSignalHandler);
    uint64_t m_totalBytesRx;

private:
    void MonitorExitKeypressThreadFunction();

    volatile bool m_runningFromSigHandler;
};



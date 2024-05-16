#pragma once

#include "BpReceiveStream.h"


class BpReceiveStreamRunner {
public:
    BpReceiveStreamRunner();
    ~BpReceiveStreamRunner();
    bool Run(int argc, const char* const argv[], std::atomic<bool> & running, bool useSignalHandler);
    uint64_t m_totalBytesRx;

private:
    void MonitorExitKeypressThreadFunction();

    std::atomic<bool> m_runningFromSigHandler;
};



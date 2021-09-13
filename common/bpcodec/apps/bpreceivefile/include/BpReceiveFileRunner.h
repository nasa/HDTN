#ifndef _BP_RECEIVE_FILE_RUNNER_H
#define _BP_RECEIVE_FILE_RUNNER_H 1

#include <stdint.h>
#include "BpReceiveFile.h"


class BpReceiveFileRunner {
public:
    BpReceiveFileRunner();
    ~BpReceiveFileRunner();
    bool Run(int argc, const char* const argv[], volatile bool & running, bool useSignalHandler);
    uint64_t m_totalBytesRx;

private:
    void MonitorExitKeypressThreadFunction();

    volatile bool m_runningFromSigHandler;
};


#endif //_BP_RECEIVE_FILE_RUNNER_H

#ifndef _BP_SEND_FILE_RUNNER_H
#define _BP_SEND_FILE_RUNNER_H 1

#include <stdint.h>
#include "BpSendFile.h"


class BpSendFileRunner {
public:
    BpSendFileRunner();
    ~BpSendFileRunner();
    bool Run(int argc, const char* const argv[], volatile bool & running, bool useSignalHandler);
    uint64_t m_bundleCount;
    uint64_t m_totalBundlesAcked;

    OutductFinalStats m_outductFinalStats;


private:
    void MonitorExitKeypressThreadFunction();

    volatile bool m_runningFromSigHandler;
};


#endif //_BP_SEND_FILE_RUNNER_H

#ifndef _BPGEN_ASYNC_RUNNER_H
#define _BPGEN_ASYNC_RUNNER_H 1

#include <stdint.h>
#include "BpGenAsync.h"


class BpGenAsyncRunner {
public:
    BpGenAsyncRunner();
    ~BpGenAsyncRunner();
    bool Run(int argc, const char* const argv[], volatile bool & running, bool useSignalHandler);
    uint64_t m_bundleCount;
    uint64_t m_totalBundlesAcked;

    struct FinalStats m_FinalStats;


private:
    void MonitorExitKeypressThreadFunction();

    volatile bool m_runningFromSigHandler;
};


#endif //_BPGEN_ASYNC_RUNNER_H

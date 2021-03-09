#ifndef _EGRESS_ASYNC_RUNNER_H
#define _EGRESS_ASYNC_RUNNER_H 1

#include <stdint.h>


class EgressAsyncRunner {
public:
    EgressAsyncRunner();
    ~EgressAsyncRunner();
    bool Run(int argc, const char* const argv[], volatile bool & running, bool useSignalHandler);
    uint64_t m_bundleCount;
    uint64_t m_bundleData;
    uint64_t m_messageCount;

private:
    void MonitorExitKeypressThreadFunction();

    volatile bool m_runningFromSigHandler;
};


#endif //_EGRESS_ASYNC_RUNNER_H

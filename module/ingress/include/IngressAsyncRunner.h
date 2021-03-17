#ifndef _INGRESS_ASYNC_RUNNER_H
#define _INGRESS_ASYNC_RUNNER_H 1

#include <stdint.h>


class IngressAsyncRunner {
public:
    IngressAsyncRunner();
    ~IngressAsyncRunner();
    bool Run(int argc, const char* const argv[], volatile bool & running, bool useSignalHandler);
    uint64_t m_bundleCountStorage;
    uint64_t m_bundleCountEgress;
    uint64_t m_bundleCount;
    uint64_t m_bundleData;

private:
    void MonitorExitKeypressThreadFunction();

    volatile bool m_runningFromSigHandler;
};


#endif //_INGRESS_ASYNC_RUNNER_H

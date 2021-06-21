#ifndef _HDTN_ONE_PROCESS_RUNNER_H
#define _HDTN_ONE_PROCESS_RUNNER_H 1

#include <stdint.h>


class HdtnOneProcessRunner {
public:
    HdtnOneProcessRunner();
    ~HdtnOneProcessRunner();
    bool Run(int argc, const char* const argv[], volatile bool & running, bool useSignalHandler);

    //ingress
    uint64_t m_ingressBundleCountStorage;
    uint64_t m_ingressBundleCountEgress;
    uint64_t m_ingressBundleCount;
    uint64_t m_ingressBundleData;

    //egress
    uint64_t m_egressBundleCount;
    uint64_t m_egressBundleData;
    uint64_t m_egressMessageCount;

    //storage
    std::size_t m_totalBundlesErasedFromStorage;
    std::size_t m_totalBundlesSentToEgressFromStorage;

private:
    

    void MonitorExitKeypressThreadFunction();

    volatile bool m_runningFromSigHandler;
};


#endif //_HDTN_ONE_PROCESS_RUNNER_H

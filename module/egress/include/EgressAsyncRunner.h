#ifndef _EGRESS_ASYNC_RUNNER_H
#define _EGRESS_ASYNC_RUNNER_H 1

#include <stdint.h>
#include "Logger.h"
#include "egress_async_lib_export.h"

class EgressAsyncRunner {
public:
    EGRESS_ASYNC_LIB_EXPORT EgressAsyncRunner();
    EGRESS_ASYNC_LIB_EXPORT ~EgressAsyncRunner();
    EGRESS_ASYNC_LIB_EXPORT bool Run(int argc, const char* const argv[], volatile bool & running, bool useSignalHandler);
    uint64_t m_bundleCount;
    uint64_t m_bundleData;
    uint64_t m_messageCount;

private:
    EGRESS_ASYNC_LIB_EXPORT void MonitorExitKeypressThreadFunction();

    volatile bool m_runningFromSigHandler;
};


#endif //_EGRESS_ASYNC_RUNNER_H

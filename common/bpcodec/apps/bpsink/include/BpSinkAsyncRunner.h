#ifndef _BPSINK_ASYNC_RUNNER_H
#define _BPSINK_ASYNC_RUNNER_H 1

#include <stdint.h>
#include "BpSinkAsync.h"


class BpSinkAsyncRunner {
public:
    BpSinkAsyncRunner();
    ~BpSinkAsyncRunner();
    bool Run(int argc, const char* const argv[], volatile bool & running, bool useSignalHandler);
    uint64_t m_totalBytesRx;
    uint64_t m_receivedCount;
    uint64_t m_duplicateCount;
    FinalStatsBpSink m_FinalStatsBpSink;

private:
    void MonitorExitKeypressThreadFunction();

    volatile bool m_runningFromSigHandler;
};


#endif //_BPSINK_ASYNC_RUNNER_H

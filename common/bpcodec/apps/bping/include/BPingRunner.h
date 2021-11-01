#ifndef _BPING_RUNNER_H
#define _BPING_RUNNER_H 1

#include <stdint.h>
#include "BPing.h"


class BPingRunner {
public:
    BPingRunner();
    ~BPingRunner();
    bool Run(int argc, const char* const argv[], volatile bool & running, bool useSignalHandler);

private:
    void MonitorExitKeypressThreadFunction();

    volatile bool m_runningFromSigHandler;
};


#endif //_BPING_RUNNER_H

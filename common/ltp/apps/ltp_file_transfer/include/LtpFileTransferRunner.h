#ifndef _BPGEN_ASYNC_RUNNER_H
#define _BPGEN_ASYNC_RUNNER_H 1

#include <stdint.h>


class LtpFileTransferRunner {
public:
    LtpFileTransferRunner();
    ~LtpFileTransferRunner();
    bool Run(int argc, const char* const argv[], volatile bool & running, bool useSignalHandler);
    
    //FinalStats m_FinalStats;


private:
    void MonitorExitKeypressThreadFunction();

    volatile bool m_runningFromSigHandler;
};


#endif //_BPGEN_ASYNC_RUNNER_H

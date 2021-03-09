#ifndef _STORAGE_RUNNER_H
#define _STORAGE_RUNNER_H 1

#include <stdint.h>


class StorageRunner {
public:
    StorageRunner();
    ~StorageRunner();
    bool Run(int argc, const char* const argv[], volatile bool & running, bool useSignalHandler);
    std::size_t m_totalBundlesErasedFromStorage;
    std::size_t m_totalBundlesSentToEgressFromStorage;

private:
    void MonitorExitKeypressThreadFunction();

    volatile bool m_runningFromSigHandler;
};


#endif //_STORAGE_RUNNER_H

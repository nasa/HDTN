#ifndef _BUNDLE_STORAGE_MANAGER_MT_H
#define _BUNDLE_STORAGE_MANAGER_MT_H

#include "BundleStorageManagerBase.h"


class BundleStorageManagerMT : public BundleStorageManagerBase {
public:
    BundleStorageManagerMT();
    BundleStorageManagerMT(const std::string & jsonConfigFileName);
    BundleStorageManagerMT(const StorageConfig_ptr & storageConfigPtr);
    virtual ~BundleStorageManagerMT();
    virtual void Start();


private:
    void ThreadFunc(unsigned int threadIndex);
    virtual void NotifyDiskOfWorkToDo_ThreadSafe(const unsigned int diskId);
private:

    //boost::condition_variable m_conditionVariables[NUM_STORAGE_THREADS];
    //boost::shared_ptr<boost::thread> m_threadPtrs[NUM_STORAGE_THREADS];
    //CircularIndexBufferSingleProducerSingleConsumer m_circularIndexBuffers[NUM_STORAGE_THREADS];
    std::vector<boost::condition_variable> m_conditionVariablesVec;
    std::vector<std::unique_ptr<boost::thread> > m_threadPtrsVec;

    volatile bool m_running;
};


#endif //_BUNDLE_STORAGE_MANAGER_MT_H

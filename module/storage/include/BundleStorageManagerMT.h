#ifndef _BUNDLE_STORAGE_MANAGER_MT_H
#define _BUNDLE_STORAGE_MANAGER_MT_H

#include "BundleStorageManagerBase.h"


class CLASS_VISIBILITY_STORAGE_LIB BundleStorageManagerMT : public BundleStorageManagerBase {
public:
    STORAGE_LIB_EXPORT BundleStorageManagerMT();
    STORAGE_LIB_EXPORT BundleStorageManagerMT(const std::string & jsonConfigFileName);
    STORAGE_LIB_EXPORT BundleStorageManagerMT(const StorageConfig_ptr & storageConfigPtr);
    STORAGE_LIB_EXPORT virtual ~BundleStorageManagerMT();
    STORAGE_LIB_EXPORT virtual void Start();


private:
    STORAGE_LIB_NO_EXPORT void ThreadFunc(unsigned int threadIndex);
    STORAGE_LIB_NO_EXPORT virtual void NotifyDiskOfWorkToDo_ThreadSafe(const unsigned int diskId);
private:

    //boost::condition_variable m_conditionVariables[NUM_STORAGE_THREADS];
    //boost::shared_ptr<boost::thread> m_threadPtrs[NUM_STORAGE_THREADS];
    //CircularIndexBufferSingleProducerSingleConsumer m_circularIndexBuffers[NUM_STORAGE_THREADS];
    std::vector<boost::condition_variable> m_conditionVariablesVec;
    std::vector<std::unique_ptr<boost::thread> > m_threadPtrsVec;

    volatile bool m_running;
};


#endif //_BUNDLE_STORAGE_MANAGER_MT_H

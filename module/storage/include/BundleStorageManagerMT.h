/**
 * @file BundleStorageManagerMT.h
 * @author  Brian Tomko <brian.j.tomko@nasa.gov>
 *
 * @copyright Copyright © 2021 United States Government as represented by
 * the National Aeronautics and Space Administration.
 * No copyright is claimed in the United States under Title 17, U.S.Code.
 * All Other Rights Reserved.
 *
 * @section LICENSE
 * Released under the NASA Open Source Agreement (NOSA)
 * See LICENSE.md in the source root directory for more information.
 *
 * @section DESCRIPTION
 *
 * This BundleStorageManagerMT class inherits from the BundleStorageManagerBase class and implements
 * writing and reading bundles to and from solid state disk drive(s) using 1 thread per disk drive (i.e. 1 thread per storeFilePath)
 * and uses cross-platform blocking synchronous I/O operations from stdio.h such as fwrite.
 */

#ifndef _BUNDLE_STORAGE_MANAGER_MT_H
#define _BUNDLE_STORAGE_MANAGER_MT_H 1

#include "BundleStorageManagerBase.h"


class CLASS_VISIBILITY_STORAGE_LIB BundleStorageManagerMT : public BundleStorageManagerBase {
public:
    STORAGE_LIB_EXPORT BundleStorageManagerMT();
    STORAGE_LIB_EXPORT BundleStorageManagerMT(const boost::filesystem::path& jsonConfigFilePath);
    STORAGE_LIB_EXPORT BundleStorageManagerMT(const StorageConfig_ptr & storageConfigPtr);
    STORAGE_LIB_EXPORT virtual ~BundleStorageManagerMT();
    STORAGE_LIB_EXPORT virtual void Start();


private:
    STORAGE_LIB_NO_EXPORT void StopAllDiskThreads();
    STORAGE_LIB_NO_EXPORT void ThreadFunc(unsigned int threadIndex);
    STORAGE_LIB_NO_EXPORT virtual void CommitWriteAndNotifyDiskOfWorkToDo_ThreadSafe(const unsigned int diskId);
private:

    //boost::condition_variable m_conditionVariables[NUM_STORAGE_THREADS];
    //std::shared_ptr<boost::thread> m_threadPtrs[NUM_STORAGE_THREADS];
    //CircularIndexBufferSingleProducerSingleConsumer m_circularIndexBuffers[NUM_STORAGE_THREADS];
    std::vector<std::pair<boost::condition_variable, boost::mutex> > m_conditionVariablesPlusMutexesVec;
    std::vector<std::unique_ptr<boost::thread> > m_threadPtrsVec;

    volatile bool m_running;
    volatile bool m_noFatalErrorsOccurred;
};


#endif //_BUNDLE_STORAGE_MANAGER_MT_H

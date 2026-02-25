/**
 * @file BundleStorageManagerMT.h
 * @author  Brian Tomko <brian.j.tomko@nasa.gov>
 *
 * @copyright Copyright (c) 2026 United States Government as represented by
 * the National Aeronautics and Space Administration.
 * No copyright is claimed in the United States under Title 17, U.S.Code.
 * All Other Rights Reserved.
 *
 * @section LICENSE
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
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
#include <atomic>


class CLASS_VISIBILITY_STORAGE_LIB BundleStorageManagerMT : public BundleStorageManagerBase {
public:
    STORAGE_LIB_EXPORT BundleStorageManagerMT();
    STORAGE_LIB_EXPORT BundleStorageManagerMT(const boost::filesystem::path& jsonConfigFilePath);
    STORAGE_LIB_EXPORT BundleStorageManagerMT(const StorageConfig_ptr & storageConfigPtr);
    STORAGE_LIB_EXPORT virtual ~BundleStorageManagerMT() override;
    STORAGE_LIB_EXPORT virtual void Start() override;


private:
    STORAGE_LIB_NO_EXPORT void StopAllDiskThreads();
    STORAGE_LIB_NO_EXPORT void ThreadFunc(unsigned int threadIndex);
    STORAGE_LIB_NO_EXPORT virtual void CommitWriteAndNotifyDiskOfWorkToDo_ThreadSafe(const unsigned int diskId) override;
private:

    //boost::condition_variable m_conditionVariables[NUM_STORAGE_THREADS];
    //std::shared_ptr<boost::thread> m_threadPtrs[NUM_STORAGE_THREADS];
    //CircularIndexBufferSingleProducerSingleConsumer m_circularIndexBuffers[NUM_STORAGE_THREADS];
    std::vector<std::pair<boost::condition_variable, boost::mutex> > m_conditionVariablesPlusMutexesVec;
    std::vector<std::unique_ptr<boost::thread> > m_threadPtrsVec;

    std::atomic<bool> m_running;
    std::atomic<bool> m_noFatalErrorsOccurred;
};


#endif //_BUNDLE_STORAGE_MANAGER_MT_H

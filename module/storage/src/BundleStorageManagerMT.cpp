/**
 * @file BundleStorageManagerMT.cpp
 * @author  Brian Tomko <brian.j.tomko@nasa.gov>
 *
 * @copyright Copyright � 2021 United States Government as represented by
 * the National Aeronautics and Space Administration.
 * No copyright is claimed in the United States under Title 17, U.S.Code.
 * All Other Rights Reserved.
 *
 * @section LICENSE
 * Released under the NASA Open Source Agreement (NOSA)
 * See LICENSE.md in the source root directory for more information.
 */

#include "BundleStorageManagerMT.h"
#include <iostream>
#include <string>
#include <boost/filesystem.hpp>
#include <memory>
#include <boost/make_unique.hpp>


BundleStorageManagerMT::BundleStorageManagerMT() : BundleStorageManagerMT("storageConfig.json") {}

BundleStorageManagerMT::BundleStorageManagerMT(const std::string & jsonConfigFileName) : BundleStorageManagerMT(StorageConfig::CreateFromJsonFile(jsonConfigFileName)) {
    if (!m_storageConfigPtr) {
        LOG_ERROR(hdtn::Logger::Module::storage) << "cannot open storage json config file: " << jsonConfigFileName;
        return;
    }
}

BundleStorageManagerMT::BundleStorageManagerMT(const StorageConfig_ptr & storageConfigPtr) :
    BundleStorageManagerBase(storageConfigPtr),

    m_conditionVariablesVec(M_NUM_STORAGE_DISKS),
    m_threadPtrsVec(M_NUM_STORAGE_DISKS),
    m_running(false)
{

}

BundleStorageManagerMT::~BundleStorageManagerMT() {
    m_running = false; //thread stopping criteria
    for (unsigned int diskId = 0; diskId < M_NUM_STORAGE_DISKS; ++diskId) {
        if (m_threadPtrsVec[diskId]) {
            m_threadPtrsVec[diskId]->join();
            m_threadPtrsVec[diskId].reset(); //delete it
        }
    }

}

void BundleStorageManagerMT::Start() {
    if ((!m_running) && (m_storageConfigPtr)) {
        m_running = true;
        for (unsigned int diskId = 0; diskId < M_NUM_STORAGE_DISKS; ++diskId) {
            m_threadPtrsVec[diskId] = boost::make_unique<boost::thread>(
                boost::bind(&BundleStorageManagerMT::ThreadFunc, this, diskId)); //create and start the worker thread
        }
    }
}

void BundleStorageManagerMT::ThreadFunc(const unsigned int threadIndex) {

    boost::mutex localMutex;
    boost::mutex::scoped_lock lock(localMutex);
    boost::condition_variable & cv = m_conditionVariablesVec[threadIndex];
    CircularIndexBufferSingleProducerSingleConsumerConfigurable & cb = m_circularIndexBuffersVec[threadIndex];
    const char * const filePath = m_storageConfigPtr->m_storageDiskConfigVector[threadIndex].storeFilePath.c_str();
    LOG_INFO(hdtn::Logger::Module::storage) << ((m_successfullyRestoredFromDisk) ? "reopening " : "creating ") << filePath;
    if (m_successfullyRestoredFromDisk)
    {
        hdtn::Logger::getInstance()->logNotification("storage", "Reopening " + std::string(filePath));
    }
    else
    {
        hdtn::Logger::getInstance()->logNotification("storage", "Creating " + std::string(filePath));
    }
    FILE * fileHandle = (m_successfullyRestoredFromDisk) ? fopen(filePath, "r+bR") : fopen(filePath, "w+bR");
    boost::uint8_t * const circularBufferBlockDataPtr = &m_circularBufferBlockDataPtr[threadIndex * CIRCULAR_INDEX_BUFFER_SIZE * SEGMENT_SIZE];
    segment_id_t * const circularBufferSegmentIdsPtr = &m_circularBufferSegmentIdsPtr[threadIndex * CIRCULAR_INDEX_BUFFER_SIZE];

    while (m_running || (cb.GetIndexForRead() != CIRCULAR_INDEX_BUFFER_EMPTY)) { //keep thread alive if running or cb not empty


        const unsigned int consumeIndex = cb.GetIndexForRead(); //store the volatile

        if (consumeIndex == CIRCULAR_INDEX_BUFFER_EMPTY) { //if empty
            cv.timed_wait(lock, boost::posix_time::milliseconds(10)); // call lock.unlock() and blocks the current thread
            //thread is now unblocked, and the lock is reacquired by invoking lock.lock()
            continue;
        }

        boost::uint8_t * const data = &circularBufferBlockDataPtr[consumeIndex * SEGMENT_SIZE]; //expected data for testing when reading
        const segment_id_t segmentId = circularBufferSegmentIdsPtr[consumeIndex];
        volatile boost::uint8_t * const readFromStorageDestPointer = m_circularBufferReadFromStoragePointers[threadIndex * CIRCULAR_INDEX_BUFFER_SIZE + consumeIndex];
        volatile bool * const isReadCompletedPointer = m_circularBufferIsReadCompletedPointers[threadIndex * CIRCULAR_INDEX_BUFFER_SIZE + consumeIndex];
        const bool isWriteToDisk = (readFromStorageDestPointer == NULL);
        if (segmentId == SEGMENT_ID_LAST) {
            LOG_ERROR(hdtn::Logger::Module::storage) << "error segmentId is last";
            m_running = false;
            continue;
        }

        const boost::uint64_t offsetBytes = static_cast<boost::uint64_t>(segmentId / M_NUM_STORAGE_DISKS) * SEGMENT_SIZE;
#ifdef _MSC_VER 
        _fseeki64_nolock(fileHandle, offsetBytes, SEEK_SET);
#elif defined __APPLE__ 
        fseeko(fileHandle, offsetBytes, SEEK_SET);
#else
        fseeko64(fileHandle, offsetBytes, SEEK_SET);
#endif

        if (isWriteToDisk) {
            if (fwrite(data, 1, SEGMENT_SIZE, fileHandle) != SEGMENT_SIZE) {
                LOG_ERROR(hdtn::Logger::Module::storage) << "error writing";
            }
        }
        else { //read from disk
            if (fread((void*)readFromStorageDestPointer, 1, SEGMENT_SIZE, fileHandle) != SEGMENT_SIZE) {
                LOG_ERROR(hdtn::Logger::Module::storage) << "error reading";
            }
            *isReadCompletedPointer = true;
        }


        cb.CommitRead();
        m_conditionVariableMainThread.notify_one();
    }

    if (fileHandle) {
        fclose(fileHandle);
        fileHandle = NULL;
    }
}

//virtual function to be called immediately after a disk's circular buffer CommitWrite();
void BundleStorageManagerMT::NotifyDiskOfWorkToDo_ThreadSafe(const unsigned int diskId) {
    m_conditionVariablesVec[diskId].notify_one();
}

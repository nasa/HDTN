/**
 * @file BundleStorageManagerMT.cpp
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
 */

#include "BundleStorageManagerMT.h"
#include <string>
#include <boost/filesystem.hpp>
#include <memory>
#include <boost/make_unique.hpp>
#include "ThreadNamer.h"

static constexpr hdtn::Logger::SubProcess subprocess = hdtn::Logger::SubProcess::storage;

BundleStorageManagerMT::BundleStorageManagerMT() : BundleStorageManagerMT("storageConfig.json") {}

BundleStorageManagerMT::BundleStorageManagerMT(const boost::filesystem::path& jsonConfigFilePath) :
    BundleStorageManagerMT(StorageConfig::CreateFromJsonFilePath(jsonConfigFilePath)) {
    if (!m_storageConfigPtr) {
        LOG_ERROR(subprocess) << "cannot open storage json config file: " << jsonConfigFilePath;
        return;
    }
}

BundleStorageManagerMT::BundleStorageManagerMT(const StorageConfig_ptr & storageConfigPtr) :
    BundleStorageManagerBase(storageConfigPtr),

    m_conditionVariablesPlusMutexesVec(M_NUM_STORAGE_DISKS),
    m_threadPtrsVec(M_NUM_STORAGE_DISKS),
    m_running(false),
    m_noFatalErrorsOccurred(true)
{

}

void BundleStorageManagerMT::StopAllDiskThreads() {
    m_running = false; //thread stopping criteria
    for (unsigned int diskId = 0; diskId < M_NUM_STORAGE_DISKS; ++diskId) { //only lock one mutex at a time to prevent deadlock (a worker may call this function on an error condition)
        //lock then unlock each thread's mutex to prevent a missed notify after setting thread stopping criteria above
        m_conditionVariablesPlusMutexesVec[diskId].second.lock();
        m_conditionVariablesPlusMutexesVec[diskId].second.unlock();
        m_conditionVariablesPlusMutexesVec[diskId].first.notify_one();
    }
}

BundleStorageManagerMT::~BundleStorageManagerMT() {
    StopAllDiskThreads();
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
        m_noFatalErrorsOccurred = true;
        for (unsigned int diskId = 0; diskId < M_NUM_STORAGE_DISKS; ++diskId) {
            m_threadPtrsVec[diskId] = boost::make_unique<boost::thread>(
                boost::bind(&BundleStorageManagerMT::ThreadFunc, this, diskId)); //create and start the worker thread
        }
    }
}

void BundleStorageManagerMT::ThreadFunc(const unsigned int threadIndex) {
    const std::string threadName = "StorageMTdisk" + boost::lexical_cast<std::string>(threadIndex);
    ThreadNamer::SetThisThreadName(threadName);
    
    //boost::mutex::scoped_lock lock(localMutex);
    std::pair<boost::condition_variable, boost::mutex>& cvMutexPairRef = m_conditionVariablesPlusMutexesVec[threadIndex];
    boost::condition_variable & cv = cvMutexPairRef.first;
    boost::mutex & localMutex = cvMutexPairRef.second;
    CircularIndexBufferSingleProducerSingleConsumerConfigurable & cb = m_circularIndexBuffersVec[threadIndex];
    //const char * const filePath = m_storageConfigPtr->m_storageDiskConfigVector[threadIndex].storeFilePath.c_str();
    const boost::filesystem::path& filePath = m_filePathsVec[threadIndex];
    const boost::filesystem::path::value_type* filePathCstr = filePath.c_str();
    LOG_INFO(subprocess) << ((m_successfullyRestoredFromDisk) ? "reopening " : "creating ") << filePath;
    FILE * fileHandle = (m_successfullyRestoredFromDisk) ? 
#ifdef _WIN32
        _wfopen(filePathCstr, L"r+bR") : _wfopen(filePathCstr, L"w+bR");
#else
        fopen(filePathCstr, "r+bR") : fopen(filePathCstr, "w+bR");
#endif // _WIN32

        
    boost::uint8_t * const circularBufferBlockDataPtr = &m_circularBufferBlockDataPtr[threadIndex * CIRCULAR_INDEX_BUFFER_SIZE * SEGMENT_SIZE];
    segment_id_t * const circularBufferSegmentIdsPtr = &m_circularBufferSegmentIdsPtr[threadIndex * CIRCULAR_INDEX_BUFFER_SIZE];

    while (m_noFatalErrorsOccurred) { //keep thread alive if running or cb not empty, i.e. "while (m_running || (m_circularIndexBuffer.GetIndexForRead() != CIRCULAR_INDEX_BUFFER_EMPTY))"
        unsigned int consumeIndex = cb.GetIndexForRead(); //store the volatile
        if (consumeIndex == CIRCULAR_INDEX_BUFFER_EMPTY) { //if empty
            //try again, but with the mutex
            boost::mutex::scoped_lock lock(localMutex);
            consumeIndex = cb.GetIndexForRead(); //store the volatile
            if (consumeIndex == CIRCULAR_INDEX_BUFFER_EMPTY) { //if empty again (lock mutex (above) before checking condition)
                if (!m_running) { //m_running is mutex protected, if it stopped running, exit the thread (lock mutex (above) before checking condition)
                    break; //thread stopping criteria (empty and not running)
                }
                cv.wait(lock); // call lock.unlock() and blocks the current thread
                //thread is now unblocked, and the lock is reacquired by invoking lock.lock()
                continue;
            }
        }

        boost::uint8_t * const data = &circularBufferBlockDataPtr[consumeIndex * SEGMENT_SIZE]; //expected data for testing when reading
        const segment_id_t segmentId = circularBufferSegmentIdsPtr[consumeIndex];
        volatile boost::uint8_t * const readFromStorageDestPointer = m_circularBufferReadFromStoragePointers[threadIndex * CIRCULAR_INDEX_BUFFER_SIZE + consumeIndex];
        const bool isWriteToDisk = (readFromStorageDestPointer == NULL);
        volatile bool junk;
        volatile bool * const isReadCompletedPointer = (isWriteToDisk) ?
            &junk : m_circularBufferIsReadCompletedPointers[threadIndex * CIRCULAR_INDEX_BUFFER_SIZE + consumeIndex];
        if (segmentId == SEGMENT_ID_LAST) {
            LOG_ERROR(subprocess) << "error segmentId is last";
            m_noFatalErrorsOccurred = false; //a fatal error occurred
            StopAllDiskThreads(); //sets m_running = false;
            break;
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
                LOG_ERROR(subprocess) << "error writing";
            }
        }
        else { //read from disk
            if (fread((void*)readFromStorageDestPointer, 1, SEGMENT_SIZE, fileHandle) != SEGMENT_SIZE) {
                LOG_ERROR(subprocess) << "error reading";
            }
        }

        m_mutexMainThread.lock();
        *isReadCompletedPointer = true;
        cb.CommitRead();
        m_mutexMainThread.unlock();
        m_conditionVariableMainThread.notify_one();
    }

    if (fileHandle) {
        fclose(fileHandle);
        fileHandle = NULL;
    }
}

//virtual function to be called immediately after a disk's circular buffer CommitWrite();
void BundleStorageManagerMT::CommitWriteAndNotifyDiskOfWorkToDo_ThreadSafe(const unsigned int diskId) {
    CircularIndexBufferSingleProducerSingleConsumerConfigurable& cb = m_circularIndexBuffersVec[diskId];
    std::pair<boost::condition_variable, boost::mutex>& cvMutexPairRef = m_conditionVariablesPlusMutexesVec[diskId];
    boost::condition_variable& cv = cvMutexPairRef.first;
    boost::mutex& cvMutex = cvMutexPairRef.second;

    cvMutex.lock();
    cb.CommitWrite();
    cvMutex.unlock();
    cv.notify_one();
}

#include "BundleStorageManagerMT.h"
#include <iostream>
#include <string>
#include <boost/filesystem.hpp>
#include <boost/random/mersenne_twister.hpp>
#include <boost/random/uniform_int_distribution.hpp>
#include <boost/timer/timer.hpp>
#include <boost/make_shared.hpp>
#include "SignalHandler.h"

#ifdef _MSC_VER //Windows tests
static const char * FILE_PATHS[NUM_STORAGE_THREADS] = { "map0.bin", "map1.bin", "map2.bin", "map3.bin" };
#else //Lambda Linux tests
//static const char * FILE_PATHS[NUM_STORAGE_THREADS] = { "/mnt/sda1/test/map0.bin", "/mnt/sdb1/test/map1.bin", "/mnt/sdc1/test/map2.bin", "/mnt/sdd1/test/map3.bin" };
static const char * FILE_PATHS[NUM_STORAGE_THREADS] = { "/home/hdtn/hdtn.store/map0.bin", "/home/hdtn/hdtn.store/map1.bin", "/home/hdtn/hdtn.store/map2.bin", "/home/hdtn/hdtn.store/map3.bin" };
#endif



BundleStorageManagerMT::BundleStorageManagerMT() : m_lockMainThread(m_mutexMainThread), m_running(false), m_successfullyRestoredFromDisk(false), m_autoDeleteFilesOnExit(true) {

	m_circularBufferBlockDataPtr = (uint8_t*) malloc(CIRCULAR_INDEX_BUFFER_SIZE * NUM_STORAGE_THREADS * SEGMENT_SIZE * sizeof(uint8_t));
	m_circularBufferSegmentIdsPtr = (segment_id_t*) malloc(CIRCULAR_INDEX_BUFFER_SIZE * NUM_STORAGE_THREADS * sizeof(segment_id_t));
}

BundleStorageManagerMT::~BundleStorageManagerMT() {
	m_running = false; //thread stopping criteria
	for (unsigned int tId = 0; tId < NUM_STORAGE_THREADS; ++tId) {
		if (m_threadPtrs[tId]) {
			m_threadPtrs[tId]->join();
		}
		m_threadPtrs[tId] = boost::shared_ptr<boost::thread>();
	}

	free(m_circularBufferBlockDataPtr);
	free(m_circularBufferSegmentIdsPtr);
	
}

void BundleStorageManagerMT::Start(bool autoDeleteFilesOnExit) {
	if (!m_running) {
		m_autoDeleteFilesOnExit = autoDeleteFilesOnExit;
		m_running = true;
		for (unsigned int tId = 0; tId < NUM_STORAGE_THREADS; ++tId) {
			m_threadPtrs[tId] = boost::make_shared<boost::thread>(
				boost::bind(&BundleStorageManagerMT::ThreadFunc, this, tId)); //create and start the worker thread
		}
	}
}

void BundleStorageManagerMT::ThreadFunc(const unsigned int threadIndex) {

	boost::mutex localMutex;
	boost::mutex::scoped_lock lock(localMutex);
	boost::condition_variable & cv = m_conditionVariables[threadIndex];
	CircularIndexBufferSingleProducerSingleConsumer & cb = m_circularIndexBuffers[threadIndex];
	const char * const filePath = FILE_PATHS[threadIndex];
	FILE * fileHandle = (m_successfullyRestoredFromDisk) ? fopen(filePath, "r+bR") : fopen(filePath, "w+bR");
	boost::uint8_t * const circularBufferBlockDataPtr = &m_circularBufferBlockDataPtr[threadIndex * CIRCULAR_INDEX_BUFFER_SIZE * SEGMENT_SIZE];
	segment_id_t * const circularBufferSegmentIdsPtr = &m_circularBufferSegmentIdsPtr[threadIndex * CIRCULAR_INDEX_BUFFER_SIZE];
	
	while (m_running || (cb.GetIndexForRead() != UINT32_MAX)) { //keep thread alive if running or cb not empty
		
			
		const unsigned int consumeIndex = cb.GetIndexForRead(); //store the volatile
			
		if (consumeIndex == UINT32_MAX) { //if empty
			cv.timed_wait(lock, boost::posix_time::milliseconds(10)); // call lock.unlock() and blocks the current thread
			//thread is now unblocked, and the lock is reacquired by invoking lock.lock()
			continue;
		}

		boost::uint8_t * const data = &circularBufferBlockDataPtr[consumeIndex * SEGMENT_SIZE]; //expected data for testing when reading
		const segment_id_t segmentId = circularBufferSegmentIdsPtr[consumeIndex];
		volatile boost::uint8_t * const readFromStorageDestPointer = m_circularBufferReadFromStoragePointers[threadIndex * CIRCULAR_INDEX_BUFFER_SIZE + consumeIndex];
		volatile bool * const isReadCompletedPointer = m_circularBufferIsReadCompletedPointers[threadIndex * CIRCULAR_INDEX_BUFFER_SIZE + consumeIndex];
		const bool isWriteToDisk = (readFromStorageDestPointer == NULL);
		if (segmentId == UINT32_MAX) {
			std::cout << "error segmentId is max\n";
			m_running = false;
			continue;
		}
		
		const boost::uint64_t offsetBytes = static_cast<boost::uint64_t>(segmentId / NUM_STORAGE_THREADS) * SEGMENT_SIZE;
#ifdef _MSC_VER 
		_fseeki64_nolock(fileHandle, offsetBytes, SEEK_SET);
#else
		fseeko64(fileHandle, offsetBytes, SEEK_SET);
#endif

		if (isWriteToDisk) {
			if (fwrite(data, 1, SEGMENT_SIZE, fileHandle) != SEGMENT_SIZE) {
				std::cout << "error writing\n";
			}
		}
		else { //read from disk
			if (fread((void*)readFromStorageDestPointer, 1, SEGMENT_SIZE, fileHandle) != SEGMENT_SIZE) {
				std::cout << "error reading\n";
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

	const boost::filesystem::path p(filePath);

	if (m_autoDeleteFilesOnExit && boost::filesystem::exists(p)) {
		boost::filesystem::remove(p);
		std::cout << "deleted " << p.string() << "\n";
	}

}

const MemoryManagerTreeArray & BundleStorageManagerMT::GetMemoryManagerConstRef() {
	return m_memoryManager;
}

void BundleStorageManagerMT::AddLink(boost::uint64_t linkName) {
	m_destMap[linkName] = priority_vec_t(NUMBER_OF_PRIORITIES);
}

boost::uint64_t BundleStorageManagerMT::Push(BundleStorageManagerSession_WriteToDisk & session, bp_primary_if_base_t & bundleMetaData) {
	chain_info_t & chainInfo = session.chainInfo;
	segment_id_chain_vec_t & segmentIdChainVec = chainInfo.second;
	const boost::uint64_t bundleSizeBytes = bundleMetaData.length;
	const boost::uint64_t totalSegmentsRequired = (bundleSizeBytes / BUNDLE_STORAGE_PER_SEGMENT_SIZE) + ((bundleSizeBytes % BUNDLE_STORAGE_PER_SEGMENT_SIZE) == 0 ? 0 : 1);

	chainInfo.first = bundleSizeBytes;
	segmentIdChainVec.resize(totalSegmentsRequired);
	session.nextLogicalSegment = 0;

	session.destLinkId = bundleMetaData.dst_node;
	//The bits in positions 8 and 7 constitute a two-bit priority field indicating the bundle's priority, with higher values
	//being of higher priority : 00 = bulk, 01 = normal, 10 = expedited, 11 is reserved for future use.
	session.priorityIndex = (bundleMetaData.flags >> 7) & 3;
	session.absExpiration = bundleMetaData.creation + bundleMetaData.lifetime;

	if (m_memoryManager.AllocateSegments_ThreadSafe(segmentIdChainVec)) {
		//std::cout << "firstseg " << segmentIdChainVec[0] << "\n";
		return totalSegmentsRequired;
	}
	
	return 0;
}

int BundleStorageManagerMT::PushSegment(BundleStorageManagerSession_WriteToDisk & session, void * buf, std::size_t size) {
	chain_info_t & chainInfo = session.chainInfo;
	segment_id_chain_vec_t & segmentIdChainVec = chainInfo.second;

	if (session.nextLogicalSegment >= segmentIdChainVec.size()) {
		return 0;
	}
	const boost::uint64_t bundleSizeBytes = (session.nextLogicalSegment == 0) ? chainInfo.first : UINT64_MAX;
	const segment_id_t segmentId = segmentIdChainVec[session.nextLogicalSegment++];
	const unsigned int threadIndex = segmentId % NUM_STORAGE_THREADS;
	CircularIndexBufferSingleProducerSingleConsumer & cb = m_circularIndexBuffers[threadIndex];
	boost::condition_variable & cv = m_conditionVariables[threadIndex];
	unsigned int produceIndex = cb.GetIndexForWrite();
	while (produceIndex == UINT32_MAX) { //store the volatile, wait until not full				
		m_conditionVariableMainThread.timed_wait(m_lockMainThread, boost::posix_time::milliseconds(10)); // call lock.unlock() and blocks the current thread
		//thread is now unblocked, and the lock is reacquired by invoking lock.lock()	
		produceIndex = cb.GetIndexForWrite();
	}

	boost::uint8_t * const circularBufferBlockDataPtr = &m_circularBufferBlockDataPtr[threadIndex * CIRCULAR_INDEX_BUFFER_SIZE * SEGMENT_SIZE];
	segment_id_t * const circularBufferSegmentIdsPtr = &m_circularBufferSegmentIdsPtr[threadIndex * CIRCULAR_INDEX_BUFFER_SIZE];
	

	boost::uint8_t * const dataCb = &circularBufferBlockDataPtr[produceIndex * SEGMENT_SIZE];
	circularBufferSegmentIdsPtr[produceIndex] = segmentId;
	m_circularBufferReadFromStoragePointers[threadIndex * CIRCULAR_INDEX_BUFFER_SIZE + produceIndex] = NULL; //isWriteToDisk = true

	const segment_id_t nextSegmentId = (session.nextLogicalSegment == segmentIdChainVec.size()) ? UINT32_MAX : segmentIdChainVec[session.nextLogicalSegment];
	memcpy(dataCb, &bundleSizeBytes, sizeof(bundleSizeBytes));
	memcpy(dataCb + sizeof(boost::uint64_t), &nextSegmentId, sizeof(nextSegmentId));
	memcpy(dataCb + SEGMENT_RESERVED_SPACE, buf, size);

	cb.CommitWrite();
	cv.notify_one();
	//std::cout << "writing " << size << " bytes\n";
	if (session.nextLogicalSegment == segmentIdChainVec.size()) {	
		if (m_destMap.count(session.destLinkId) == 0) {
			AddLink(session.destLinkId);
		}
		priority_vec_t & priorityVec = m_destMap[session.destLinkId];
		expiration_map_t & expirationMap = priorityVec[session.priorityIndex];
		chain_info_vec_t & chainInfoVec = expirationMap[session.absExpiration];
		chainInfoVec.push_back(std::move(chainInfo));
		//std::cout << "write complete\n";
	}

	return 1;
}


uint64_t BundleStorageManagerMT::PopTop(BundleStorageManagerSession_ReadFromDisk & session, const std::vector<uint64_t> & availableDestLinks) { //0 if empty, size if entry
	std::vector<priority_vec_t *> priorityVecPtrs;
	priorityVecPtrs.reserve(availableDestLinks.size());
	for (std::size_t i = 0; i < availableDestLinks.size(); ++i) {
		if (m_destMap.count(availableDestLinks[i]) > 0) {
			priorityVecPtrs.push_back(&m_destMap[availableDestLinks[i]]);
		}
	}
	session.nextLogicalSegment = 0;
	session.nextLogicalSegmentToCache = 0;
	session.cacheReadIndex = 0;
	session.cacheWriteIndex = 0;
	
	//memset((uint8_t*)session.readCacheIsSegmentReady, 0, READ_CACHE_NUM_SEGMENTS_PER_SESSION);
	for (int i = NUMBER_OF_PRIORITIES-1; i >=0; --i) { //00 = bulk, 01 = normal, 10 = expedited
		abs_expiration_t lowestExpiration = UINT64_MAX;
		session.expirationMapPtr = NULL;
		session.chainInfoVecPtr = NULL;
		
		for (std::size_t j = 0; j < priorityVecPtrs.size(); ++j) {
			priority_vec_t * priorityVec = priorityVecPtrs[j];
			//std::cout << "size " << (*priorityVec).size() << "\n";
			expiration_map_t & expirationMap = (*priorityVec)[i];
			expiration_map_t::iterator it = expirationMap.begin();
			if (it != expirationMap.end()) {
				const abs_expiration_t thisExpiration = it->first;
				//std::cout << "thisexp " << thisExpiration << "\n";
				if (lowestExpiration > thisExpiration) {
					lowestExpiration = thisExpiration;
					//linkIndex = j;
					session.expirationMapPtr = &expirationMap;
					session.chainInfoVecPtr = &it->second;
					session.expirationMapIterator = it;
					session.absExpiration = it->first;
				}
			}
		}
		if (session.chainInfoVecPtr) {
			//have session take custody of data structure
			session.chainInfo = std::move(session.chainInfoVecPtr->front());
			session.chainInfoVecPtr->erase(session.chainInfoVecPtr->begin());

			if (session.chainInfoVecPtr->size() == 0) {
				session.expirationMapPtr->erase(session.expirationMapIterator);
			}
			else {
				////segmentIdVecPtr->shrink_to_fit();
			}
			return session.chainInfo.first; //use the front as new writes will be pushed back
			
		}

	}
	return 0;
}

bool BundleStorageManagerMT::ReturnTop(BundleStorageManagerSession_ReadFromDisk & session) { //0 if empty, size if entry
	(*session.expirationMapPtr)[session.absExpiration].push_back(std::move(session.chainInfo));	
	return true;
}

std::size_t BundleStorageManagerMT::TopSegment(BundleStorageManagerSession_ReadFromDisk & session, void * buf) {
	segment_id_chain_vec_t & segments = session.chainInfo.second;

	while ( ((session.nextLogicalSegmentToCache - session.nextLogicalSegment) < READ_CACHE_NUM_SEGMENTS_PER_SESSION) 
		&& (session.nextLogicalSegmentToCache < segments.size()) )
	{
		const segment_id_t segmentId = segments[session.nextLogicalSegmentToCache++];
		const unsigned int threadIndex = segmentId % NUM_STORAGE_THREADS;
		CircularIndexBufferSingleProducerSingleConsumer & cb = m_circularIndexBuffers[threadIndex];
		boost::condition_variable & cv = m_conditionVariables[threadIndex];
		unsigned int produceIndex = cb.GetIndexForWrite();
		while (produceIndex == UINT32_MAX) { //store the volatile, wait until not full				
			m_conditionVariableMainThread.timed_wait(m_lockMainThread, boost::posix_time::milliseconds(10)); // call lock.unlock() and blocks the current thread
			//thread is now unblocked, and the lock is reacquired by invoking lock.lock()	
			produceIndex = cb.GetIndexForWrite();
		}

		session.readCacheIsSegmentReady[session.cacheWriteIndex] = false;
		m_circularBufferIsReadCompletedPointers[threadIndex * CIRCULAR_INDEX_BUFFER_SIZE + produceIndex] = &session.readCacheIsSegmentReady[session.cacheWriteIndex];
		m_circularBufferReadFromStoragePointers[threadIndex * CIRCULAR_INDEX_BUFFER_SIZE + produceIndex] = &session.readCache[session.cacheWriteIndex * SEGMENT_SIZE];
		session.cacheWriteIndex = (session.cacheWriteIndex + 1) % READ_CACHE_NUM_SEGMENTS_PER_SESSION;
		m_circularBufferSegmentIdsPtr[threadIndex * CIRCULAR_INDEX_BUFFER_SIZE + produceIndex] = segmentId;

		cb.CommitWrite();
		cv.notify_one();
	}

	bool readIsReady = session.readCacheIsSegmentReady[session.cacheReadIndex];
	while (!readIsReady) { //store the volatile, wait until not full				
		m_conditionVariableMainThread.timed_wait(m_lockMainThread, boost::posix_time::milliseconds(10)); // call lock.unlock() and blocks the current thread
		//thread is now unblocked, and the lock is reacquired by invoking lock.lock()	
		readIsReady = session.readCacheIsSegmentReady[session.cacheReadIndex];
	}

	boost::uint64_t bundleSizeBytes;
	memcpy(&bundleSizeBytes, (void*)&session.readCache[session.cacheReadIndex * SEGMENT_SIZE + 0], sizeof(bundleSizeBytes));
	if (session.nextLogicalSegment == 0  && bundleSizeBytes != session.chainInfo.first) {// ? chainInfo.first : UINT64_MAX;
		std::cout << "error: read bundle size bytes = " << bundleSizeBytes << " does not match chainInfo = " << session.chainInfo.first << "\n";
	}
	else if (session.nextLogicalSegment != 0 && bundleSizeBytes != UINT64_MAX) {// ? chainInfo.first : UINT64_MAX;
		std::cout << "error: read bundle size bytes = " << bundleSizeBytes << " is not UINT64_MAX\n";
	}

	segment_id_t nextSegmentId;
	++session.nextLogicalSegment;
	memcpy(&nextSegmentId, (void*)&session.readCache[session.cacheReadIndex * SEGMENT_SIZE + sizeof(bundleSizeBytes)], sizeof(nextSegmentId));
	if (session.nextLogicalSegment != session.chainInfo.second.size() && nextSegmentId != session.chainInfo.second[session.nextLogicalSegment]) {
		std::cout << "error: read nextSegmentId = " << nextSegmentId << " does not match chainInfo = " << session.chainInfo.second[session.nextLogicalSegment] << "\n";
	}
	else if (session.nextLogicalSegment == session.chainInfo.second.size() && nextSegmentId != UINT32_MAX) {
		std::cout << "error: read nextSegmentId = " << nextSegmentId << " is not UINT32_MAX\n";
	}

	std::size_t size = BUNDLE_STORAGE_PER_SEGMENT_SIZE;
	if (nextSegmentId == UINT32_MAX) {
		boost::uint64_t modBytes = (session.chainInfo.first % BUNDLE_STORAGE_PER_SEGMENT_SIZE);
		if (modBytes != 0) {
			size = modBytes;
		}
	}

	memcpy(buf, (void*)&session.readCache[session.cacheReadIndex * SEGMENT_SIZE + SEGMENT_RESERVED_SPACE], size);
	session.cacheReadIndex = (session.cacheReadIndex + 1) % READ_CACHE_NUM_SEGMENTS_PER_SESSION;
	

	return size;
}
bool BundleStorageManagerMT::RemoveReadBundleFromDisk(BundleStorageManagerSession_ReadFromDisk & session, bool forceRemove) {
	if (!forceRemove && (session.nextLogicalSegment != session.chainInfo.second.size())) {
		std::cout << "error: bundle not yet read prior to removal\n";
		return false;
	}

	
	//destroy the head on the disk by writing UINT64_MAX to bundleSizeBytes of first logical segment
	chain_info_t & chainInfo = session.chainInfo;
	segment_id_chain_vec_t & segmentIdChainVec = chainInfo.second;

		
	const boost::uint64_t bundleSizeBytes = UINT64_MAX;
	const segment_id_t segmentId = segmentIdChainVec[0];
	const unsigned int threadIndex = segmentId % NUM_STORAGE_THREADS;
	CircularIndexBufferSingleProducerSingleConsumer & cb = m_circularIndexBuffers[threadIndex];
	boost::condition_variable & cv = m_conditionVariables[threadIndex];
	unsigned int produceIndex = cb.GetIndexForWrite();
	while (produceIndex == UINT32_MAX) { //store the volatile, wait until not full				
		m_conditionVariableMainThread.timed_wait(m_lockMainThread, boost::posix_time::milliseconds(10)); // call lock.unlock() and blocks the current thread
		//thread is now unblocked, and the lock is reacquired by invoking lock.lock()	
		produceIndex = cb.GetIndexForWrite();
	}

	boost::uint8_t * const circularBufferBlockDataPtr = &m_circularBufferBlockDataPtr[threadIndex * CIRCULAR_INDEX_BUFFER_SIZE * SEGMENT_SIZE];
	segment_id_t * const circularBufferSegmentIdsPtr = &m_circularBufferSegmentIdsPtr[threadIndex * CIRCULAR_INDEX_BUFFER_SIZE];


	boost::uint8_t * const dataCb = &circularBufferBlockDataPtr[produceIndex * SEGMENT_SIZE];
	circularBufferSegmentIdsPtr[produceIndex] = segmentId;
	m_circularBufferReadFromStoragePointers[threadIndex * CIRCULAR_INDEX_BUFFER_SIZE + produceIndex] = NULL; //isWriteToDisk = true

	memcpy(dataCb, &bundleSizeBytes, sizeof(bundleSizeBytes));
		

	cb.CommitWrite();
	cv.notify_one();
	
	return m_memoryManager.FreeSegments_ThreadSafe(segmentIdChainVec);
}
//uint64_t BundleStorageManagerMT::TopSegmentCount(BundleStorageManagerSession_ReadFromDisk & session) {
//	return session.chainInfoVecPtr->front().second.size(); //use the front as new writes will be pushed back
//}

bool BundleStorageManagerMT::RestoreFromDisk(uint64_t * totalBundlesRestored, uint64_t * totalBytesRestored, uint64_t * totalSegmentsRestored) {
	*totalBundlesRestored = 0; *totalBytesRestored = 0; *totalSegmentsRestored = 0;
	boost::uint8_t dataReadBuf[SEGMENT_SIZE];
	FILE * fileHandles[NUM_STORAGE_THREADS];
	boost::uint64_t fileSizes[NUM_STORAGE_THREADS];
	for (unsigned int tId = 0; tId < NUM_STORAGE_THREADS; ++tId) {
		const char * const filePath = FILE_PATHS[tId];
		const boost::filesystem::path p(filePath);
		if (boost::filesystem::exists(p)) {
			fileSizes[tId] = boost::filesystem::file_size(p);
			std::cout << "thread " << tId << " has file size of " << fileSizes[tId] << "\n";
		}
		else {
			std::cout << "error: " << filePath << " does not exist\n";
			return false;
		}
		fileHandles[tId] = fopen(filePath, "rbR");
		if (fileHandles[tId] == NULL) {
			std::cout << "error opening file " << filePath << " for reading and restoring\n";
			return false;
		}
	}

	bool restoreInProgress = true;
	for (segment_id_t potentialHeadSegmentId = 0; restoreInProgress; ++potentialHeadSegmentId) {
		if (!m_memoryManager.IsSegmentFree(potentialHeadSegmentId)) continue;
		segment_id_t segmentId = potentialHeadSegmentId;
		BundleStorageManagerSession_WriteToDisk session;
		chain_info_t & chainInfo = session.chainInfo;
		segment_id_chain_vec_t & segmentIdChainVec = chainInfo.second;
		bool headSegmentFound = false;
		for (session.nextLogicalSegment = 0; ; ++session.nextLogicalSegment) {
			const unsigned int threadIndex = segmentId % NUM_STORAGE_THREADS;
			FILE * const fileHandle = fileHandles[threadIndex];
			const boost::uint64_t offsetBytes = static_cast<boost::uint64_t>(segmentId / NUM_STORAGE_THREADS) * SEGMENT_SIZE;
			const boost::uint64_t fileSize = fileSizes[threadIndex];
			if ((session.nextLogicalSegment == 0) && ((offsetBytes + SEGMENT_SIZE) > fileSize)) {
				std::cout << "end of restore\n";
				restoreInProgress = false;
				break;
			}
#ifdef _MSC_VER 
			_fseeki64_nolock(fileHandle, offsetBytes, SEEK_SET);
#else
			fseeko64(fileHandle, offsetBytes, SEEK_SET);
#endif

			const std::size_t bytesReadFromFread = fread((void*)dataReadBuf, 1, SEGMENT_SIZE, fileHandle);
			if (bytesReadFromFread != SEGMENT_SIZE) {
				std::cout << "error reading at offset " << offsetBytes << " for thread " << threadIndex << " filesize " << fileSize << " logical segment " << session.nextLogicalSegment << " bytesread " << bytesReadFromFread << "\n";
				return false;
			}

			boost::uint64_t bundleSizeBytes; // = (session.nextLogicalSegment == 0) ? chainInfo.first : UINT64_MAX;
			segment_id_t nextSegmentId; // = (session.nextLogicalSegment == segmentIdChainVec.size()) ? UINT32_MAX : segmentIdChainVec[session.nextLogicalSegment];

			memcpy(&bundleSizeBytes, dataReadBuf, sizeof(bundleSizeBytes));
			memcpy(&nextSegmentId, dataReadBuf + sizeof(boost::uint64_t), sizeof(nextSegmentId));

			if ((session.nextLogicalSegment == 0) && (bundleSizeBytes != UINT64_MAX)) { //head segment
				headSegmentFound = true;
				
				//copy bundle header and store to maps, push segmentId to chain vec
				bp_primary_if_base_t bundleMetaData;
				memcpy(&bundleMetaData, dataReadBuf + SEGMENT_RESERVED_SPACE, sizeof(bundleMetaData));
				
				
				const boost::uint64_t totalSegmentsRequired = (bundleSizeBytes / BUNDLE_STORAGE_PER_SEGMENT_SIZE) + ((bundleSizeBytes % BUNDLE_STORAGE_PER_SEGMENT_SIZE) == 0 ? 0 : 1);

				if (bundleSizeBytes != bundleMetaData.length) {
					std::cout << "error: bundleSizeBytes != bundleMetaData.length\n";
					return false;
				}
				//std::cout << "tot segs req " << totalSegmentsRequired << "\n";
				*totalBytesRestored += bundleSizeBytes;
				*totalSegmentsRestored += totalSegmentsRequired;
				chainInfo.first = bundleMetaData.length; //bundleSizeBytes;
				segmentIdChainVec.resize(totalSegmentsRequired);

				session.destLinkId = bundleMetaData.dst_node;
				//The bits in positions 8 and 7 constitute a two-bit priority field indicating the bundle's priority, with higher values
				//being of higher priority : 00 = bulk, 01 = normal, 10 = expedited, 11 is reserved for future use.
				session.priorityIndex = (bundleMetaData.flags >> 7) & 3;
				session.absExpiration = bundleMetaData.creation + bundleMetaData.lifetime;

				
			}
			if (!headSegmentFound) break;
			if ((session.nextLogicalSegment) >= segmentIdChainVec.size()) {
				std::cout << "error: logical segment exceeds total segments required\n";
				return false;
			}
			if (!m_memoryManager.IsSegmentFree(segmentId)) {
				std::cout << "error: segmentId is already allocated\n";
				return false;
			}
			m_memoryManager.AllocateSegmentId_NoCheck_NotThreadSafe(segmentId);
			segmentIdChainVec[session.nextLogicalSegment] = segmentId;

			

			if ((session.nextLogicalSegment+1) >= segmentIdChainVec.size()) { //==
				if (nextSegmentId != UINT32_MAX) { //there are more segments
					std::cout << "error: at the last logical segment but nextSegmentId != UINT32_MAX\n";
					return false;
				}
				if (m_destMap.count(session.destLinkId) == 0) {
					AddLink(session.destLinkId);
				}
				priority_vec_t & priorityVec = m_destMap[session.destLinkId];
				expiration_map_t & expirationMap = priorityVec[session.priorityIndex];
				chain_info_vec_t & chainInfoVec = expirationMap[session.absExpiration];
				chainInfoVec.push_back(std::move(chainInfo));
				*totalBundlesRestored += 1;
				break;
				//std::cout << "write complete\n";
			}
			
			if (nextSegmentId == UINT32_MAX) { //there are more segments
				std::cout << "error: there are more logical segments but nextSegmentId == UINT32_MAX\n";
				return false;				
			}
			segmentId = nextSegmentId;
			
		}
	}


	for (unsigned int tId = 0; tId < NUM_STORAGE_THREADS; ++tId) {
		fclose(fileHandles[tId]);
	}

	m_successfullyRestoredFromDisk = true;
	return true;
}


static volatile bool g_running = true;

void MonitorExitKeypressThreadFunction() {
	std::cout << "Keyboard Interrupt.. exiting\n";
	g_running = false; //do this first
}

static SignalHandler g_sigHandler(boost::bind(&MonitorExitKeypressThreadFunction));



struct TestFile {
	TestFile() {}
	TestFile(boost::uint64_t size) : m_data(size) {
		boost::random::mt19937 gen(static_cast<unsigned int>(std::time(0)));
		const boost::random::uniform_int_distribution<> distRandomData(0, 255);
		for (std::size_t i = 0; i < size; ++i) {
			m_data[i] = distRandomData(gen);
		}
	}
	std::vector<boost::uint8_t> m_data;
};

bool BundleStorageManagerMT::TestSpeed() {
	boost::random::mt19937 gen(static_cast<unsigned int>(std::time(0)));
	const boost::random::uniform_int_distribution<> distLinkId(0, 9);
	const boost::random::uniform_int_distribution<> distFileId(0, 9);
	const boost::random::uniform_int_distribution<> distPriorityIndex(0, 2);
	const boost::random::uniform_int_distribution<> distAbsExpiration(0, NUMBER_OF_EXPIRATIONS - 1);
	const boost::random::uniform_int_distribution<> distTotalBundleSize(1, 65536);

	g_sigHandler.Start();

	static const boost::uint64_t DEST_LINKS[10] = { 1,2,3,4,5,6,7,8,9,10 };
	std::vector<boost::uint64_t> availableDestLinks = { 1,2,3,4,5,6,7,8,9,10 };



	BundleStorageManagerMT bsm;
	bsm.Start();

	for (int i = 0; i < 10; ++i) {
		bsm.AddLink(DEST_LINKS[i]);
	}

	

	static const boost::uint64_t sizes[10] = {
		
		BUNDLE_STORAGE_PER_SEGMENT_SIZE - 2 ,		
		BUNDLE_STORAGE_PER_SEGMENT_SIZE + 2,

		2 * BUNDLE_STORAGE_PER_SEGMENT_SIZE - 2 ,		
		2 * BUNDLE_STORAGE_PER_SEGMENT_SIZE + 2,

		500 * BUNDLE_STORAGE_PER_SEGMENT_SIZE - 2 ,		
		500 * BUNDLE_STORAGE_PER_SEGMENT_SIZE + 2,

		1000 * BUNDLE_STORAGE_PER_SEGMENT_SIZE - 2 ,
		1000 * BUNDLE_STORAGE_PER_SEGMENT_SIZE + 2,

		10000 * BUNDLE_STORAGE_PER_SEGMENT_SIZE - 2 ,
		10000 * BUNDLE_STORAGE_PER_SEGMENT_SIZE + 2,
	};
	std::cout << "generating test files\n";
	std::vector<TestFile> testFiles(10);
	std::map<boost::uint64_t, TestFile*> fileMap;
	for (std::size_t i = 0; i < 10; ++i) {
		testFiles[i] = TestFile(sizes[i]);
		fileMap[sizes[i]] = &testFiles[i];
	}
	std::cout << "done generating test files\n";
	
	boost::uint64_t totalSegmentsStoredOnDisk = 0;
	double gigaBitsPerSecReadDoubleAvg = 0.0, gigaBitsPerSecWriteDoubleAvg = 0.0;
	const unsigned int NUM_TESTS = 5;
	for (unsigned int testI = 0; testI < NUM_TESTS; ++testI) {

		{
			std::cout << "filling up the storage\n";
			boost::uint64_t totalBytesWrittenThisTest = 0;
			boost::timer::cpu_timer timer;
			while (g_running) {
				const unsigned int fileIdx = distFileId(gen);
				std::vector<boost::uint8_t> & data = testFiles[fileIdx].m_data;
				const boost::uint64_t size = data.size();
				//std::cout << "testing size " << size << "\n";

				const unsigned int linkId = distLinkId(gen);
				const unsigned int priorityIndex = distPriorityIndex(gen);
				const abs_expiration_t absExpiration = distAbsExpiration(gen);

				BundleStorageManagerSession_WriteToDisk sessionWrite;
				bp_primary_if_base_t bundleMetaData;
				bundleMetaData.flags = (priorityIndex & 3) << 7;
				bundleMetaData.dst_node = DEST_LINKS[linkId];
				bundleMetaData.length = size;
				bundleMetaData.creation = 0;
				bundleMetaData.lifetime = absExpiration;

				boost::uint64_t totalSegmentsRequired = bsm.Push(sessionWrite, bundleMetaData);
				//std::cout << "totalSegmentsRequired " << totalSegmentsRequired << "\n";
				if (totalSegmentsRequired == 0) break;
				totalSegmentsStoredOnDisk += totalSegmentsRequired;
				totalBytesWrittenThisTest += size;

				for (boost::uint64_t i = 0; i < totalSegmentsRequired; ++i) {
					std::size_t bytesToCopy = BUNDLE_STORAGE_PER_SEGMENT_SIZE;
					if (i == totalSegmentsRequired - 1) {
						boost::uint64_t modBytes = (size % BUNDLE_STORAGE_PER_SEGMENT_SIZE);
						if (modBytes != 0) {
							bytesToCopy = modBytes;
						}
					}

					bsm.PushSegment(sessionWrite, &data[i*BUNDLE_STORAGE_PER_SEGMENT_SIZE], bytesToCopy);
				}
			}
			const boost::uint64_t nanoSecWall = timer.elapsed().wall;
			//std::cout << "nanosec=" << nanoSecWall << "\n";
			const double bytesPerNanoSecDouble = static_cast<double>(totalBytesWrittenThisTest) / static_cast<double>(nanoSecWall);
			const double gigaBytesPerSecDouble = bytesPerNanoSecDouble;// / 1e9 * 1e9;
			//std::cout << "GBytes/sec=" << gigaBytesPerSecDouble << "\n";
			const double gigaBitsPerSecDouble = gigaBytesPerSecDouble * 8.0;
			gigaBitsPerSecWriteDoubleAvg += gigaBitsPerSecDouble;
			std::cout << "WRITE GBits/sec=" << gigaBitsPerSecDouble << "\n\n";
		}
		{
			std::cout << "reading half of the stored\n";
			boost::uint64_t totalBytesReadThisTest = 0;
			boost::timer::cpu_timer timer;
			while (g_running) {

				BundleStorageManagerSession_ReadFromDisk sessionRead;
				boost::uint64_t bytesToReadFromDisk = bsm.PopTop(sessionRead, availableDestLinks);
				//std::cout << "bytesToReadFromDisk " << bytesToReadFromDisk << "\n";
				std::vector<boost::uint8_t> dataReadBack(bytesToReadFromDisk);
				TestFile & originalFile = *fileMap[bytesToReadFromDisk];

				const std::size_t numSegmentsToRead = sessionRead.chainInfo.second.size();
				std::size_t totalBytesRead = 0;
				for (std::size_t i = 0; i < numSegmentsToRead; ++i) {
					totalBytesRead += bsm.TopSegment(sessionRead, &dataReadBack[i*BUNDLE_STORAGE_PER_SEGMENT_SIZE]);
				}
				//std::cout << "totalBytesRead " << totalBytesRead << "\n";
				if (totalBytesRead != bytesToReadFromDisk) return false;
				totalBytesReadThisTest += totalBytesRead;
				if (dataReadBack != originalFile.m_data) {
					std::cout << "dataReadBack does not equal data\n";
					return false;
				}
				if (!bsm.RemoveReadBundleFromDisk(sessionRead)) {
					std::cout << "error freeing bundle from disk\n";
					return false;
				}

				totalSegmentsStoredOnDisk -= numSegmentsToRead;
				if (totalSegmentsStoredOnDisk < (MAX_SEGMENTS / 2)) {
					break;
				}
			}
			const boost::uint64_t nanoSecWall = timer.elapsed().wall;
			//std::cout << "nanosec=" << nanoSecWall << "\n";
			const double bytesPerNanoSecDouble = static_cast<double>(totalBytesReadThisTest) / static_cast<double>(nanoSecWall);
			const double gigaBytesPerSecDouble = bytesPerNanoSecDouble;// / 1e9 * 1e9;
			//std::cout << "GBytes/sec=" << gigaBytesPerSecDouble << "\n";
			const double gigaBitsPerSecDouble = gigaBytesPerSecDouble * 8.0;
			gigaBitsPerSecReadDoubleAvg += gigaBitsPerSecDouble;
			std::cout << "READ GBits/sec=" << gigaBitsPerSecDouble << "\n\n";
		}
	}

	if (g_running) {
		std::cout << "Read avg GBits/sec=" << gigaBitsPerSecReadDoubleAvg / NUM_TESTS << "\n\n";
		std::cout << "Write avg GBits/sec=" << gigaBitsPerSecWriteDoubleAvg / NUM_TESTS << "\n\n";
	}
	return true;

}


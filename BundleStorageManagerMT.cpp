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
static const char * FILE_PATHS[NUM_STORAGE_THREADS] = { "/mnt/sda1/test/map0.bin", "/mnt/sdb1/test/map1.bin", "/mnt/sdc1/test/map2.bin", "/mnt/sdd1/test/map3.bin" };
#endif



BundleStorageManagerMT::BundleStorageManagerMT() : m_lockMainThread(m_mutexMainThread), m_running(true) {
	m_circularBufferBlockDataPtr = (uint8_t*) malloc(CIRCULAR_INDEX_BUFFER_SIZE * NUM_STORAGE_THREADS * SEGMENT_SIZE * sizeof(uint8_t));
	m_circularBufferSegmentIdsPtr = (segment_id_t*) malloc(CIRCULAR_INDEX_BUFFER_SIZE * NUM_STORAGE_THREADS * sizeof(segment_id_t));
	

	for (unsigned int tId = 0; tId < NUM_STORAGE_THREADS; ++tId) {		
		m_threadPtrs[tId] = boost::make_shared<boost::thread>(
			boost::bind(&BundleStorageManagerMT::ThreadFunc, this, tId)); //create and start the worker thread
	}
}
BundleStorageManagerMT::~BundleStorageManagerMT() {
	m_running = false; //thread stopping criteria
	for (unsigned int tId = 0; tId < NUM_STORAGE_THREADS; ++tId) {
		m_threadPtrs[tId]->join();
		m_threadPtrs[tId] = boost::shared_ptr<boost::thread>();
	}

	free(m_circularBufferBlockDataPtr);
	free(m_circularBufferSegmentIdsPtr);
	
}

void BundleStorageManagerMT::ThreadFunc(const unsigned int threadIndex) {

	boost::mutex localMutex;
	boost::mutex::scoped_lock lock(localMutex);
	boost::condition_variable & cv = m_conditionVariables[threadIndex];
	CircularIndexBufferSingleProducerSingleConsumer & cb = m_circularIndexBuffers[threadIndex];
	const char * const filePath = FILE_PATHS[threadIndex];
	FILE * fileHandle = fopen(filePath, "w+bR");
	boost::uint8_t * const circularBufferBlockDataPtr = &m_circularBufferBlockDataPtr[threadIndex * CIRCULAR_INDEX_BUFFER_SIZE * SEGMENT_SIZE];
	segment_id_t * const circularBufferSegmentIdsPtr = &m_circularBufferSegmentIdsPtr[threadIndex * CIRCULAR_INDEX_BUFFER_SIZE];
	
	while (m_running) {
		
			
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

	if (boost::filesystem::exists(p)) {
		boost::filesystem::remove(p);
		std::cout << "deleted " << p.string() << "\n";
	}

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
	std::cout << "writing " << size << " bytes\n";
	if (session.nextLogicalSegment == segmentIdChainVec.size()) {		
		priority_vec_t & priorityVec = m_destMap[session.destLinkId];
		expiration_map_t & expirationMap = priorityVec[session.priorityIndex];
		chain_info_vec_t & chainInfoVec = expirationMap[session.absExpiration];
		chainInfoVec.push_back(std::move(chainInfo));
		std::cout << "write complete\n";
	}

	return 1;
}
/*
void BundleStorageManagerMT::StoreBundle(const std::string & linkName, const unsigned int priorityIndex, const abs_expiration_t absExpiration, const segment_id_t segmentId, const boost::uint32_t logicalIndex, const unsigned char * const data) {

	const unsigned int threadIndex = segmentId % NUM_STORAGE_THREADS;
	CircularIndexBufferSingleProducerSingleConsumer & cb = m_circularIndexBuffers[threadIndex];
	boost::condition_variable & cv = m_conditionVariables[threadIndex];
	unsigned int produceIndex = cb.GetIndexForWrite();
	while (produceIndex == UINT32_MAX) { //store the volatile, wait until not full				
		m_conditionVariableMainThread.timed_wait(m_lockMainThread, boost::posix_time::milliseconds(10)); // call lock.unlock() and blocks the current thread
		//thread is now unblocked, and the lock is reacquired by invoking lock.lock()	
		produceIndex = cb.GetIndexForWrite();
	}


	priority_vec_t & priorityVec = m_destMap[linkName];
	expiration_map_t & expirationMap = priorityVec[priorityIndex];
	chains_vec_t & chainsVec = expirationMap[absExpiration];
	if (logicalIndex == 0) {
		chainsVec.resize(chainsVec.size() + 1);
	}
	chain_info_t & chainInfo = chainsVec.back();
	boost::uint64_t & totalBundleSizeBytes = chainInfo.first;
	segment_id_chain_vec_t & segmentIdChainVec = chainInfo.second;
	//std::cout << "add exp=" << absExpiration << " seg=" << segmentId << " pri=" << priorityIndex << "link=" << linkName << "\n";
	segmentIdChainVec.push_back(segmentId);
	////segmentIdVec.shrink_to_fit();

	boost::uint8_t * const circularBufferBlockDataPtr = &m_circularBufferBlockDataPtr[threadIndex * CIRCULAR_INDEX_BUFFER_SIZE * SEGMENT_SIZE];
	segment_id_t * const circularBufferSegmentIdsPtr = &m_circularBufferSegmentIdsPtr[threadIndex * CIRCULAR_INDEX_BUFFER_SIZE];
	bool * const circularBufferReadWriteBoolsPtr = &m_circularBufferReadWriteBoolsPtr[threadIndex * CIRCULAR_INDEX_BUFFER_SIZE];

	boost::uint8_t * const dataCb = &circularBufferBlockDataPtr[produceIndex * SEGMENT_SIZE];
	circularBufferSegmentIdsPtr[produceIndex] = segmentId;
	circularBufferReadWriteBoolsPtr[produceIndex] = true; //isWriteToDisk

	memcpy(dataCb, data, SEGMENT_SIZE);

	cb.CommitWrite();
	cv.notify_one();

}*/

uint64_t BundleStorageManagerMT::Top(BundleStorageManagerSession_ReadFromDisk & session, const std::vector<uint64_t> & availableDestLinks) { //0 if empty, size if entry
	std::vector<priority_vec_t *> priorityVecPtrs(availableDestLinks.size());
	for (std::size_t i = 0; i < availableDestLinks.size(); ++i) {
		priorityVecPtrs[i] = &m_destMap[availableDestLinks[i]];
	}
	session.nextLogicalSegment = 0;
	session.nextLogicalSegmentToCache = 0;
	session.cacheReadIndex = 0;
	session.cacheWriteIndex = 0;
	
	//memset((uint8_t*)session.readCacheIsSegmentReady, 0, READ_CACHE_NUM_SEGMENTS_PER_SESSION);
	for (int i = NUMBER_OF_PRIORITIES-1; i >=0; --i) { //00 = bulk, 01 = normal, 10 = expedited
		abs_expiration_t lowestExpiration = UINT64_MAX;
		std::size_t linkIndex;
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
					linkIndex = j;
					session.expirationMapPtr = &expirationMap;
					session.chainInfoVecPtr = &it->second;
					session.expirationMapIterator = it;
					
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
/*
int BundleStorageManagerMT::Pop(BundleStorageManagerSession_ReadFromDisk & session) { //remove top value

	if (session.chainInfoVecPtr) {
		session.chainInfoVecPtr->erase(session.chainInfoVecPtr->begin());


		if (session.chainInfoVecPtr->size() == 0) {
			session.expirationMapPtr->erase(session.expirationMapIterator);
		}
		else {
			////segmentIdVecPtr->shrink_to_fit();
		}
		return 1;
	}
	return 0;
}
*/
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
//uint64_t BundleStorageManagerMT::TopSegmentCount(BundleStorageManagerSession_ReadFromDisk & session) {
//	return session.chainInfoVecPtr->front().second.size(); //use the front as new writes will be pushed back
//}

/*
segment_id_t BundleStorageManagerMT::GetBundle(const std::vector<std::string> & availableDestLinks) {

	//std::cout << "adlsz " << availableDestLinks.size() << "\n";
	std::vector<priority_vec_t *> priorityVecPtrs(availableDestLinks.size());
	for (std::size_t i = 0; i < availableDestLinks.size(); ++i) {
		priorityVecPtrs[i] = &m_destMap[availableDestLinks[i]];
		//std::cout << "pv " << priorityVecPtrs[i] << "\n";
	}
	

	for (unsigned int i = 0; i < NUMBER_OF_PRIORITIES; ++i) {
		abs_expiration_t lowestExpiration = UINT64_MAX;
		std::size_t linkIndex;
		expiration_map_t * expirationMapPtr = NULL;
		segment_id_vec_t * segmentIdVecPtr = NULL;
		expiration_map_t::iterator expirationMapIterator;
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
					linkIndex = j;
					expirationMapPtr = &expirationMap;
					segmentIdVecPtr = &it->second;
					expirationMapIterator = it;
				}
			}
		}
		if (segmentIdVecPtr) {
			//std::cout << "szb4=" << segmentIdVecPtr->size();
			const segment_id_t segmentId = segmentIdVecPtr->back();
			segmentIdVecPtr->pop_back();
			//std::cout << "szaf=" << segmentIdVecPtr->size() << "\n";
			//std::cout << "remove exp=" << lowestExpiration << " seg=" << segmentId << " pri=" << i << "link=" << availableDestLinks[linkIndex] << "\n";


			if (segmentIdVecPtr->size() == 0) {
				//std::cout << "sz0\n";
				expirationMapPtr->erase(expirationMapIterator);
			}
			else {
				////segmentIdVecPtr->shrink_to_fit();
			}

			const unsigned int threadIndex = segmentId % NUM_STORAGE_THREADS;
			CircularIndexBufferSingleProducerSingleConsumer & cb = m_circularIndexBuffers[threadIndex];
			boost::condition_variable & cv = m_conditionVariables[threadIndex];
			unsigned int produceIndex = cb.GetIndexForWrite();
			while(produceIndex == UINT32_MAX) { //store the volatile, wait until not full				
				m_conditionVariableMainThread.timed_wait(m_lockMainThread, boost::posix_time::milliseconds(10)); // call lock.unlock() and blocks the current thread
				//thread is now unblocked, and the lock is reacquired by invoking lock.lock()	
				produceIndex = cb.GetIndexForWrite();
			} 

			boost::uint8_t * const circularBufferBlockDataPtr = &m_circularBufferBlockDataPtr[threadIndex * CIRCULAR_INDEX_BUFFER_SIZE * SEGMENT_SIZE];
			segment_id_t * const circularBufferSegmentIdsPtr = &m_circularBufferSegmentIdsPtr[threadIndex * CIRCULAR_INDEX_BUFFER_SIZE];
			bool * const circularBufferReadWriteBoolsPtr = &m_circularBufferReadWriteBoolsPtr[threadIndex * CIRCULAR_INDEX_BUFFER_SIZE];

			boost::uint8_t * const dataCb = &circularBufferBlockDataPtr[produceIndex * SEGMENT_SIZE];
			circularBufferSegmentIdsPtr[produceIndex] = segmentId;
			circularBufferReadWriteBoolsPtr[produceIndex] = false; //isWriteToDisk

			//copy data (expected) to dataCb for thread to run comparison
			unsigned int linkIdCpy = static_cast<unsigned int>(linkIndex);
			unsigned int priorityIndexCpy = i;
			memcpy(&dataCb[1000], &linkIdCpy, sizeof(linkIdCpy));
			memcpy(&dataCb[2000], &priorityIndexCpy, sizeof(priorityIndexCpy));
			//abs_expiration_t absExpirationcpy = absExpiration + 100000;
			memcpy(&dataCb[3000], &lowestExpiration, sizeof(lowestExpiration));
			memcpy(&dataCb[4000], &segmentId, sizeof(segmentId));

			cb.CommitWrite();
			cv.notify_one();
			
			return segmentId;
		}
	}
	return UINT32_MAX;

}
*/
static volatile bool g_running = true;

void MonitorExitKeypressThreadFunction() {
	std::cout << "Keyboard Interrupt.. exiting\n";
	g_running = false; //do this first
}

static SignalHandler g_sigHandler(boost::bind(&MonitorExitKeypressThreadFunction));


/*
static bool Store(const boost::uint32_t numSegments, MemoryManagerTreeArray & mmt, BundleStorageManagerMT & bsm, const std::string * DEST_LINKS) {
	static boost::random::mt19937 gen(static_cast<unsigned int>(std::time(0)));
	static const boost::random::uniform_int_distribution<> distLinkId(0, 9);
	static const boost::random::uniform_int_distribution<> distPriorityIndex(0, 2);
	static const boost::random::uniform_int_distribution<> distAbsExpiration(0, NUMBER_OF_EXPIRATIONS - 1);
	static const boost::random::uniform_int_distribution<> distTotalBundleSize(1, 65536);
	static unsigned char junkData[SEGMENT_SIZE];

	const boost::uint64_t bundleSize = distTotalBundleSize(gen);
	const boost::uint64_t totalSegmentsRequired = (bundleSize / BUNDLE_STORAGE_PER_SEGMENT_SIZE) + ((bundleSize % BUNDLE_STORAGE_PER_SEGMENT_SIZE) == 0 ? 0 : 1);


	for (boost::uint32_t i = 0; i < numSegments; ++i) {
		if (!g_running) return false;

		const segment_id_t segmentId = mmt.GetAndSetFirstFreeSegmentId();
		if (segmentId == UINT32_MAX) {
			std::cout << "error segmentId is max, i=" << i << "\n";
			return false;
		}
		const unsigned int linkId = distLinkId(gen);
		const unsigned int priorityIndex = distPriorityIndex(gen);
		const abs_expiration_t absExpiration = distAbsExpiration(gen);

		memcpy(&junkData[1000], &linkId, sizeof(linkId));
		memcpy(&junkData[2000], &priorityIndex, sizeof(priorityIndex));
		abs_expiration_t absExpirationcpy = absExpiration + 100000;
		memcpy(&junkData[3000], &absExpirationcpy, sizeof(absExpirationcpy));
		memcpy(&junkData[4000], &segmentId, sizeof(segmentId));
		bsm.StoreBundle(DEST_LINKS[linkId], priorityIndex, absExpiration + 100000, segmentId, junkData);
	}
	return true;
}

static bool Retrieve(const boost::uint32_t numSegments, MemoryManagerTreeArray & mmt, BundleStorageManagerMT & bsm, const std::string * DEST_LINKS, const std::vector<std::string> & availableDestLinks) {

	for (boost::uint32_t i = 0; i < numSegments; ++i) {
		if (!g_running) return false;

		const segment_id_t segmentId = bsm.GetBundle(availableDestLinks);
		if (!mmt.FreeSegmentId(segmentId)) {
			std::cout << "error freeing segment id " << segmentId << "\n";
			return false;
		}
		
		
		if (segmentId == UINT32_MAX) {
			std::cout << "error segmentId is max, i=" << i << "\n";
			return false;
		}



	}
	return true;
}

bool BundleStorageManagerMT::TimeRandomReadsAndWrites() {
	
	g_sigHandler.Start();
	
	static const std::string DEST_LINKS[10] = { "a1", "a2","a3", "a4", "a5", "a6", "a7", "a8", "a9", "b1" };
	std::vector<std::string> availableDestLinks = { "a1", "a2","a3", "a4", "a5", "a6", "a7", "a8", "a9", "b1" };
	
	

	BundleStorageManagerMT bsm;
	MemoryManagerTreeArray mmt;
	
	for (int i = 0; i < 10; ++i) {
		bsm.AddLink(DEST_LINKS[i]);
	}

	
	
	std::cout << "storing\n";
	

	if (!Store(MAX_SEGMENTS, mmt, bsm, DEST_LINKS)) return false;

	std::cout << "done storing\n";
	const unsigned int numSegmentsPerTest = NUM_SEGMENTS_PER_TEST;
	const boost::uint64_t numBytesPerTest = static_cast<boost::uint64_t>(numSegmentsPerTest) * SEGMENT_SIZE;
	if (numBytesPerTest > FILE_SIZE) {
		std::cout << "error numBytesPerTest (" << numBytesPerTest << ") is greater than FILE_SIZE (" << FILE_SIZE << ")\n";
		return false;
	}
	double gigaBitsPerSecReadDoubleAvg = 0.0, gigaBitsPerSecWriteDoubleAvg = 0.0;
	for (unsigned int i = 0; i < 10; ++i) {
		{
			if (!g_running) return false;
			std::cout << "READ\n";
			boost::timer::cpu_timer timer;
			if (!Retrieve(numSegmentsPerTest, mmt, bsm, DEST_LINKS, availableDestLinks)) return false;
			const boost::uint64_t nanoSecWall = timer.elapsed().wall;
			//std::cout << "nanosec=" << nanoSecWall << "\n";
			const double bytesPerNanoSecDouble = static_cast<double>(numBytesPerTest) / static_cast<double>(nanoSecWall);
			const double gigaBytesPerSecDouble = bytesPerNanoSecDouble;// / 1e9 * 1e9;
			//std::cout << "GBytes/sec=" << gigaBytesPerSecDouble << "\n";
			const double gigaBitsPerSecDouble = gigaBytesPerSecDouble * 8.0;
			gigaBitsPerSecReadDoubleAvg += gigaBitsPerSecDouble;
			std::cout << "GBits/sec=" << gigaBitsPerSecDouble << "\n\n";
		}
		{
			if (!g_running) return false;
			std::cout << "WRITE\n";
			boost::timer::cpu_timer timer;
			if (!Store(numSegmentsPerTest, mmt, bsm, DEST_LINKS)) return false;
			const boost::uint64_t nanoSecWall = timer.elapsed().wall;
			//std::cout << "nanosec=" << nanoSecWall << "\n";
			const double bytesPerNanoSecDouble = static_cast<double>(numBytesPerTest) / static_cast<double>(nanoSecWall);
			const double gigaBytesPerSecDouble = bytesPerNanoSecDouble;// / 1e9 * 1e9;
			//std::cout << "GBytes/sec=" << gigaBytesPerSecDouble << "\n";
			const double gigaBitsPerSecDouble = gigaBytesPerSecDouble * 8.0;
			gigaBitsPerSecWriteDoubleAvg += gigaBitsPerSecDouble;
			std::cout << "GBits/sec=" << gigaBitsPerSecDouble << "\n\n";
		}
		
	}
	if (g_running) {
		std::cout << "Read avg GBits/sec=" << gigaBitsPerSecReadDoubleAvg / 10.0 << "\n\n";
		std::cout << "Write avg GBits/sec=" << gigaBitsPerSecWriteDoubleAvg / 10.0 << "\n\n";


		std::cout << "done reading\n";
	}

	
	return true;
}*/

bool BundleStorageManagerMT::Test() {
	static boost::random::mt19937 gen(static_cast<unsigned int>(std::time(0)));
	static const boost::random::uniform_int_distribution<> distRandomData(0, 255);
	static const boost::random::uniform_int_distribution<> distLinkId(0, 9);
	static const boost::random::uniform_int_distribution<> distPriorityIndex(0, 2);
	static const boost::random::uniform_int_distribution<> distAbsExpiration(0, NUMBER_OF_EXPIRATIONS - 1);
	static const boost::random::uniform_int_distribution<> distTotalBundleSize(1, 65536);

	g_sigHandler.Start();

	static const boost::uint64_t DEST_LINKS[10] = { 1,2,3,4,5,6,7,8,9,10 };
	std::vector<boost::uint64_t> availableDestLinks = { 1,2,3,4,5,6,7,8,9,10 };



	BundleStorageManagerMT bsm;


	for (int i = 0; i < 10; ++i) {
		bsm.AddLink(DEST_LINKS[i]);
	}
	const boost::uint64_t size = 2*BUNDLE_STORAGE_PER_SEGMENT_SIZE-1;
	std::vector<boost::uint8_t> data(size);
	std::vector<boost::uint8_t> dataReadBack(size);
	for (std::size_t i = 0; i < size; ++i) {
		data[i] = distRandomData(gen);
	}
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
	std::cout << "totalSegmentsRequired " << totalSegmentsRequired << "\n";
	if (totalSegmentsRequired == 0) return false;

	for (boost::uint64_t i = 0; i < totalSegmentsRequired; ++i) {
		std::size_t bytesToCopy = BUNDLE_STORAGE_PER_SEGMENT_SIZE;
		if(i == totalSegmentsRequired - 1) {
			boost::uint64_t modBytes = (size % BUNDLE_STORAGE_PER_SEGMENT_SIZE);			
			if (modBytes != 0) {
				bytesToCopy = modBytes;
			}
		}

		bsm.PushSegment(sessionWrite, &data[i*BUNDLE_STORAGE_PER_SEGMENT_SIZE], bytesToCopy);
	}

	BundleStorageManagerSession_ReadFromDisk sessionRead;
	boost::uint64_t bytesToReadFromDisk = bsm.Top(sessionRead, availableDestLinks);
	std::cout << "bytesToReadFromDisk " << bytesToReadFromDisk << "\n";
	if (bytesToReadFromDisk != size) return false;

	//check if custody was taken (should be empty)
	{
		BundleStorageManagerSession_ReadFromDisk sessionRead2;
		boost::uint64_t bytesToReadFromDisk2 = bsm.Top(sessionRead2, availableDestLinks);
		std::cout << "bytesToReadFromDisk2 " << bytesToReadFromDisk2 << "\n";
		if (bytesToReadFromDisk2 != 0) return false;
	}

	if (dataReadBack == data) {
		std::cout << "dataReadBack should not equal data yet\n";
		return false;
	}

	const std::size_t numSegmentsToRead = sessionRead.chainInfo.second.size();
	std::size_t totalBytesRead = 0;
	for (std::size_t i = 0; i < numSegmentsToRead; ++i) {
		totalBytesRead += bsm.TopSegment(sessionRead, &dataReadBack[i*BUNDLE_STORAGE_PER_SEGMENT_SIZE]);
	}
	std::cout << "totalBytesRead " << totalBytesRead << "\n";
	if (totalBytesRead != size) return false;
	if (dataReadBack != data) {
		std::cout << "dataReadBack does not equal data\n";
		return false;
	}

	return true;

}
#include "BundleStorageManagerMT.h"
#include "MemoryManagerTreeArray.h"
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
	m_circularBufferReadWriteBoolsPtr = (bool*) malloc(CIRCULAR_INDEX_BUFFER_SIZE * NUM_STORAGE_THREADS * sizeof(bool));

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
	free(m_circularBufferReadWriteBoolsPtr);
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
	bool * const circularBufferReadWriteBoolsPtr = &m_circularBufferReadWriteBoolsPtr[threadIndex * CIRCULAR_INDEX_BUFFER_SIZE];
	boost::uint8_t junkDataReceived[SEGMENT_SIZE];

	while (m_running) {
		
			
		const unsigned int consumeIndex = cb.GetIndexForRead(); //store the volatile
			
		if (consumeIndex == UINT32_MAX) { //if empty
			cv.timed_wait(lock, boost::posix_time::milliseconds(10)); // call lock.unlock() and blocks the current thread
			//thread is now unblocked, and the lock is reacquired by invoking lock.lock()
			continue;
		}

		boost::uint8_t * const data = &circularBufferBlockDataPtr[consumeIndex * SEGMENT_SIZE]; //expected data for testing when reading
		const segment_id_t segmentId = circularBufferSegmentIdsPtr[consumeIndex];
		const bool isWriteToDisk = circularBufferReadWriteBoolsPtr[consumeIndex];
		
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
			if (fread(junkDataReceived, 1, SEGMENT_SIZE, fileHandle) != SEGMENT_SIZE) {
				std::cout << "error reading\n";
			}

			unsigned int linkIdExpected;
			unsigned int priorityIndexExpected;
			abs_expiration_t absExpirationExpected;
			boost::uint32_t segmentIdExpected;
			memcpy(&linkIdExpected, &data[1000], sizeof(linkIdExpected));
			memcpy(&priorityIndexExpected, &data[2000], sizeof(priorityIndexExpected));
			//abs_expiration_t absExpirationcpy = absExpiration + 100000;
			memcpy(&absExpirationExpected, &data[3000], sizeof(absExpirationExpected));
			memcpy(&segmentIdExpected, &data[4000], sizeof(segmentIdExpected));

			unsigned int linkIdRetrieved;
			unsigned int priorityIndexRetrieved;
			abs_expiration_t absExpirationRetrieved;
			boost::uint32_t segmentIdRetrieved;
			memcpy(&linkIdRetrieved, &junkDataReceived[1000], sizeof(linkIdRetrieved));
			memcpy(&priorityIndexRetrieved, &junkDataReceived[2000], sizeof(priorityIndexRetrieved));
			//abs_expiration_t absExpirationcpy = absExpiration + 100000;
			memcpy(&absExpirationRetrieved, &junkDataReceived[3000], sizeof(absExpirationRetrieved));
			memcpy(&segmentIdRetrieved, &junkDataReceived[4000], sizeof(segmentIdRetrieved));
			bool success = true;
			if (linkIdExpected != linkIdRetrieved) {
				std::cout << "mismatch linkidExpected " << linkIdExpected << "  linkIdRetrieved " << linkIdRetrieved << "\n";
				success = false;
			}
			if (priorityIndexExpected != priorityIndexRetrieved) {
				std::cout << "mismatch retPriorityIndexExpected " << priorityIndexExpected << "  priorityIndexRetrieved " << priorityIndexRetrieved << "\n";
				success = false;
			}
			if (absExpirationExpected != absExpirationRetrieved) {
				std::cout << "mismatch retAbsExpirationExpected " << absExpirationExpected << "  absExpirationRetrieved " << absExpirationRetrieved << "\n";
				success = false;
			}
			if (segmentIdExpected != segmentId) {
				std::cout << "mismatch segmentIdExpected " << segmentIdExpected << " " << segmentId << "\n";
				success = false;
			}
			if (segmentIdRetrieved != segmentId) {
				std::cout << "mismatch segmentIdRetrieved " << segmentIdRetrieved << " " << segmentId << "\n";
				success = false;
			}
			if (!success) {
				m_running = false;

			}


			if (segmentId == UINT32_MAX) {
				std::cout << "error segmentId is max\n";
				m_running = false;
			}
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



void BundleStorageManagerMT::AddLink(const std::string & linkName) {
	m_destMap[linkName] = priority_vec_t(NUMBER_OF_PRIORITIES);
}

void BundleStorageManagerMT::StoreBundle(const std::string & linkName, const unsigned int priorityIndex, const abs_expiration_t absExpiration, const segment_id_t segmentId, const unsigned char * const data) {

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
	segment_id_vec_t & segmentIdVec = expirationMap[absExpiration];
	//std::cout << "add exp=" << absExpiration << " seg=" << segmentId << " pri=" << priorityIndex << "link=" << linkName << "\n";
	segmentIdVec.push_back(segmentId);
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

}

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

static volatile bool g_running = true;

void MonitorExitKeypressThreadFunction() {
	std::cout << "Keyboard Interrupt.. exiting\n";
	g_running = false; //do this first
}

static SignalHandler g_sigHandler(boost::bind(&MonitorExitKeypressThreadFunction));



static bool Store(const boost::uint32_t numSegments, MemoryManagerTreeArray & mmt, BundleStorageManagerMT & bsm, const std::string * DEST_LINKS) {
	static boost::random::mt19937 gen(static_cast<unsigned int>(std::time(0)));
	static const boost::random::uniform_int_distribution<> distLinkId(0, 9);
	static const boost::random::uniform_int_distribution<> distPriorityIndex(0, 2);
	static const boost::random::uniform_int_distribution<> distAbsExpiration(0, NUMBER_OF_EXPIRATIONS - 1);
	static unsigned char junkData[SEGMENT_SIZE];

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
}
#include "BundleStorageManager.h"
#include "MemoryManagerTreeArray.h"
#include <iostream>
#include <string>
#include <boost/filesystem.hpp>
#include <boost/random/mersenne_twister.hpp>
#include <boost/random/uniform_int_distribution.hpp>
#include <boost/timer/timer.hpp>

BundleStorageManager::BundleStorageManager() {
	OpenFile();
}
BundleStorageManager::~BundleStorageManager() {
	CloseFile();
}

void BundleStorageManager::OpenFile() {
	boost::iostreams::mapped_file_params  params;
	params.path = "map.bin";
	params.new_file_size = FILE_SIZE; //0 to read file
	//params.mode = (std::ios_base::out | std::ios_base::in);
	params.flags = boost::iostreams::mapped_file::mapmode::readwrite;
	
	m_mappedFile.open(params);
	std::cout << "gran " << m_mappedFile.alignment() << "\n";
	//const char * teststr = "hello this is a test";
	//memcpy(m_mappedFile.data(), teststr, strlen(teststr));
	//char buf[100];
	//memcpy(buf, m_mappedFile.data(), 100);
	//std::cout << "buf:" << buf << "\n";
	
}

void BundleStorageManager::CloseFile() {
	if (m_mappedFile.is_open()) {
		m_mappedFile.close();
	}
	const boost::filesystem::path p("map.bin");

	if (boost::filesystem::exists(p)) {
		boost::filesystem::remove(p);
		std::cout << "deleted " << p.string() << "\n";
	}
}


void BundleStorageManager::AddLink(const std::string & linkName) {
#ifdef USE_VECTOR_CIRCULAR_BUFFER
	priority_vec_t priorityVec(NUMBER_OF_PRIORITIES);
	for (unsigned int i = 0; i < NUMBER_OF_PRIORITIES; ++i) {
		priorityVec[i] = expiration_circular_buf_t(NUMBER_OF_EXPIRATIONS);
	}
	m_destMap[linkName] = std::move(priorityVec);
#else
	m_destMap[linkName] = priority_vec_t(NUMBER_OF_PRIORITIES);
#endif
}

void BundleStorageManager::StoreBundle(const std::string & linkName, const unsigned int priorityIndex, const abs_expiration_t absExpiration, const segment_id_t segmentId, const unsigned char * const data, std::size_t dataSize) {
#ifdef USE_VECTOR_CIRCULAR_BUFFER

#else
	priority_vec_t & priorityVec = m_destMap[linkName];
	expiration_map_t & expirationMap = priorityVec[priorityIndex];
	segment_id_vec_t & segmentIdVec = expirationMap[absExpiration];
	//std::cout << "add exp=" << absExpiration << " seg=" << segmentId << " pri=" << priorityIndex << "link=" << linkName << "\n";
	segmentIdVec.push_back(segmentId);
	////segmentIdVec.shrink_to_fit();
	if (data) {
		const boost::uint64_t offsetBytes = static_cast<boost::uint64_t>(segmentId) * SEGMENT_SIZE;
		memcpy(m_mappedFile.data() + offsetBytes, data, dataSize);
	}
#endif
}

segment_id_t BundleStorageManager::GetBundle(const std::vector<std::string> & availableDestLinks, std::size_t & retLinkIndex, unsigned int & retPriorityIndex, abs_expiration_t & retAbsExpiration, unsigned char * const data, std::size_t dataSize) {
#ifdef USE_VECTOR_CIRCULAR_BUFFER

#else
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
			retAbsExpiration = lowestExpiration;
			retLinkIndex = linkIndex;
			retPriorityIndex = i;

			if (segmentIdVecPtr->size() == 0) {
				//std::cout << "sz0\n";
				expirationMapPtr->erase(expirationMapIterator);
			}
			else {
				////segmentIdVecPtr->shrink_to_fit();
			}
			if (data) {
				const boost::uint64_t offsetBytes = static_cast<boost::uint64_t>(segmentId) * SEGMENT_SIZE;
				memcpy(data, m_mappedFile.data() + offsetBytes, dataSize);
			}
			return segmentId;
		}
	}
	return UINT32_MAX;
#endif
}

void SetJunk(char * data) {

}

bool BundleStorageManager::UnitTest() {
	//boost::random::mt19937 gen(std::time(0));

	//boost::random::uniform_int_distribution<> dist255(0, 255);
	static const std::string DEST_LINKS[10] = { "a1", "a2","a3", "a4", "a5", "a6", "a7", "a8", "a9", "b1" };
	std::vector<std::string> availableDestLinks = { "a1", "a2","a3", "a4", "a5", "a6", "a7", "a8", "a9", "b1" };
	static unsigned char junkData[SEGMENT_SIZE];
	static unsigned char junkDataReceived[SEGMENT_SIZE];
	
	BundleStorageManager bsm;
	//bsm.OpenFile();
	//getchar();
	
	MemoryManagerTreeArray mmt;
	//mmt.SetupTree();
	for (int i = 0; i < 10; ++i) {
		bsm.AddLink(DEST_LINKS[i]);
	}

	unsigned int linkId = 0;
	unsigned int priorityIndex = 0;
	abs_expiration_t absExpiration = 0;
	segment_id_t segmentId;

	std::cout << "storing\n";
	boost::timer::auto_cpu_timer timer;

	//for (boost::uint32_t i = 0; i < 16777216 * 35; ++i) {
	for (boost::uint32_t i = 0; i < MAX_SEGMENTS; ++i) {
		if (i % 16777216 == 0) {
			std::cout << "about to push, i=" << i << "\n";
			//getchar();
		}
		segmentId = mmt.GetAndSetFirstFreeSegmentId();
		if (segmentId != i) {
			std::cout << "error " << segmentId << " " << i << "\n";
			return false;
		}
		//for (boost::uint32_t k = 0; k < SEGMENT_SIZE; ++k) {
		//	junkData[k] = k ^ segmentId; //dist255(gen);
		//}
		memcpy(&junkData[1000], &linkId, sizeof(linkId));
		memcpy(&junkData[2000], &priorityIndex, sizeof(priorityIndex));
		abs_expiration_t absExpirationcpy = absExpiration + 100000;
		memcpy(&junkData[3000], &absExpirationcpy, sizeof(absExpirationcpy));
		memcpy(&junkData[4000], &segmentId, sizeof(segmentId));
		bsm.StoreBundle(DEST_LINKS[linkId], priorityIndex, absExpiration + 100000, segmentId, junkData, SEGMENT_SIZE);

		linkId = (linkId + 1) % 10;
		priorityIndex = (priorityIndex + 1) % 3;
		absExpiration = (absExpiration + 1) % NUMBER_OF_EXPIRATIONS;
	}

	std::cout << "done storing\n";
	
	linkId = 0;
	priorityIndex = 0;
	absExpiration = 0;
	for (boost::uint32_t i = 0; i < MAX_SEGMENTS; ++i) {
		std::size_t retLinkIndex;
		unsigned int retPriorityIndex;
		abs_expiration_t retAbsExpiration;
		segmentId = bsm.GetBundle(availableDestLinks, retLinkIndex, retPriorityIndex, retAbsExpiration, junkDataReceived, SEGMENT_SIZE);
		//if (memcmp(junkDataReceived, junkData, SEGMENT_SIZE) != 0) {
		//	std::cout << "received data doesnt match\n";
		//	return false;
		//}
		{
			unsigned int linkIdCpy;
			unsigned int priorityIndexCpy;
			abs_expiration_t absExpirationCpy;
			boost::uint32_t segmentIdCpy;
			memcpy(&linkIdCpy, &junkDataReceived[1000], sizeof(linkIdCpy));
			memcpy(&priorityIndexCpy, &junkDataReceived[2000], sizeof(priorityIndexCpy));
			//abs_expiration_t absExpirationcpy = absExpiration + 100000;
			memcpy(&absExpirationCpy, &junkDataReceived[3000], sizeof(absExpirationCpy));
			memcpy(&segmentIdCpy, &junkDataReceived[4000], sizeof(segmentIdCpy));
			bool success = true;
			//std::cout << "testing\n";
			//std::cout << "linkid " << linkIdCpy << " " << retLinkIndex << "\n";
			//std::cout << "retPriorityIndex " << priorityIndexCpy << " " << retPriorityIndex << "\n";
			//std::cout << "retAbsExpiration " << absExpirationCpy << " " << retAbsExpiration << "\n";
			//std::cout << "segmentId " << segmentIdCpy << " " << segmentId << "\n";
			if (linkIdCpy != retLinkIndex) {
				std::cout << "mismatch linkid " << linkIdCpy << " " << retLinkIndex << "\n";
				success = false;
			}
			if (priorityIndexCpy != retPriorityIndex) {
				std::cout << "mismatch retPriorityIndex " << priorityIndexCpy << " " << retPriorityIndex << "\n";
				success = false;
			}
			if (absExpirationCpy != retAbsExpiration) {
				std::cout << "mismatch retAbsExpiration " << absExpirationCpy << " " << retAbsExpiration << "\n";
				success = false;
			}
			if (segmentIdCpy != segmentId) {
				std::cout << "mismatch segmentId " << segmentIdCpy << " " << segmentId << "\n";
				success = false;
			}
			if (!success) return false;
		}

		if (segmentId == UINT32_MAX) {
			std::cout << "error segmentId is max, i=" << i << "\n";
			return false;
		}

		if (retPriorityIndex < priorityIndex) {
			std::cout << "error priority out of order\n";
			return false;
		}
		else if (retPriorityIndex > priorityIndex) {
			priorityIndex = retPriorityIndex;
			std::cout << "priority change to " << priorityIndex << "\n";
			absExpiration = 0;
		}

		if (retAbsExpiration < absExpiration) {
			std::cout << "error expiration out of order " << retAbsExpiration << "<=" << absExpiration << "\n";
			return false;
		}
		absExpiration = retAbsExpiration;
		
	}
	std::cout << "done reading\n";

	{
		std::size_t retLinkIndex;
		unsigned int retPriorityIndex;
		abs_expiration_t retAbsExpiration;
		const boost::uint32_t segmentId = bsm.GetBundle(availableDestLinks, retLinkIndex, retPriorityIndex, retAbsExpiration, NULL, 0);
		if (segmentId != UINT32_MAX) {
			std::cout << "error segmentId not max\n";
			return false;
		}
	}
	//getchar();
	//bsm.CloseFile();
	//mmt.FreeTree();
	return true;
}

bool Store(const boost::uint32_t numSegments, MemoryManagerTreeArray & mmt, BundleStorageManager & bsm, const std::string * DEST_LINKS) {
	static boost::random::mt19937 gen(std::time(0));
	static const boost::random::uniform_int_distribution<> distLinkId(0, 9);
	static const boost::random::uniform_int_distribution<> distPriorityIndex(0, 2);
	static const boost::random::uniform_int_distribution<> distAbsExpiration(0, NUMBER_OF_EXPIRATIONS - 1);
	static unsigned char junkData[SEGMENT_SIZE];

	for (boost::uint32_t i = 0; i < numSegments; ++i) {

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
		bsm.StoreBundle(DEST_LINKS[linkId], priorityIndex, absExpiration + 100000, segmentId, junkData, SEGMENT_SIZE);
	}
	return true;
}

bool Retrieve(const boost::uint32_t numSegments, MemoryManagerTreeArray & mmt, BundleStorageManager & bsm, const std::string * DEST_LINKS, const std::vector<std::string> & availableDestLinks) {

	static unsigned char junkDataReceived[SEGMENT_SIZE];

	for (boost::uint32_t i = 0; i < numSegments; ++i) {
		std::size_t retLinkIndex;
		unsigned int retPriorityIndex;
		abs_expiration_t retAbsExpiration;
		const segment_id_t segmentId = bsm.GetBundle(availableDestLinks, retLinkIndex, retPriorityIndex, retAbsExpiration, junkDataReceived, SEGMENT_SIZE);
		if (!mmt.FreeSegmentId(segmentId)) {
			std::cout << "error freeing segment id " << segmentId << "\n";
			return false;
		}
		
		unsigned int linkIdCpy;
		unsigned int priorityIndexCpy;
		abs_expiration_t absExpirationCpy;
		boost::uint32_t segmentIdCpy;
		memcpy(&linkIdCpy, &junkDataReceived[1000], sizeof(linkIdCpy));
		memcpy(&priorityIndexCpy, &junkDataReceived[2000], sizeof(priorityIndexCpy));
		//abs_expiration_t absExpirationcpy = absExpiration + 100000;
		memcpy(&absExpirationCpy, &junkDataReceived[3000], sizeof(absExpirationCpy));
		memcpy(&segmentIdCpy, &junkDataReceived[4000], sizeof(segmentIdCpy));
		bool success = true;
		if (linkIdCpy != retLinkIndex) {
			std::cout << "mismatch linkid " << linkIdCpy << " " << retLinkIndex << "\n";
			success = false;
		}
		if (priorityIndexCpy != retPriorityIndex) {
			std::cout << "mismatch retPriorityIndex " << priorityIndexCpy << " " << retPriorityIndex << "\n";
			success = false;
		}
		if (absExpirationCpy != retAbsExpiration) {
			std::cout << "mismatch retAbsExpiration " << absExpirationCpy << " " << retAbsExpiration << "\n";
			success = false;
		}
		if (segmentIdCpy != segmentId) {
			std::cout << "mismatch segmentId " << segmentIdCpy << " " << segmentId << "\n";
			success = false;
		}
		if (!success) return false;
		

		if (segmentId == UINT32_MAX) {
			std::cout << "error segmentId is max, i=" << i << "\n";
			return false;
		}



	}
	return true;
}

bool BundleStorageManager::TimeRandomReadsAndWrites() {
	

	
	static const std::string DEST_LINKS[10] = { "a1", "a2","a3", "a4", "a5", "a6", "a7", "a8", "a9", "b1" };
	std::vector<std::string> availableDestLinks = { "a1", "a2","a3", "a4", "a5", "a6", "a7", "a8", "a9", "b1" };
	
	

	BundleStorageManager bsm;
	MemoryManagerTreeArray mmt;
	
	for (int i = 0; i < 10; ++i) {
		bsm.AddLink(DEST_LINKS[i]);
	}

	
	
	std::cout << "storing\n";
	

	if (!Store(MAX_SEGMENTS, mmt, bsm, DEST_LINKS)) return false;

	std::cout << "done storing\n";
	const unsigned int numSegmentsPerTest = 100000;
	const boost::uint64_t numBytesPerTest = static_cast<boost::uint64_t>(numSegmentsPerTest) * 8192;
	for (unsigned int i = 0; i < 10; ++i) {
		{
			std::cout << "READ\n";
			boost::timer::cpu_timer timer;
			if (!Retrieve(numSegmentsPerTest, mmt, bsm, DEST_LINKS, availableDestLinks)) return false;
			const boost::uint64_t nanoSecWall = timer.elapsed().wall;
			//std::cout << "nanosec=" << nanoSecWall << "\n";
			const double bytesPerNanoSecDouble = static_cast<double>(numBytesPerTest) / static_cast<double>(nanoSecWall);
			const double gigaBytesPerSecDouble = bytesPerNanoSecDouble;// / 1e9 * 1e9;
			//std::cout << "GBytes/sec=" << gigaBytesPerSecDouble << "\n";
			const double gigaBitsPerSecDouble = gigaBytesPerSecDouble * 8.0;
			std::cout << "GBits/sec=" << gigaBitsPerSecDouble << "\n\n";
		}
		{
			std::cout << "WRITE\n";
			boost::timer::cpu_timer timer;
			if (!Store(numSegmentsPerTest, mmt, bsm, DEST_LINKS)) return false;
			const boost::uint64_t nanoSecWall = timer.elapsed().wall;
			//std::cout << "nanosec=" << nanoSecWall << "\n";
			const double bytesPerNanoSecDouble = static_cast<double>(numBytesPerTest) / static_cast<double>(nanoSecWall);
			const double gigaBytesPerSecDouble = bytesPerNanoSecDouble;// / 1e9 * 1e9;
			//std::cout << "GBytes/sec=" << gigaBytesPerSecDouble << "\n";
			const double gigaBitsPerSecDouble = gigaBytesPerSecDouble * 8.0;
			std::cout << "GBits/sec=" << gigaBitsPerSecDouble << "\n\n";
		}
		
	}

	
	
	std::cout << "done reading\n";

	
	return true;
}
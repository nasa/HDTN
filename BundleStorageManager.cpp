#include "BundleStorageManager.h"
#include "MemoryManagerTreeArray.h"
#include <iostream>
#include <string>



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

void BundleStorageManager::StoreBundle(const std::string & linkName, const unsigned int priorityIndex, const abs_expiration_t absExpiration, const segment_id_t segmentId) {
#ifdef USE_VECTOR_CIRCULAR_BUFFER

#else
	priority_vec_t & priorityVec = m_destMap[linkName];
	expiration_map_t & expirationMap = priorityVec[priorityIndex];
	segment_id_vec_t & segmentIdVec = expirationMap[absExpiration];
	std::cout << "add exp=" << absExpiration << " seg=" << segmentId << " pri=" << priorityIndex << "link=" << linkName << "\n";
	segmentIdVec.push_back(segmentId);
	////segmentIdVec.shrink_to_fit();
#endif
}

segment_id_t BundleStorageManager::GetBundle(const std::vector<std::string> & availableDestLinks) {
#ifdef USE_VECTOR_CIRCULAR_BUFFER

#else
	//std::cout << "adlsz " << availableDestLinks.size() << "\n";
	std::vector<priority_vec_t *> priorityVecPtrs(availableDestLinks.size());
	for (std::size_t i = 0; i < availableDestLinks.size(); ++i) {
		priorityVecPtrs[i] = &m_destMap[availableDestLinks[i]];
		//std::cout << "pv " << priorityVecPtrs[i] << "\n";
	}
	

	for (std::size_t i = 0; i < NUMBER_OF_PRIORITIES; ++i) {
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
			std::cout << "remove exp=" << lowestExpiration << " seg=" << segmentId << " pri=" << i << "link=" << availableDestLinks[linkIndex] << "\n";
			if (segmentIdVecPtr->size() == 0) {
				//std::cout << "sz0\n";
				expirationMapPtr->erase(expirationMapIterator);
			}
			else {
				////segmentIdVecPtr->shrink_to_fit();
			}
			return segmentId;
		}
	}
	return UINT32_MAX;
#endif
}

bool BundleStorageManager::UnitTest() {
	static const std::string DEST_LINKS[10] = { "a1", "a2","a3", "a4", "a5", "a6", "a7", "a8", "a9", "b1" };
	std::vector<std::string> availableDestLinks = { "a1", "a2","a3", "a4"};
	BundleStorageManager bsm;
	MemoryManagerTreeArray mmt;
	mmt.SetupTree();
	for (int i = 0; i < 10; ++i) {
		bsm.AddLink(DEST_LINKS[i]);
	}

	unsigned int linkId = 0;
	unsigned int priorityIndex = 0;
	abs_expiration_t absExpiration = 0;

	//for (boost::uint32_t i = 0; i < 16777216 * 2; ++i) {
	for (boost::uint32_t i = 0; i < 100; ++i) {
		if (i % 16777216 == 0) {
			std::cout << "about to push, i=" << i << "\n";
			//getchar();
		}
		const boost::uint32_t segmentId = mmt.GetAndSetFirstFreeSegmentId();
		if (segmentId != i) {
			std::cout << "error " << segmentId << " " << i << "\n";
			return false;
		}
		bsm.StoreBundle(DEST_LINKS[linkId], priorityIndex, absExpiration + 100000, segmentId);
		linkId = (linkId + 1) % 10;
		priorityIndex = (priorityIndex + 1) % 3;
		absExpiration = (absExpiration + 1) % NUMBER_OF_EXPIRATIONS;
	}

	for (boost::uint32_t i = 0; i < 100; ++i) {
		
		const boost::uint32_t segmentId = bsm.GetBundle(availableDestLinks);
		
	}

	return true;
}
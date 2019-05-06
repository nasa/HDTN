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
	segmentIdVec.push_back(segmentId);
	////segmentIdVec.shrink_to_fit();
#endif
}

bool BundleStorageManager::UnitTest() {
	static const std::string DEST_LINKS[10] = { "a1", "a2","a3", "a4", "a5", "a6", "a7", "a8", "a9", "b1" };
	BundleStorageManager bsm;
	MemoryManagerTreeArray mmt;
	mmt.SetupTree();
	for (int i = 0; i < 10; ++i) {
		bsm.AddLink(DEST_LINKS[i]);
	}

	unsigned int linkId = 0;
	unsigned int priorityIndex = 0;
	abs_expiration_t absExpiration = 0;

	for (boost::uint32_t i = 0; i < 16777216 * 64; ++i) {
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

	return true;
}
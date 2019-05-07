#include <iostream>
#include <string>
#include "MemoryManagerTree.h"
#include "MemoryManagerTreeArray.h"
#include <map>
#include <vector>
#include <boost/integer.hpp>
#include <stdint.h>
#include "BundleStorageManager.h"


int main() {
	// 64^4 = 16,777,216
	// 16777216 * 64 = 1,073,741,824
	// 4e12 / 8192 = 488,281,250
#if 0
	//1GB per loop (64 total)
	std::map<boost::uint64_t, boost::uint32_t> myMap;
	MemoryManagerTree t;
	t.SetupTree();
	getchar();
	for (boost::uint64_t i = 0; i < 16777216 * 64; ++i) {
		const boost::uint32_t segmentId = t.GetAndSetFirstFreeSegmentId();
		myMap[i] = segmentId;
		if (i % 16777216 == 0) {
			std::cout << "pause at " << i << "\n";
			getchar();
		}
	}
#elif 0
	//0.8GB per loop
	std::map<boost::uint32_t, boost::uint32_t> myMap;
	MemoryManagerTree t;
	t.SetupTree();
	getchar();
	for (boost::uint64_t i = 0; i < 16777216 * 64; ++i) {
		const boost::uint32_t segmentId = t.GetAndSetFirstFreeSegmentId();
		myMap[i] = segmentId;
		if (i % 16777216 == 0) {
			std::cout << "pause at " << i << "\n";
			getchar();
		}
	}
#elif 0
//0.8GB per loop
	std::multimap<boost::uint16_t, boost::uint32_t> myMap;
	MemoryManagerTree t;
	t.SetupTree();
	getchar();
	for (boost::uint64_t i = 0; i < 16777216 * 64; ++i) {
		const boost::uint32_t segmentId = t.GetAndSetFirstFreeSegmentId();
		const boost::uint16_t key = i % 40000;
		myMap.insert(std::pair< boost::uint16_t, boost::uint32_t>(key, segmentId));
		if (i % 16777216 == 0) {
			std::cout << "pause at " << i << "\n";
			getchar();
		}
	}
#elif 1
	//sizeof(std::vector<int>); 24B * 65536 = 1.57MB per vector of vectors
	//sizeof(std::vector<char>);
	//std::cout << BundleStorageManager::UnitTest() << "\n";
	std::cout << BundleStorageManager::TimeRandomReadsAndWrites() << "\n";
#else
	//std::cout << MemoryManagerTree::UnitTest() << "\n";
	std::cout << MemoryManagerTreeArray::UnitTest() << "\n";
#endif
	
	return 0;
}

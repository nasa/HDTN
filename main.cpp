#include <iostream>
#include <string>
#include "MemoryManagerTree.h"
#include "MemoryManagerTreeArray.h"
#include <map>
#include <boost/integer.hpp>
#include <stdint.h>



int main() {
	// 64^4 = 16,777,216
	// 16777216 * 64 = 1,073,741,824
	// 4e12 / 8192 = 488,281,250
#if 0
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
#else
	//std::cout << MemoryManagerTree::UnitTest() << "\n";
	std::cout << MemoryManagerTreeArray::UnitTest() << "\n";
#endif
	
	return 0;
}

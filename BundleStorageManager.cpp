#include "BundleStorageManager.h"
#include <iostream>
#include <string>



void BundleStorageManager::AddLink(const std::string & linkName) {
	priority_vec_t priorityVec(NUMBER_OF_PRIORITIES);
	for (unsigned int i = 0; i < NUMBER_OF_PRIORITIES; ++i) {
		priorityVec[i] = expiration_circular_buf_t(NUMBER_OF_EXPIRATIONS);
	}
	m_destMap[linkName] = std::move(priorityVec);
}

bool BundleStorageManager::UnitTest() {
	BundleStorageManager bsm;
	for (char i = 0; i < 10; ++i) {
		std::string key("mykey");
		key += 'a' + i;		
		std::cout << "about to add key " << key << "\n";
		getchar();
		bsm.AddLink(key); //results in about 12MB per link
	}

	return true;
}
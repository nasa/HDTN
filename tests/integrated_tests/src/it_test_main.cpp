#include <iostream>
#include <fstream>
#include <gtest/gtest.h>

int main(int ac, char*av[]) {

	std::cout << "Running Integrated Tests. " << std::endl << std::flush;
	
	testing::InitGoogleTest(&ac, av);
    int valIntegratedTests = RUN_ALL_TESTS();
    return valIntegratedTests;

}

#include <gtest/gtest.h>

#include <fstream>
#include <iostream>

int main(int ac, char* av[]) {
    std::cout << "Running Integrated Tests. " << std::endl << std::flush;
    testing::InitGoogleTest(&ac, av);
    int valIntegratedTest = RUN_ALL_TESTS();
    return valIntegratedTest;
}

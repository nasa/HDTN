#include <gtest/gtest.h>

#include <fstream>
#include <iostream>

int main(int ac, char* av[]) {
    std::cout << "Running Unit Tests. " << std::endl << std::flush;

    testing::InitGoogleTest(&ac, av);
    int valUnitTests = RUN_ALL_TESTS();
    return valUnitTests;
}

#include <boost/test/unit_test.hpp>
#include "CpuFlagDetection.h"
#include <iostream>
#include <string>
#include <inttypes.h>
#include <vector>


BOOST_AUTO_TEST_CASE(CpuFlagDetectionTestCase)
{

    BOOST_REQUIRE_GT(CpuFlagDetection::GetCpuVendor().length(), 1);
    BOOST_REQUIRE_GT(CpuFlagDetection::GetCpuBrand().length(), 1);
    BOOST_REQUIRE_GT(CpuFlagDetection::GetCpuFlagsCommaSeparated().length(), 10);
    //std::cout << "vendor: " << CpuFlagDetection::GetCpuVendor() << "\n";
    //std::cout << "brand: " << CpuFlagDetection::GetCpuBrand() << "\n";
    //std::cout << "flags: " << CpuFlagDetection::GetCpuFlagsCommaSeparated() << "\n";
}

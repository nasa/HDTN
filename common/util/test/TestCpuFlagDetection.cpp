/**
 * @file TestCpuFlagDetection.cpp
 * @author  Brian Tomko <brian.j.tomko@nasa.gov>
 *
 * @copyright Copyright (c) 2021 United States Government as represented by
 * the National Aeronautics and Space Administration.
 * No copyright is claimed in the United States under Title 17, U.S.Code.
 * All Other Rights Reserved.
 *
 * @section LICENSE
 * Released under the NASA Open Source Agreement (NOSA)
 * See LICENSE.md in the source root directory for more information.
 */

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

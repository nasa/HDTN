/**
 * @file TestStorageRunner.cpp
 * @author  Tad Kollar <tad.kollar@nasa.gov>
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
#include "BundleStorageCatalog.h"
#include <iostream>
#include <string>
#include "StartStorageRunner.h"

BOOST_AUTO_TEST_CASE(StorageRunnerMain)
{
    BOOST_REQUIRE_EQUAL(startStorageRunner(0, 0),0);
}

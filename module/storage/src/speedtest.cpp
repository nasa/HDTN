/***************************************************************************
 * NASA Glenn Research Center, Cleveland, OH
 * Released under the NASA Open Source Agreement (NOSA)
 * May  2021
 *
 ****************************************************************************
*/

#include <iostream>
#include <string>
#include "BundleStorageManagerMT.h"
#include "BundleStorageManagerAsio.h"
#include <boost/make_unique.hpp>

int main() {
    std::unique_ptr<BundleStorageManagerBase> bsmPtr;
    if (true) {
        boost::make_unique<BundleStorageManagerMT>();
    }
    else {
        boost::make_unique<BundleStorageManagerAsio>();
    }
    std::cout << BundleStorageManagerMT::TestSpeed(*bsmPtr) << "\n";
    return 0;
}

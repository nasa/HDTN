/**
 * @file Telemetry.cpp
 * @author  Blake LaFuente
 *
 * @copyright Copyright � 2021 United States Government as represented by
 * the National Aeronautics and Space Administration.
 * No copyright is claimed in the United States under Title 17, U.S.Code.
 * All Other Rights Reserved.
 *
 * @section LICENSE
 * Released under the NASA Open Source Agreement (NOSA)
 * See LICENSE.md in the source root directory for more information.
 */

#include "Telemetry.h"
#include <boost/endian/conversion.hpp>

void IngressTelemetry_t::ToLittleEndianInplace() {
    //boost::endian::native_to_little_inplace(averageDataRate);
    //boost::endian::native_to_little_inplace(bundleDataRate);
    //boost::endian::native_to_little_inplace(totalData);
    boost::endian::native_to_little_inplace(bundleCountEgress);
    boost::endian::native_to_little_inplace(bundleCountStorage);
}

void IngressTelemetry_t::ToNativeEndianInplace() {
    //boost::endian::little_to_native_inplace(bundleDataRate);
    //boost::endian::little_to_native_inplace(averageDataRate);
    //boost::endian::little_to_native_inplace(totalData);
    boost::endian::little_to_native_inplace(bundleCountEgress);
    boost::endian::little_to_native_inplace(bundleCountStorage);
}

void EgressTelemetry_t::ToLittleEndianInplace(){
    boost::endian::native_to_little_inplace(egressBundleCount);
    //boost::endian::native_to_little_inplace(egressBundleData);
    boost::endian::native_to_little_inplace(egressMessageCount);
}

void EgressTelemetry_t::ToNativeEndianInplace(){
    boost::endian::little_to_native_inplace(egressBundleCount);
    //boost::endian::little_to_native_inplace(egressBundleData);
    boost::endian::little_to_native_inplace(egressMessageCount);
}

void StorageTelemetry_t::ToLittleEndianInplace(){
    boost::endian::native_to_little_inplace(totalBundlesErasedFromStorage);
    boost::endian::native_to_little_inplace(totalBundlesSentToEgressFromStorage);
}

void StorageTelemetry_t::ToNativeEndianInplace(){
    boost::endian::little_to_native_inplace(totalBundlesErasedFromStorage);
    boost::endian::little_to_native_inplace(totalBundlesSentToEgressFromStorage);
}
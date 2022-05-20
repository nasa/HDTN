/**
 * @file Telemetry.cpp
 * @author  Blake LaFuente
 *
 * @copyright Copyright © 2021 United States Government as represented by
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
    boost::endian::native_to_little_inplace(data1);
}
void IngressTelemetry_t::ToNativeEndianInplace() {
    boost::endian::little_to_native_inplace(data1);
}

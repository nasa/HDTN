/**
 * @file CpuFlagDetection.h
 * @author  Brian Tomko <brian.j.tomko@nasa.gov>
 *
 * @copyright Copyright © 2021 United States Government as represented by
 * the National Aeronautics and Space Administration.
 * No copyright is claimed in the United States under Title 17, U.S.Code.
 * All Other Rights Reserved.
 *
 * @section LICENSE
 * Released under the NASA Open Source Agreement (NOSA)
 * See LICENSE.md in the source root directory for more information.
 *
 * @section DESCRIPTION
 *
 * This CpuFlagDetection class file is used for detecting the current processor's CPU flags
 * to determine which CPU instructions are supported.  It is cross platform.
 * It is used by this project's CMake build system when compiling locally (not cross-compiling)
 * to determine if any hardware accelerated functions should be compiled in.
 * For more information, see the example from https://docs.microsoft.com/en-us/cpp/intrinsics/cpuid-cpuidex?view=msvc-170 which this code is based on.
 * Also see https://www.boost.org/doc/libs/master/boost/beast/core/detail/cpu_info.hpp from which the cross platform code idea is based on.
 */

#ifndef _CPU_FLAG_DETECTION_H
#define _CPU_FLAG_DETECTION_H 1
#include <string>
#include "hdtn_util_export.h"

class HDTN_UTIL_EXPORT CpuFlagDetection {
public:
    static std::string GetCpuFlagsCommaSeparated();
    static std::string GetCpuVendor();
    static std::string GetCpuBrand();
};
#endif      // _CPU_FLAG_DETECTION_H 

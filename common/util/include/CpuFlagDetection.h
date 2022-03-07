//Implementation of https://datatracker.ietf.org/doc/html/rfc8949 for CBOR unsigned integers only
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

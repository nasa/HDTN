/**
 * @file Utf8Paths.h
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
 * This class allows conversion between UTF-8 and boost::filesystem::path.
 * The conversions are no-op on Linux, but perform conversion on Windows.
 */

#ifndef _UTF8_PATHS_H
#define _UTF8_PATHS_H 1
#include <stdlib.h>
#include <stdint.h>
#include <vector>
#include <string>
#include <boost/filesystem/path.hpp>
#include "hdtn_util_export.h"

class HDTN_UTIL_EXPORT Utf8Paths {
public:
    static std::string PathToUtf8String(const boost::filesystem::path& p);
    static boost::filesystem::path Utf8StringToPath(const std::string& u8String);
    static bool IsAscii(const std::string& u8String);
};
#endif      // _UTF8_PATHS_H 

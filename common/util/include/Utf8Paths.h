/**
 * @file Utf8Paths.h
 * @author  Brian Tomko <brian.j.tomko@nasa.gov>
 *
 * @copyright Copyright (c) 2026 United States Government as represented by
 * the National Aeronautics and Space Administration.
 * No copyright is claimed in the United States under Title 17, U.S.Code.
 * All Other Rights Reserved.
 *
 * @section LICENSE
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
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

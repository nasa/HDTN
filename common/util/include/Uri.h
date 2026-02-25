/**
 * @file Uri.h
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
 *
 * This Uri static class provides methods to convert between endpoint ID nodeNumber/serviceNumber pairs
 * and IPN URI strings.
 */

#ifndef URI_H
#define URI_H 1

#include <string>
#include <cstdint>
#include "hdtn_util_export.h"

class HDTN_UTIL_EXPORT Uri {
private:
    Uri();
    ~Uri();

public:
    static std::string GetIpnUriString(const uint64_t eidNodeNumber, const uint64_t eidServiceNumber);
    static std::string GetIpnUriStringAnyServiceNumber(const uint64_t eidNodeNumber);

    //return 0 on failure, or bytesWritten including the null character
    static std::size_t WriteIpnUriCstring(const uint64_t eidNodeNumber, const uint64_t eidServiceNumber, char * buffer, std::size_t maxBufferSize);

    static bool ParseIpnUriString(const std::string & uri, uint64_t & eidNodeNumber, uint64_t & eidServiceNumber, bool* serviceNumberIsWildCard = NULL);
    static bool ParseIpnUriCstring(const char * data, uint64_t bufferSize, uint64_t & bytesDecodedIncludingNullChar, uint64_t & eidNodeNumber, uint64_t & eidServiceNumber, bool* serviceNumberIsWildCard = NULL);

    //parse just the scheme specific part
    static bool ParseIpnSspString(std::string::const_iterator stringSspCbegin, std::string::const_iterator stringSspCend, uint64_t & eidNodeNumber, uint64_t & eidServiceNumber);
    static bool ParseIpnSspString(const char * data, std::size_t length, uint64_t & eidNodeNumber, uint64_t & eidServiceNumber, bool * serviceNumberIsWildCard = NULL); //more efficient (no string copies)

    //more efficient method of boost::lexical_cast<std::string>(val).length();
    static uint64_t GetStringLengthOfUint(const uint64_t val);

    static uint64_t GetIpnUriCstringLengthRequiredIncludingNullTerminator(const uint64_t eidNodeNumber, const uint64_t eidServiceNumber);
};

#endif //URI_H


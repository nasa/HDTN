/**
 * @file Uri.h
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

    static bool ParseIpnUriString(const std::string & uri, uint64_t & eidNodeNumber, uint64_t & eidServiceNumber);
    static bool ParseIpnUriCstring(const char * data, uint64_t bufferSize, uint64_t & bytesDecodedIncludingNullChar, uint64_t & eidNodeNumber, uint64_t & eidServiceNumber);

    //parse just the scheme specific part
    static bool ParseIpnSspString(std::string::const_iterator stringSspCbegin, std::string::const_iterator stringSspCend, uint64_t & eidNodeNumber, uint64_t & eidServiceNumber);
    static bool ParseIpnSspString(const char * data, std::size_t length, uint64_t & eidNodeNumber, uint64_t & eidServiceNumber); //more efficient (no string copies)

    //more efficient method of boost::lexical_cast<std::string>(val).length();
    static uint64_t GetStringLengthOfUint(const uint64_t val);

    static uint64_t GetIpnUriCstringLengthRequiredIncludingNullTerminator(const uint64_t eidNodeNumber, const uint64_t eidServiceNumber);
};

#endif //URI_H


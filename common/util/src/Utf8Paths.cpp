/**
 * @file Utf8Paths.cpp
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
 */

#include "Utf8Paths.h"
#include "Logger.h"

#ifdef _WIN32
#include <Windows.h>
//https://stackoverflow.com/a/69410299
static std::wstring Utf8StringToNativeString(const std::string& string) { //Utf8StringToWideString
    if (string.empty()) {
        return L"";
    }

    //last param lpWideCharStr. If this value is 0, the function returns the required buffer size,
    // in characters, including any terminating null character, and makes no use of the 2nd to last param lpWideCharStr buffer.
    const int size_needed = MultiByteToWideChar(CP_UTF8, 0, &string.at(0), (int)string.size(), nullptr, 0);
    if (size_needed <= 0) {
        throw std::runtime_error("MultiByteToWideChar() failed: " + std::to_string(size_needed));
    }

    std::wstring result(size_needed, 0);
    MultiByteToWideChar(CP_UTF8, 0, &string.at(0), (int)string.size(), &result.at(0), size_needed);
    return result;
}

static std::string NativeStringToUtf8String(const std::wstring& wide_string) { //WideStringToUtf8String
    if (wide_string.empty()) {
        return "";
    }

    //cbMultiByte (6th param), If this value is 0, the function returns the required buffer size,
    //in bytes, including any terminating null character, and makes no use of the lpMultiByteStr buffer.
    const int size_needed = WideCharToMultiByte(CP_UTF8, 0, &wide_string.at(0), (int)wide_string.size(), nullptr, 0, nullptr, nullptr);
    if (size_needed <= 0) {
        throw std::runtime_error("WideCharToMultiByte() failed: " + std::to_string(size_needed));
    }

    //In C++11 and later, mystring.c_str() is equivalent to mystring.data() is equivalent to &mystring[0],
    // and mystring[mystring.size()] is guaranteed to be '\0'.
    std::string result(size_needed, 0);

    WideCharToMultiByte(CP_UTF8, 0, &wide_string.at(0), (int)wide_string.size(), &result.at(0), size_needed, nullptr, nullptr);
    return result;
}
#else
static std::string Utf8StringToNativeString(const std::string& doNothingStr) { //Utf8StringToWideString
    return doNothingStr;
}
#endif //_WIN32
static std::string NativeStringToUtf8String(const std::string& doNothingStr) { //overload for non-wide OS
    return doNothingStr;
}

std::string Utf8Paths::PathToUtf8String(const boost::filesystem::path& p) {
    return std::string(NativeStringToUtf8String(p.native()));
}
boost::filesystem::path Utf8Paths::Utf8StringToPath(const std::string& u8String) {
    return boost::filesystem::path(Utf8StringToNativeString(u8String));
}

bool Utf8Paths::IsAscii(const std::string& u8String) {
    return std::all_of(u8String.begin(), u8String.end(), ::isprint);
}

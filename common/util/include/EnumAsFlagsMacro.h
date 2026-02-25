/**
 * @file EnumAsFlagsMacro.h
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
 * This EnumAsFlagsMacro include file is used for giving strongly-typed enums (new in C++11)
 * inlined bitwise operators and the ostream operator.
 * For more information, see http://www.cplusplus.com/forum/general/44137/ from which this code is based on.
 */

#ifndef _ENUM_AS_FLAGS_MACRO_H
#define _ENUM_AS_FLAGS_MACRO_H 1
#include <stdlib.h>
#include <stdint.h>
#include <type_traits>
#include <boost/config/detail/suffix.hpp>
#include <ostream>

//note: static_assert(true, "") is to require a semicolon after the macro to eliminate warnings when -Wpedantic is enabled as a compiler option
#define MAKE_ENUM_SUPPORT_FLAG_OPERATORS(ENUMTYPE) \
BOOST_FORCEINLINE ENUMTYPE operator | (ENUMTYPE a, ENUMTYPE b) { return static_cast<ENUMTYPE>((static_cast<std::underlying_type<ENUMTYPE>::type>(a)) | (static_cast<std::underlying_type<ENUMTYPE>::type>(b))); } \
BOOST_FORCEINLINE ENUMTYPE &operator |= (ENUMTYPE &a, ENUMTYPE b) { return (ENUMTYPE &)(((std::underlying_type<ENUMTYPE>::type &)(a)) |= (static_cast<std::underlying_type<ENUMTYPE>::type>(b))); } \
BOOST_FORCEINLINE ENUMTYPE operator & (ENUMTYPE a, ENUMTYPE b) { return static_cast<ENUMTYPE>((static_cast<std::underlying_type<ENUMTYPE>::type>(a)) & (static_cast<std::underlying_type<ENUMTYPE>::type>(b))); } \
BOOST_FORCEINLINE ENUMTYPE &operator &= (ENUMTYPE &a, ENUMTYPE b) { return (ENUMTYPE &)(((std::underlying_type<ENUMTYPE>::type &)(a)) &= (static_cast<std::underlying_type<ENUMTYPE>::type>(b))); } \
BOOST_FORCEINLINE ENUMTYPE operator ~ (ENUMTYPE a) { return static_cast<ENUMTYPE>(~(static_cast<std::underlying_type<ENUMTYPE>::type>(a))); } \
BOOST_FORCEINLINE ENUMTYPE operator ^ (ENUMTYPE a, ENUMTYPE b) { return static_cast<ENUMTYPE>((static_cast<std::underlying_type<ENUMTYPE>::type>(a)) ^ (static_cast<std::underlying_type<ENUMTYPE>::type>(b))); } \
BOOST_FORCEINLINE ENUMTYPE &operator ^= (ENUMTYPE &a, ENUMTYPE b) { return (ENUMTYPE &)(((std::underlying_type<ENUMTYPE>::type &)(a)) ^= (static_cast<std::underlying_type<ENUMTYPE>::type>(b))); } static_assert(true, "")

#define MAKE_ENUM_SUPPORT_OSTREAM_OPERATOR(ENUMTYPE) \
BOOST_FORCEINLINE std::ostream& operator<<(std::ostream& os, const ENUMTYPE & a) { os << std::hex << "0x" << (static_cast<uint64_t>((std::underlying_type<ENUMTYPE>::type &)(a))) << std::dec; return os; } static_assert(true, "")


#endif      // _ENUM_AS_FLAGS_MACRO_H 

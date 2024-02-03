/**
 * @file EnumAsFlagsMacro.h
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

//note: static_assert(true, "") is to require a semicolon after the macro to eliminate warnings when -Wpedantic is enabled as a compiler warning
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

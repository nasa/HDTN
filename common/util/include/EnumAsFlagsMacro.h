#ifndef _ENUM_AS_FLAGS_MACRO_H
#define _ENUM_AS_FLAGS_MACRO_H 1
#include <stdlib.h>
#include <stdint.h>
#include <type_traits>
#include <boost/config/detail/suffix.hpp>
#include <ostream>

//http://www.cplusplus.com/forum/general/44137/

#define MAKE_ENUM_SUPPORT_FLAG_OPERATORS(ENUMTYPE) \
BOOST_FORCEINLINE ENUMTYPE operator | (ENUMTYPE a, ENUMTYPE b) { return static_cast<ENUMTYPE>((static_cast<std::underlying_type<ENUMTYPE>::type>(a)) | (static_cast<std::underlying_type<ENUMTYPE>::type>(b))); } \
BOOST_FORCEINLINE ENUMTYPE &operator |= (ENUMTYPE &a, ENUMTYPE b) { return (ENUMTYPE &)(((std::underlying_type<ENUMTYPE>::type &)(a)) |= (static_cast<std::underlying_type<ENUMTYPE>::type>(b))); } \
BOOST_FORCEINLINE ENUMTYPE operator & (ENUMTYPE a, ENUMTYPE b) { return static_cast<ENUMTYPE>((static_cast<std::underlying_type<ENUMTYPE>::type>(a)) & (static_cast<std::underlying_type<ENUMTYPE>::type>(b))); } \
BOOST_FORCEINLINE ENUMTYPE &operator &= (ENUMTYPE &a, ENUMTYPE b) { return (ENUMTYPE &)(((std::underlying_type<ENUMTYPE>::type &)(a)) &= (static_cast<std::underlying_type<ENUMTYPE>::type>(b))); } \
BOOST_FORCEINLINE ENUMTYPE operator ~ (ENUMTYPE a) { return static_cast<ENUMTYPE>(~(static_cast<std::underlying_type<ENUMTYPE>::type>(a))); } \
BOOST_FORCEINLINE ENUMTYPE operator ^ (ENUMTYPE a, ENUMTYPE b) { return static_cast<ENUMTYPE>((static_cast<std::underlying_type<ENUMTYPE>::type>(a)) ^ (static_cast<std::underlying_type<ENUMTYPE>::type>(b))); } \
BOOST_FORCEINLINE ENUMTYPE &operator ^= (ENUMTYPE &a, ENUMTYPE b) { return (ENUMTYPE &)(((std::underlying_type<ENUMTYPE>::type &)(a)) ^= (static_cast<std::underlying_type<ENUMTYPE>::type>(b))); }

#define MAKE_ENUM_SUPPORT_OSTREAM_OPERATOR(ENUMTYPE) \
BOOST_FORCEINLINE std::ostream& operator<<(std::ostream& os, const ENUMTYPE & a) { os << std::hex << "0x" << ((std::underlying_type<ENUMTYPE>::type &)(a)) << std::dec; return os; }


#endif      // _ENUM_AS_FLAGS_MACRO_H 

# @file hardware_acceleration_neon.cmake
# @author  Brian Tomko <brian.j.tomko@nasa.gov>
#
# @copyright Copyright (c) 2021 United States Government as represented by
# the National Aeronautics and Space Administration.
# No copyright is claimed in the United States under Title 17, U.S.Code.
# All Other Rights Reserved.
#
# @section LICENSE
# Released under the NASA Open Source Agreement (NOSA)
# See LICENSE.md in the source root directory for more information.
#
# @section DESCRIPTION
#
# This script detects if the compiler supports specific arm64 hardware acceleration compiler intrinsics from sse2neon.h
# The project for sse2neon.h is located at https://github.com/DLTcollab/sse2neon

SET(CMAKE_REQUIRED_FLAGS "") #clear out any existing flags
if(LTP_RNG_USE_RDSEED)
	message(FATAL_ERROR "LTP_RNG_USE_RDSEED was set but is not supported on ARM64. Please set to off.")
endif()
if(USE_X86_HARDWARE_ACCELERATION)
	if(EXISTS "${CMAKE_SOURCE_DIR}/third_party_include/sse2neon.h")
		message("found existing ${CMAKE_SOURCE_DIR}/third_party_include/sse2neon.h .. not redownloading")
	else()
		message("sse2neon library needs to be located in ${CMAKE_SOURCE_DIR}/third_party_include/sse2neon.h for ARM64 hardware acceleration.. will attempt to download.")
		message("downloading https://raw.githubusercontent.com/DLTcollab/sse2neon/master/sse2neon.h")
		file(MAKE_DIRECTORY "${CMAKE_SOURCE_DIR}/third_party_include")
		file(DOWNLOAD "https://raw.githubusercontent.com/DLTcollab/sse2neon/master/sse2neon.h" "${CMAKE_SOURCE_DIR}/third_party_include/sse2neon.h")
		message("successfully downloaded to ${CMAKE_SOURCE_DIR}/third_party_include/sse2neon.h")
	endif()
	SET(CMAKE_REQUIRED_FLAGS  "-I${CMAKE_SOURCE_DIR}/third_party_include -march=armv8-a+fp+simd+crypto+crc")
	check_include_file("sse2neon.h" HAVE_SSE2NEON_H)
	if(NOT HAVE_SSE2NEON_H)
        message(FATAL_ERROR "USE_X86_HARDWARE_ACCELERATION was set but compiler doesn't have sse2neon.h.. you must unset USE_X86_HARDWARE_ACCELERATION as it won't work on this machine")
    endif()

	check_cxx_source_compiles("
		#include <sse2neon.h>
		#include <cstdint>
		int main() {
			uint8_t data[32];
			_mm_stream_si32((int32_t *)data, 0x123456);
			_mm_stream_si64((int64_t*)data, 0x1234567891011u);
			{
				const __m128i enc = _mm_castps_si128(_mm_load_ss((float const*)data)); //Load a single-precision (32-bit) floating-point element from memory into the lower of dst, and zero the upper 3 elements. mem_addr does not need to be aligned on any particular boundary.
				const uint32_t result32Be = _mm_cvtsi128_si32(enc); //SSE2 Copy the lower 32-bit integer in a to dst.
			}
			{
				const __m128i enc = _mm_castpd_si128(_mm_load_sd((double const*)data)); //Load a double-precision (64-bit) floating-point element from memory into the lower of dst, and zero the upper element. mem_addr does not need to be aligned on any particular boundary.
				const uint64_t result64Be = _mm_cvtsi128_si64(enc); //SSE2 Copy the lower 64-bit integer in a to dst.
			}
			return 0;
		}" USE_CBOR_FAST)


	check_cxx_source_compiles("
		#include <sse2neon.h>
		#include <cstdint>
		int main() {
			uint64_t crc = UINT32_MAX;
			crc = _mm_crc32_u8(static_cast<uint32_t>(crc), 5);
			crc = _mm_crc32_u64(crc, 0x123456789abcdU);
			return 0;
		}" USE_CRC32C_FAST)

	if(USE_CBOR_FAST AND USE_CRC32C_FAST)
		message("adding compile definition: USE_CBOR_FAST (cpu supports SSE and SSE2)")
		add_compile_definitions(USE_CBOR_FAST)
		message("adding compile definition: USE_SSE_SSE2 (cpu supports SSE and SSE2) (used by TCPCLv4)") #for tcpclv4 loads and stores
		add_compile_definitions(USE_SSE_SSE2)
		message("adding compile definition: USE_CRC32C_FAST (cpu supports SSE4.2)")
		add_compile_definitions(USE_CRC32C_FAST)
		message("adding compile definition: HAVE_SSE2NEON_H")
		add_compile_definitions(HAVE_SSE2NEON_H)
		list(APPEND COMPILE_DEFINITIONS_TO_EXPORT USE_CBOR_FAST) #used in CborUint.h (should be fixed, but for now the package config must export it)
		list(APPEND COMPILE_DEFINITIONS_TO_EXPORT USE_CRC32C_FAST) #used in Bpv7Crc.h (should be fixed, but for now the package config must export it)
		list(APPEND COMPILE_DEFINITIONS_TO_EXPORT HAVE_SSE2NEON_H) #used in headers (should be fixed, but for now the package config must export it)
		list(APPEND NON_WINDOWS_HARDWARE_ACCELERATION_FLAGS "-march=armv8-a+fp+simd+crypto+crc")
	else()
		message(FATAL_ERROR "USE_X86_HARDWARE_ACCELERATION was set but compiler was unable to compile using sse2neon.h.. you must unset USE_X86_HARDWARE_ACCELERATION as it won't work on this machine")
	endif()
	
endif()

SET(CMAKE_REQUIRED_FLAGS "") #clear out any existing flags

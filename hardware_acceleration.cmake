
if(USE_X86_HARDWARE_ACCELERATION)
	#check_include_file("immintrin.h" HAVE_IMMINTRIN_H)
	#check_include_file("x86intrin.h" HAVE_X86INTRIN_H)
	#check_include_file("nmmintrin.h" HAVE_NMMINTRIN_H) #crc32
	#check_symbol_exists("_pdep_u64" "immintrin.h" HAVE_pdep_u64) #BMI2
	#check_symbol_exists(_pext_u64 "immintrin.h" HAVE_pext_u64) #BMI2
    #check_symbol_exists(_mm_crc32_u64 "nmmintrin.h" HAVE_mm_crc32_u64) #sse4.2
	#check_symbol_exists(_mm_crc32_u8 "nmmintrin.h" HAVE_mm_crc32_u8) #sse4.2
    #check_symbol_exists(_bittest64 "immintrin.h" HAVE_bittest64)
    #check_symbol_exists(_bittestandreset64 "immintrin.h" HAVE_bittestandreset64)
    #check_symbol_exists(__andn_u64 "x86intrin.h" HAVE__andn_u64) #gnu c only and BMI1 
	check_cxx_source_compiles("
		#include <immintrin.h>
		#include <emmintrin.h>
		#include <smmintrin.h>
		#include <intrin.h>
		#include <cstdint>
		int main() {
			uint8_t data[32];
			{
				const uint64_t encoded64 = _pdep_u64(5, 0x7f);
				_mm_stream_si64((long long int *)(data), encoded64);
				const uint64_t mask1 = _bzhi_u64(0x7f7f7f7f7f7f7f7f, 16);
			}
			{
				__m128i sdnvEncoded = _mm_loadl_epi64((__m128i const*)(data)); //_mm_loadu_si64(data); //SSE Load unaligned 64-bit integer from memory into the first element of dst.
				int significantBitsSetMask = _mm_movemask_epi8(sdnvEncoded);//SSE2 most significant bit of the corresponding packed 8-bit integer in a. //_mm_movepi8_mask(sdnvEncoded); 
				const uint64_t encoded64 = _mm_cvtsi128_si64(sdnvEncoded); //SSE2 Copy the lower 64-bit integer in a to dst.
				const uint64_t decoded = _pext_u64(encoded64, 0x7f);
			}
			{
				__m128i sdnvEncodedSse3 = _mm_lddqu_si128((__m128i const*) data); //SSE3 Load 128-bits of integer data from unaligned memory into dst. This intrinsic may perform better than _mm_loadu_si128 when the data crosses a cache line boundary.
				__m128i sdnvEncoded = _mm_loadu_si128((__m128i const*) data); //SSE2 Load 128-bits of integer data from memory into dst. mem_addr does not need to be aligned on any particular boundary.
				const uint64_t mask2 = _bextr_u64(0x7f7f, 64, 16);
				const uint8_t shift = static_cast<uint8_t>(_mm_popcnt_u64(mask2));
				sdnvEncoded = _mm_srli_si128(sdnvEncoded, 8);
			}
			{
		#ifdef _MSC_VER
				__declspec(align(16))
		#endif
					static const uint8_t SHUFFLE_SHR_128I[1 * sizeof(__m128i)]
		#ifndef _MSC_VER
					__attribute__((aligned(16)))
		#endif
					=
				{
					//little endian reverse order (LSB first)
					0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f //shr 0
				};
				__m128i mySet = _mm_setzero_si128();
				_mm_storeu_si64(data, mySet);//SSE Store 64-bit integer from the first element of a into memory. mem_addr does not need to be aligned on any particular boundary.
				const __m128i shuffleControlMask = _mm_stream_load_si128((__m128i*) &SHUFFLE_SHR_128I[0]); //SSE4.1 requires alignment
				mySet = _mm_shuffle_epi8(mySet, shuffleControlMask);
			}
			return 0;
		}" USE_SDNV_FAST)
	check_cxx_source_compiles("
		#include <immintrin.h>
		#include <emmintrin.h>
		#include <smmintrin.h>
		#include <intrin.h>
		#include <cstdint>
		int main() {
			uint8_t data[32];
			__m256i sdnvsEncoded = _mm256_loadu_si256((__m256i const*) data); //AVX Load 256-bits of integer data from memory into dst. mem_addr does not need to be aligned on any particular boundary.
			int significantBitsSetMask = _mm256_movemask_epi8(sdnvsEncoded);//SSE2 most significant bit of the corresponding packed 8-bit integer in a. //_mm_movepi8_mask(sdnvEncoded); 
			const uint64_t encoded64 = _mm_cvtsi128_si64(_mm256_castsi256_si128(sdnvsEncoded)); //SSE2 Copy the lower 64-bit integer in a to dst.
			const uint64_t encoded64High = _mm_cvtsi128_si64(_mm256_castsi256_si128(_mm256_srli_si256(sdnvsEncoded, 8))); //SSE2 Copy the lower 64-bit integer in a to dst.
			const __m256i shiftIn = _mm256_permute2f128_si256(_mm256_setzero_si256(), sdnvsEncoded, 0x03);
			sdnvsEncoded = _mm256_alignr_epi8(shiftIn, sdnvsEncoded, 1);
			return 0;
		}" SDNV_SUPPORT_AVX2_FUNCTIONS)
	check_cxx_source_compiles("
		#include <immintrin.h>
		#include <emmintrin.h>
		#include <smmintrin.h>
		#include <intrin.h>
		#include <cstdint>
		int main() {
			uint8_t data[32];
			_mm_stream_si32((int32_t *)data, 0x123456);
			_mm_stream_si64((long long int *)data, 0x1234567891011u);
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
		#include <nmmintrin.h>
		#include <cstdint>
		int main() {
			uint64_t crc = UINT32_MAX;
			crc = _mm_crc32_u8(static_cast<uint32_t>(crc), 5);
			crc = _mm_crc32_u64(crc, 0x123456789abcdU);
			return 0;
		}" USE_CRC32C_FAST)
	check_cxx_source_compiles("
		#include <immintrin.h>
		#if defined(__GNUC__)
		#include <x86intrin.h>
		#define _andn_u64(a, b)   (__andn_u64((a), (b)))
		#else
		#include <ammintrin.h>
		#endif
		#include <cstdint>
		int main() {
			uint64_t bits = 0xff;
            const uint64_t mask64 = 0x01;
            bits = _andn_u64(mask64, bits);
			return 0;
		}" USE_ANDN)
	check_cxx_source_compiles("
		#include <immintrin.h>
		#include <intrin.h>
		#include <cstdint>
		int main() {
			uint64_t bits = 0xff;
			_bittestandreset64((int64_t*)&bits, 1);
			char set = _bittest64((int64_t*)&bits, 2);
			const bool bitWasAlreadyOne = _bittestandset64((int64_t*)&bits, 1);
			return 0;
		}" USE_BITTEST)
	SET(TESTING_CPU_FLAGS_LIST "POPCNT;BMI1;BMI2;SSE;SSE2;SSE3;SSSE3;SSE41;SSE42;AVX;AVX2")
	if(NOT CMAKE_CROSSCOMPILING)
		TRY_RUN(
			test_run_result # Name of variable to store the run result (process exit status; number) in:
			test_compile_result # Name of variable to store the compile result (TRUE or FALSE) in:
			${CMAKE_CURRENT_BINARY_DIR}/ # Binary directory:
			${CMAKE_CURRENT_SOURCE_DIR}/common/util/src/CpuFlagDetection.cpp # Source file to be compiled:
			COMPILE_OUTPUT_VARIABLE test_compile_output # Where to store the output produced during compilation:
			RUN_OUTPUT_VARIABLE test_run_output # Where to store the output produced by running the compiled executable:
			COMPILE_DEFINITIONS "-DCPU_FLAG_DETECTION_RUN_MAIN_ONLY"
		)
		if(NOT test_compile_result)
			message(FATAL_ERROR "failed to compile ${CMAKE_CURRENT_SOURCE_DIR}/common/util/src/CpuFlagDetection.cpp with result:\n ${test_compile_output}")
		else()
			message("compiled ${CMAKE_CURRENT_SOURCE_DIR}/common/util/src/CpuFlagDetection.cpp with result:\n ${test_compile_output}")
		endif()
		if(test_run_result EQUAL 0) #program successfully returned 0
			#message("run output: ${test_run_output}")
			message("Not Crosscompiling.  Detecting CPU features...")
			string(REGEX MATCH "VENDOR_BEGIN(.+)VENDOR_END" junk ${test_run_output})
			message("vendor: ${CMAKE_MATCH_1}")
			string(REGEX MATCH "BRAND_BEGIN(.+)BRAND_END" junk ${test_run_output})
			message("brand: ${CMAKE_MATCH_1}")
			string(REGEX MATCH "ALL_CPU_FLAGS_BEGIN(.+)ALL_CPU_FLAGS_END" junk ${test_run_output})
			string(REPLACE "," ";" CPU_FLAG_LIST ${CMAKE_MATCH_1})
			message("flags: ${CMAKE_MATCH_1}")
			list(LENGTH CPU_FLAG_LIST num_cpu_flags)
			#message("num_cpu_flags: ${num_cpu_flags}")
			
			foreach(CURRENT_FLAG IN LISTS TESTING_CPU_FLAGS_LIST)
				list(FIND CPU_FLAG_LIST ${CURRENT_FLAG} flag_index)
				if(NOT (flag_index EQUAL -1))
					message("cpu supports ${CURRENT_FLAG}")
					set("${CURRENT_FLAG}_supported" TRUE)
				else()
					message(WARNING "cpu does not support ${CURRENT_FLAG}")
					set("${CURRENT_FLAG}_supported" FALSE)
				endif()
			endforeach()
			message("BMI1_supported: ${BMI1_supported}")
			message("BMII_supported: ${BMII_supported}")
			#message("bmi1_index: ${bmi1_index}")
		else()
			message(FATAL_ERROR "failed to run ${CMAKE_CURRENT_SOURCE_DIR}/common/util/src/CpuFlagDetection.cpp")
		endif()
	else() #cross compiling (assume cpu supports instructions)
		foreach(CURRENT_FLAG IN LISTS TESTING_CPU_FLAGS_LIST)
			set("${CURRENT_FLAG}_supported" TRUE)
		endforeach()
	endif()
	#//bmi2,sse2,sse,sse41,ssse3 bmi1,popcnt (replace _mm_loadu_si128 sse3 with _mm_lddqu_si128)
	if(USE_SDNV_FAST AND SSE_supported AND SSE2_supported AND SSE3_supported AND SSSE3_supported AND SSE41_supported AND POPCNT_supported AND BMI1_supported AND BMI2_supported)
		message("adding compile definition: USE_SDNV_FAST (cpu supports SSE, SSE2, SSE3, SSSE3, SSE4.1, POPCNT, BMI1, and BMI2)")
		add_compile_definitions(USE_SDNV_FAST)
	endif()
	if(SDNV_SUPPORT_AVX2_FUNCTIONS AND AVX_supported AND AVX2_supported)
		message("adding compile definition: SDNV_SUPPORT_AVX2_FUNCTIONS (cpu supports AVX and AVX2) (but HDTN doesn't currently implement batch sdnv decode operations)")
		add_compile_definitions(SDNV_SUPPORT_AVX2_FUNCTIONS)
	endif()
	if(USE_CBOR_FAST AND SSE_supported AND SSE2_supported)
		message("adding compile definition: USE_CBOR_FAST (cpu supports SSE and SSE2)")
		add_compile_definitions(USE_CBOR_FAST)
	endif()
	if(USE_CRC32C_FAST AND SSE42_supported)
		message("adding compile definition: USE_CRC32C_FAST (cpu supports SSE4.2)")
		add_compile_definitions(USE_CRC32C_FAST)
	endif()
	if(USE_ANDN AND BMI1_supported)
		message("adding compile definition: USE_ANDN (cpu supports BMI1)")
		add_compile_definitions(USE_ANDN)
	endif()
	if(USE_BITTEST)
		message("adding compile definition: USE_BITTEST")
		add_compile_definitions(USE_BITTEST)
	endif()
    add_compile_definitions(USE_X86_HARDWARE_ACCELERATION)
    if(NOT WIN32)
        SET(CMAKE_CXX_FLAGS  "${CMAKE_CXX_FLAGS} -msse -msse2 -mbmi -mbmi2 -msse3 -mssse3 -msse4.1 -msse4.2 -mavx -mavx2")
    endif()
endif()
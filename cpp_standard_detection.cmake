# @file cpp_standard_detection.cmake
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
# This script detects if the compiler supports C++17 standard and
# sets the HDTN project to compile to with C++17 standard (if the user desires through option HDTN_TRY_USE_CPP17).
# Otherwise this script sets the HDTN project to compile with C++11 standard.
# If C++17 is selected, this script makes sure the __cplusplus define is correct for Visual Studio.

option(HDTN_TRY_USE_CPP17 "Build HDTN with C++17 standard if compiler supports it.  Otherwise fall back to C++11 standard." On)
if(HDTN_TRY_USE_CPP17)
	#from https://gitlab.kitware.com/cmake/cmake/-/issues/18837
	# /Zc:__cplusplus is required to make __cplusplus accurate
	# /Zc:__cplusplus is available starting with Visual Studio 2017 version 15.7
	# (according to https://docs.microsoft.com/en-us/cpp/build/reference/zc-cplusplus)
	# That version is equivalent to _MSC_VER==1914
	# (according to https://docs.microsoft.com/en-us/cpp/preprocessor/predefined-macros?view=vs-2019)
	# CMake's ${MSVC_VERSION} is equivalent to _MSC_VER
	# (according to https://cmake.org/cmake/help/latest/variable/MSVC_VERSION.html#variable:MSVC_VERSION)
	#if ((MSVC) AND (MSVC_VERSION GREATER_EQUAL 1914))
	#	target_compile_options(MainApp PUBLIC "/Zc:__cplusplus")
	#endif()
	if ((MSVC) AND (MSVC_VERSION GREATER_EQUAL 1914))
		SET(CMAKE_REQUIRED_FLAGS "/Zc:__cplusplus /std:c++17")
	elseif(NOT WIN32)
		SET(CMAKE_REQUIRED_FLAGS "-std=c++17")
	else()
		SET(CMAKE_REQUIRED_FLAGS "-std=c++17") #todo
	endif()

	check_cxx_source_compiles("
		#include <vector>
		#include <unordered_map>
		#include <string>
	
		int main() {
	#if (__cplusplus >= 201703L)
			//sample code used throughout hdtn

			//vector emplace_back() returning a reference is c++17
			struct TestStruct {
				int myInt;
				std::string myString;
			};
			std::vector<TestStruct> myVec;
			TestStruct & testStructVecRef = myVec.emplace_back();
			testStructVecRef.myInt = 2;
			testStructVecRef.myString = \"test\";

			//try_emplace is c++17
			typedef std::unordered_map<int, int> my_map_t;
			my_map_t myMap;
			std::pair<my_map_t::iterator, bool> pairRetvalMyMap = myMap.try_emplace(5, 6);
	#else
			force a compile error
	#endif
			return 0;
		}" COMPILER_HAS_CPP17)



	#add compile definitions for x86 hardware acceleration
	if(COMPILER_HAS_CPP17)
		message("Setting C++ standard to 17 (compiler supports C++17)")
		set(CMAKE_CXX_STANDARD 17)
		if ((MSVC) AND (MSVC_VERSION GREATER_EQUAL 1914))
			message("manually adding compile options: /Zc:__cplusplus /std:c++17 (Visual Studio compiler supports c++17)")
			add_compile_options($<$<COMPILE_LANGUAGE:CXX>:/Zc:__cplusplus> $<$<COMPILE_LANGUAGE:CXX>:/std:c++17>)	
		endif()
	else()
		message("Compiler does not support C++17 standard.  Setting C++ standard to 11")
		set(CMAKE_CXX_STANDARD 11)
	endif()
else()
	message("Option HDTN_TRY_USE_CPP17 was set to Off in CMakeCache.txt.  Setting C++ standard to 11")
	set(CMAKE_CXX_STANDARD 11)
endif()

set(CMAKE_CXX_STANDARD_REQUIRED ON)

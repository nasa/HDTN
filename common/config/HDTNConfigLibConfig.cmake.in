get_filename_component(HDTNCONFIGLIB_CMAKE_DIR "${CMAKE_CURRENT_LIST_FILE}" PATH)
include(CMakeFindDependencyMacro)

find_dependency(HDTNUtil REQUIRED)
find_dependency(LoggerLib REQUIRED)

if(NOT TARGET HDTN::HDTNConfigLib)
    include("${HDTNCONFIGLIB_CMAKE_DIR}/HDTNConfigLibTargets.cmake")
endif()

set(HDTNCONFIGLIB_LIBRARIES HDTN::HDTNConfigLib)

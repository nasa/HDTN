get_filename_component(STCPLIB_CMAKE_DIR "${CMAKE_CURRENT_LIST_FILE}" PATH)
include(CMakeFindDependencyMacro)

find_dependency(HDTNUtil REQUIRED)
find_dependency(TelemetryDefinitions REQUIRED)
find_dependency(LoggerLib REQUIRED)

if(NOT TARGET HDTN::StcpLib)
    include("${STCPLIB_CMAKE_DIR}/StcpLibTargets.cmake")
endif()

set(STCPLIB_LIBRARIES HDTN::StcpLib)

get_filename_component(UDPLIB_CMAKE_DIR "${CMAKE_CURRENT_LIST_FILE}" PATH)
include(CMakeFindDependencyMacro)

find_dependency(HDTNUtil REQUIRED)
find_dependency(LoggerLib REQUIRED)

if(NOT TARGET HDTN::UdpLib)
    include("${UDPLIB_CMAKE_DIR}/UdpLibTargets.cmake")
endif()

set(UDPLIB_LIBRARIES HDTN::UdpLib)

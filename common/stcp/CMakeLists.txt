cmake_minimum_required(VERSION 3.0)
project(stcp_lib)
set(CMAKE_CXX_STANDARD 11)



add_library(stcp_lib
	src/StcpBundleSink.cpp
	src/StcpBundleSource.cpp
)
GENERATE_EXPORT_HEADER(stcp_lib)
install(TARGETS stcp_lib DESTINATION lib)
install(FILES
	include/StcpBundleSink.h
	include/StcpBundleSource.h
	${PROJECT_BINARY_DIR}/stcp_lib_export.h
	DESTINATION include
)
target_link_libraries(stcp_lib hdtn_util ${Boost_LIBRARIES} ${pthread_LIB})
target_include_directories(stcp_lib PUBLIC include ${Boost_INCLUDE_DIRS} ${PROJECT_BINARY_DIR})



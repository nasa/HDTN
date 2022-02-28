cmake_minimum_required(VERSION 3.0)
project(tcpcl_lib)
set(CMAKE_CXX_STANDARD 11)



add_library(tcpcl_lib
    src/Tcpcl.cpp
	src/TcpclV4.cpp
	src/TcpclBundleSink.cpp
	src/TcpclBundleSource.cpp
	src/TcpclV3BidirectionalLink.cpp
	src/TcpclV4BundleSource.cpp
	src/TcpclV4BundleSink.cpp
	src/TcpclV4BidirectionalLink.cpp
)
GENERATE_EXPORT_HEADER(tcpcl_lib)
install(TARGETS tcpcl_lib DESTINATION lib)
install(FILES
	include/BidirectionalLink.h
	include/Tcpcl.h
	include/TcpclBundleSink.h
	include/TcpclBundleSource.h
	include/TcpclV3BidirectionalLink.h
	include/TcpclV4.h
	include/TcpclV4BidirectionalLink.h
	include/TcpclV4BundleSink.h
	include/TcpclV4BundleSource.h
	${PROJECT_BINARY_DIR}/tcpcl_lib_export.h
	DESTINATION include
)
target_link_libraries(tcpcl_lib hdtn_util ${Boost_LIBRARIES} ${pthread_LIB})
target_include_directories(tcpcl_lib PUBLIC include ${Boost_INCLUDE_DIRS} ${PROJECT_BINARY_DIR})



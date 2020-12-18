cmake_minimum_required(VERSION 3.0)
project(hdtn-storage)
set(CMAKE_BUILD_TYPE Debug)
set(CMAKE_CXX_STANDARD 11)
find_package(Boost 1.67.0 REQUIRED system)

add_executable(hdtn-storage
    src/main.cpp
    src/store.cpp
    src/store-worker.cpp
    src/cache/cache-basic.cpp
)
target_link_libraries(hdtn-storage pthread zmq hdtn_reg blosc bpcodec)
target_link_directories(hdtn-storage PUBLIC lib)
target_include_directories(hdtn-storage PUBLIC include lib/include ../../common/bpcodec/include ../../common/regsvr/lib/include ../../common/include)

add_executable(hdtn-test-ingress
    src/test/ingress.cpp
)
target_link_libraries(hdtn-test-ingress pthread zmq hdtn_reg bpcodec)
target_link_directories(hdtn-test-ingress PUBLIC lib)
target_include_directories(hdtn-test-ingress PUBLIC include lib/include ../../common/bpcodec/include ../../common/regsvr/lib/include ../../common/include)

add_executable(hdtn-test-egress
    src/test/egress.cpp
)
target_link_libraries(hdtn-test-egress pthread zmq hdtn_reg bpcodec)
target_link_directories(hdtn-test-egress PUBLIC lib)
target_include_directories(hdtn-test-egress PUBLIC include lib/include ../../common/bpcodec/include ../../common/regsvr/lib/include ../../common/include)

add_executable(hdtn-test-schedule
    src/test/schedule.cpp
)
target_link_libraries(hdtn-test-schedule pthread zmq hdtn_reg bpcodec ${Boost_LIBRARIES}) 
target_link_directories(hdtn-test-schedule PUBLIC lib)
target_include_directories(hdtn-test-schedule PUBLIC include lib/include ../../common/bpcodec/include ../../common/regsvr/lib/include ../../common/include)

add_executable(hdtn-test-release
    src/test/release.cpp
)
target_link_libraries(hdtn-test-release pthread zmq hdtn_reg bpcodec)
target_link_directories(hdtn-test-release PUBLIC lib)
target_include_directories(hdtn-test-release PUBLIC include lib/include ../../common/bpcodec/include ../../common/regsvr/lib/include ../../common/include)


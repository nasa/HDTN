cmake_minimum_required(VERSION 3.0)
project(hdtn-storage)
set(CMAKE_CXX_STANDARD 11)
find_package(Boost 1.40.0 REQUIRED system filesystem timer thread)

OPTION(USE_BRIAN_STORAGE "use brian's storage solution" ON)
if(USE_BRIAN_STORAGE)
    add_compile_definitions(USE_BRIAN_STORAGE)
    add_executable(hdtn-storage
        src/main.cpp
        src/store.cpp
        #src/store-worker.cpp
        #src/cache/cache-basic.cpp
        storage-brian/src/MemoryManagerTreeArray.cpp
        storage-brian/src/BundleStorageManagerMT.cpp
        storage-brian/src/CircularIndexBufferSingleProducerSingleConsumer.cpp
        storage-brian/src/SignalHandler.cpp
        storage-brian/src/ZmqStorageInterface.cpp
    )
    target_link_libraries(hdtn-storage pthread zmq hdtn_reg bpcodec boost_system boost_filesystem boost_timer boost_thread)
    target_link_directories(hdtn-storage PUBLIC lib)
    target_include_directories(hdtn-storage PUBLIC include lib/include ../../common/bpcodec/include ../../common/regsvr/lib/include ../../common/include storage-brian/include)

else()
    add_executable(hdtn-storage
        src/main.cpp
        src/store.cpp
        src/store-worker.cpp
        src/cache/cache-basic.cpp
    )
    target_link_libraries(hdtn-storage pthread zmq hdtn_reg blosc bpcodec)
    target_link_directories(hdtn-storage PUBLIC lib)
    target_include_directories(hdtn-storage PUBLIC include lib/include ../../common/bpcodec/include ../../common/regsvr/lib/include ../../common/include)
endif()

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

add_executable(hdtn-test-bundle
    src/test/bundle_test.cpp
)
target_link_libraries(hdtn-test-bundle pthread zmq hdtn_reg bpcodec ${Boost_LIBRARIES})
target_link_directories(hdtn-test-bundle PUBLIC lib)
target_include_directories(hdtn-test-bundle PUBLIC include lib/include ../../common/bpcodec/include ../../common/regsvr/lib/include ../../common/include)

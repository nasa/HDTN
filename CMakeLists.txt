cmake_minimum_required(VERSION 3.0)

add_subdirectory(common/bpcodec)
add_subdirectory(common/regsvr/lib)
add_subdirectory(module/storage)
add_subdirectory(module/ingress)
add_subdirectory(module/egress)

add_subdirectory(tests/unit_tests)

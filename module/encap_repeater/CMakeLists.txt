add_executable(encap-repeater 
    src/EncapRepeater.cpp
)
install(TARGETS encap-repeater DESTINATION ${CMAKE_INSTALL_BINDIR})
target_link_libraries(encap-repeater
    Boost::boost #boost headers
    Boost::thread #also adds Threads::Threads
    Boost::filesystem
    Boost::program_options
)

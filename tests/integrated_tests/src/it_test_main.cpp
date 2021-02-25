#define BOOST_TEST_MODULE HtdnTestsModule

//todo: better cmake solution to detect if we are using boost static or shared libs... assume for now
//      that shared libs will be used on linux and static libs will be used on windows.
#ifndef _WIN32
#define BOOST_TEST_DYN_LINK
#endif

//#define BOOST_TEST_NO_MAIN 1


#include <boost/test/unit_test.hpp>



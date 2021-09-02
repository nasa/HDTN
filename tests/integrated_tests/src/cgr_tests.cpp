#include <fstream>
#include <iostream>
#include <thread>
#include <zmq.hpp>
#include <stdio.h>
#include <stdlib.h>
#include <boost/process.hpp>
#include <boost/thread.hpp>
#include <boost/assign/list_of.hpp>
#include <boost/lexical_cast.hpp>
#include <string>
#include <vector>
#include <boost/test/unit_test.hpp>
#include <boost/test/results_reporter.hpp>
#include <boost/test/unit_test_parameters.hpp>
#include "cgrServer.h"

BOOST_AUTO_TEST_CASE(cgrTest)
{
    CgrServer server;
    server.init("tcp://localhost:5556");
    int nextHop = server.requestNextHop(1,2,0);
    std::cout << "Next hop is: " << std::to_string(nextHop) << std::endl;

    BOOST_CHECK();
}



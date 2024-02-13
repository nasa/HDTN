/**
 * @file HdtnCliMain.cpp
 * @author Ethan Schweinsberg <ethan.e.schweinsberg@nasa.gov>
 * 
 * @copyright Copyright (c) 2021 United States Government as represented by
 * the National Aeronautics and Space Administration.
 * No copyright is claimed in the United States under Title 17, U.S.Code.
 * All Other Rights Reserved.
 * 
 * @section LICENSE
 * Released under the NASA Open Source Agreement (NOSA)
 * See LICENSE.md in the source root directory for more information.
*/

#include <boost/program_options.hpp>

#include <zmq.hpp>

#include "ThreadNamer.h"
#include "HdtnCliRunner.h"

int main(int argc, const char* argv[]) {
    ThreadNamer::SetThisThreadName("HdtnCliMain");

    HdtnCliRunner runner;
    if (!runner.Run(argc, argv)) {
        std::cout << "HDTN CLI failed to run" << std::endl;
    }
    return 0;
}

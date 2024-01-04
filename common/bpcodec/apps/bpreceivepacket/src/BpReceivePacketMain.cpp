/**
 * @file BpReceiveFileMain.cpp
 * @author Timothy Recker University of California Berkeley
 * @author Nadia Kortas <nadia.kortas@nasa.gov>
 *
 * @copyright Copyright © 2021 United States Government as represented by
 * the National Aeronautics and Space Administration.
 * No copyright is claimed in the United States under Title 17, U.S.Code.
 * All Other Rights Reserved.
 *
 * @section LICENSE
 * Released under the NASA Open Source Agreement (NOSA)
 * See LICENSE.md in the source root directory for more information.
 */

#include <iostream>
#include "BpReceivePacketRunner.h"
#include "Logger.h"
#include "ThreadNamer.h"

int main(int argc, const char* argv[]) {

    ThreadNamer::SetThisThreadName("BpReceivePacketMain");
    BpReceivePacketRunner runner;
    std::atomic<bool> running;
    runner.Run(argc, argv, running, true);
    return 0;

}

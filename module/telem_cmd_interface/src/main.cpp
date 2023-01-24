/**
 * @file main.cpp
 *
 * @copyright Copyright © 2023 United States Government as represented by
 * the National Aeronautics and Space Administration.
 * No copyright is claimed in the United States under Title 17, U.S.Code.
 * All Other Rights Reserved.
 *
 * @section LICENSE
 * Released under the NASA Open Source Agreement (NOSA)
 * See LICENSE.md in the source root directory for more information.
 *
 */

#include "Telemetry.h"
#include "Logger.h"

int main(int argc, const char* argv[]) {
    {
        hdtn::Logger::initializeWithProcess(hdtn::Logger::Process::telem);

        Telemetry telemetry;
        volatile bool running;
        telemetry.Run(argc, argv, running);
    }
    return 0;
}

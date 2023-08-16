/**
 * @file Telemetry.h
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
 * @section DESCRIPTION
 * This Telemetry class is used for launching the telemetry runner in its own process.
 * The common use-case for this functionality is when running HDTN in distributed mode.
 * Telemetry provides a blocking Run method, which creates and initializes a TelemetryRunner
 * object with any passed-in command line arguments.
 */

#ifndef TELEMETRY_H
#define TELEMETRY_H 1

#include "telem_lib_export.h"
#include <atomic>

class Telemetry
{
    public:
        TELEM_LIB_EXPORT Telemetry();

        /**
         * Starts the TelemetryRunner as a standalone process 
         */
        TELEM_LIB_EXPORT bool Run(int argc, const char* const argv[], std::atomic<bool>& running);

    private:
        void MonitorExitKeypressThreadFunc();
        std::atomic<bool> m_runningFromSigHandler;

};

#endif //TELEMETRY_H

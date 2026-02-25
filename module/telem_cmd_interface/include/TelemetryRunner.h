/**
 * @file TelemetryRunner.h
 *
 * @copyright Copyright (c) 2026 United States Government as represented by
 * the National Aeronautics and Space Administration.
 * No copyright is claimed in the United States under Title 17, U.S.Code.
 * All Other Rights Reserved.
 *
 * @section LICENSE
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * @section DESCRIPTION
 *
 * This TelemetryRunner class launches a thread that polls HDTN telemetry data. It also
 * processes the data and provides interfaces for viewing it.
 */

#ifndef TELEMETRY_RUNNER_H
#define TELEMETRY_RUNNER_H 1

#include "zmq.hpp"
#include "HdtnConfig.h"
#include "telem_lib_export.h"
#include "TelemetryRunnerProgramOptions.h"


class TelemetryRunner
{
    public:
        TELEM_LIB_EXPORT TelemetryRunner();
        TELEM_LIB_EXPORT ~TelemetryRunner();

        /**
         * Starts the runner as an async thread
         * @param inprocContextPtr context to use for the inproc zmq connections
         * @param options program options for the runner
         */
        TELEM_LIB_EXPORT bool Init(const HdtnConfig& hdtnConfig, zmq::context_t *inprocContextPtr, TelemetryRunnerProgramOptions& options);

        /**
         * Stops the runner
         */
        TELEM_LIB_EXPORT void Stop();

    private:
        // Internal implementation class and pointer
        class Impl;
        std::unique_ptr<Impl> m_pimpl;
};

#endif //TELEMETRY_RUNNER_H

/**
 * @file TelemetryLogger.h
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
 * This TelemetryLogger class implements logging telemetry metrics to files
 */

#ifndef TELEMETRY_LOGGER_H
#define TELEMETRY_LOGGER_H 1

#include <boost/date_time.hpp>

#include "telem_lib_export.h"
#include "TelemetryDefinitions.h"

class TelemetryLogger
{
    public:
        TELEM_LIB_EXPORT TelemetryLogger();

        /**
         * Logs a set of telemetry data to files
         */
        TELEM_LIB_EXPORT void LogTelemetry(
            const AllInductTelemetry_t& inductTelem,
            const AllOutductTelemetry_t& outductTelem,
            const StorageTelemetry_t& storageTelem
        );

         /**
         * Helper function to calculate a megabit/s rate 
         */
        TELEM_LIB_EXPORT static double CalculateMbpsRate(
            double currentBytes,
            double prevBytes, 
            boost::posix_time::ptime nowTime,
            boost::posix_time::ptime lastProcessedTime
        );

        /**
         * Helper functions to calculate egress and ingress rates 
         */
        TELEM_LIB_EXPORT double GetEgressMbpsRate(const AllOutductTelemetry_t& telem);
        TELEM_LIB_EXPORT double GetIngressMbpsRate(const AllInductTelemetry_t& telem);

    private:
        

        boost::posix_time::ptime m_startTime;
};

#endif //TELEMETRY_LOGGER_H

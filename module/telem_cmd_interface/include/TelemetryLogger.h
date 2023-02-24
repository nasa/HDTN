/**
 * @file TelemetryLogger.h
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
        TELEM_LIB_EXPORT void LogTelemetry(Telemetry_t* telem);

         /**
         * Helper function to calculate a megabit/s rate 
         */
        TELEM_LIB_EXPORT static double CalculateMbpsRate(
            double currentBytes,
            double prevBytes, 
            boost::posix_time::ptime nowTime,
            boost::posix_time::ptime lastProcessedTime
        );

    private:
        //TELEM_LIB_EXPORT void LogTelemetry(IngressTelemetry_t* telem);
        //TELEM_LIB_EXPORT void LogTelemetry(EgressTelemetry_t* telem);
        TELEM_LIB_EXPORT void LogTelemetry(StorageTelemetry_t* telem);

        boost::posix_time::ptime m_startTime;
};

#endif //TELEMETRY_LOGGER_H

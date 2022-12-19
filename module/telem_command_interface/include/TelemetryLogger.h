/**
 * @file TelemetryLogger.h
 *
 * @copyright Copyright Ⓒ 2021 United States Government as represented by
 * the National Aeronautics and Space Administration.
 * No copyright is claimed in the United States under Title 17, U.S.Code.
 * All Other Rights Reserved.
 *
 * @section LICENSE
 * Released under the NASA Open Source Agreement (NOSA)
 * See LICENSE.md in the source root directory for more information.
 *
 * @section DESCRIPTION
 *
 * This TelemetryLogger class implements logging telemetry metrics to a file.
 */

#ifndef TELEMETRY_LOGGER_H
#define TELEMETRY_LOGGER_H 1

#include "telem_lib_export.h"
#include "Metrics.h"

class TelemetryLogger
{
    public:
        /**
         * Logs a set of metrics to files
         */
        TELEM_LIB_EXPORT void LogMetrics(Metrics::metrics_t metrics);
};

#endif //TELEMETRY_LOGGER_H

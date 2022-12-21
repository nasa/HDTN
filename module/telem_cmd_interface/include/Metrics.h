/**
 * @file Metrics.h
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
 * This Metrics class provides the data structure and helper functions for working with HDTN metrics.
 */

#ifndef METRICS_H
#define METRICS_H 1

#include <boost/date_time.hpp>

#include "telem_lib_export.h"
#include "TelemetryDefinitions.h"


class Metrics {
    public:
        struct metrics_t {
            TELEM_LIB_EXPORT metrics_t();

            // ingress
            double ingressCurrentRateMbps;
            double ingressAverageRateMbps;
            uint64_t ingressCurrentDataBytes;
            uint64_t ingressTotalDataBytes;
            uint64_t bundleCountSentToEgress;
            uint64_t bundleCountSentToStorage;

            // egress
            uint64_t egressTotalDataBytes;
            uint64_t egressCurrentDataBytes;
            uint64_t egressBundleCount;
            uint64_t egressMessageCount;
            double egressCurrentRateMbps;
            double egressAverageRateMbps;

            // storage
            uint64_t totalBundlesErasedFromStorage;
            uint64_t totalBunglesSentFromEgressToStorage;
        };

        TELEM_LIB_EXPORT Metrics();

        /**
         * Clears the underlying metric values 
         */
        TELEM_LIB_EXPORT void Clear();

        /**
         * Gets the metric values 
         */
        TELEM_LIB_EXPORT metrics_t Get();

        /**
         * Processes and stores new ingress telemetry data 
         */
        TELEM_LIB_EXPORT void ProcessIngressTelem(IngressTelemetry_t& currentTelem);

        /**
         * Processes and stores new egress telemetry data 
         */
        TELEM_LIB_EXPORT void ProcessEgressTelem(EgressTelemetry_t& currentTelem);

        /**
         * Processes and stores new storage telemetry data 
         */
        TELEM_LIB_EXPORT void ProcessStorageTelem(StorageTelemetry_t& currentTelem);

        /**
         * Helper function to calculate a megabit per second rate 
         */
        TELEM_LIB_EXPORT static double CalculateMbpsRate(
            double currentBytes,
            double prevBytes, 
            boost::posix_time::ptime nowTime,
            boost::posix_time::ptime lastProcessedTime
        );

    private:
        boost::posix_time::ptime m_startTime; 
        metrics_t m_metrics;
};

#endif //METRICS_H

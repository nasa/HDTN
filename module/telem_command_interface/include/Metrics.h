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
 *
 * This Metrics class provides the data structure and helper functions for working with HDTN metrics.
 */

#ifndef METRICS_H
#define METRICS_H 1

#include <boost/date_time.hpp>

#include "telem_lib_export.h"

class Metrics {
    public:
        struct metrics_t {
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

        TELEM_LIB_EXPORT static double CalculateMbpsRate(
            double currentBytes,
            double prevBytes, 
            boost::posix_time::ptime nowTime,
            boost::posix_time::ptime lastTime
        );
};

#endif //METRICS_H

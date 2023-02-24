/**
 * @file TelemetryLogger.cpp
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


#include "TelemetryLogger.h"
#include "StatsLogger.h"

TelemetryLogger::TelemetryLogger()
    : m_startTime(boost::posix_time::microsec_clock::universal_time())
{}


void TelemetryLogger::LogTelemetry(AllInductTelemetry_t* telem) { //ingress
    boost::posix_time::ptime nowTime = boost::posix_time::microsec_clock::universal_time();
    static boost::posix_time::ptime lastProcessedTime = nowTime;
    const uint64_t totalDataBytes = telem->m_bundleByteCountEgress + telem->m_bundleByteCountStorage;
    static uint64_t lastTotalDataBytes = totalDataBytes;
    // Skip calculating the bitrate the first time through
    if (nowTime > lastProcessedTime) {
        double currentRateMbps = CalculateMbpsRate(
            static_cast<double>(totalDataBytes),
            static_cast<double>(lastTotalDataBytes),
            nowTime,
            lastProcessedTime);
        LOG_STAT("ingress_data_rate_mbps") << currentRateMbps;
    }
    LOG_STAT("ingress_data_volume_bytes") << totalDataBytes;
    lastTotalDataBytes = totalDataBytes;
    lastProcessedTime = nowTime;
}

void TelemetryLogger::LogTelemetry(AllOutductTelemetry_t* telem) { //egress
    boost::posix_time::ptime nowTime = boost::posix_time::microsec_clock::universal_time();
    static boost::posix_time::ptime lastProcessedTime = nowTime;
    const uint64_t totalDataBytes = telem->m_totalBundleBytesGivenToOutducts;
    static uint64_t lastTotalDataBytes = totalDataBytes;

    // Skip calculating the bitrate the first time through
    if (nowTime > lastProcessedTime) {
        double currentRateMbps = CalculateMbpsRate(
            static_cast<double>(totalDataBytes),
            static_cast<double>(lastTotalDataBytes),
            nowTime,
            lastProcessedTime);
        LOG_STAT("egress_data_rate_mbps") << currentRateMbps;
    }
    LOG_STAT("egress_data_volume_bytes") << totalDataBytes;

    lastTotalDataBytes = totalDataBytes;
    lastProcessedTime = nowTime;
}

void TelemetryLogger::LogTelemetry(StorageTelemetry_t* telem) {
    // No-op. Currently, there's no need for logging storage data
}

double TelemetryLogger::CalculateMbpsRate(
    double currentBytes,
    double prevBytes, 
    boost::posix_time::ptime nowTime,
    boost::posix_time::ptime lastProcessedTime
)
{
    boost::posix_time::time_duration duration = nowTime - lastProcessedTime;
    return (8.0 * (currentBytes - prevBytes)) / duration.total_microseconds();
}

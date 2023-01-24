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

void TelemetryLogger::LogTelemetry(Telemetry_t* telem)
{
    switch (telem->GetType()) {
        case TelemetryType::ingress: {
            IngressTelemetry_t* ingressTelem = dynamic_cast<IngressTelemetry_t*>(telem);
            if (ingressTelem != nullptr) {
                LogTelemetry(ingressTelem);
            }
            break;
        }
        case TelemetryType::egress: {
            EgressTelemetry_t* egressTelem = dynamic_cast<EgressTelemetry_t*>(telem);
            if (egressTelem != nullptr) {
                LogTelemetry(egressTelem);
            }
            break;
        }
        case TelemetryType::storage: {
            StorageTelemetry_t* storageTelem = dynamic_cast<StorageTelemetry_t*>(telem);
            if (storageTelem != nullptr) {
                LogTelemetry(storageTelem);
            }
            break;
        }
    }
}

void TelemetryLogger::LogTelemetry(IngressTelemetry_t* telem)
{
    boost::posix_time::ptime nowTime = boost::posix_time::microsec_clock::universal_time();
    static boost::posix_time::ptime lastProcessedTime = nowTime;
    static uint64_t lastTotalDataBytes = telem->totalDataBytes;
    // Skip calculating the bitrate the first time through
    if (nowTime > lastProcessedTime) {
        double currentRateMbps = CalculateMbpsRate(
            static_cast<double>(telem->totalDataBytes),
            static_cast<double>(lastTotalDataBytes),
            nowTime,
            lastProcessedTime);
        LOG_STAT("ingress_data_rate_mbps") << currentRateMbps;
    }
    LOG_STAT("ingress_data_volume_bytes") << telem->totalDataBytes;
    lastTotalDataBytes = telem->totalDataBytes;
    lastProcessedTime = nowTime;
}

void TelemetryLogger::LogTelemetry(EgressTelemetry_t* telem)
{
    boost::posix_time::ptime nowTime = boost::posix_time::microsec_clock::universal_time();
    static boost::posix_time::ptime lastProcessedTime = nowTime;
    static uint64_t lastTotalDataBytes = telem->totalDataBytes;

    // Skip calculating the bitrate the first time through
    if (nowTime > lastProcessedTime) {
        double currentRateMbps = CalculateMbpsRate(
            static_cast<double>(telem->totalDataBytes),
            static_cast<double>(lastTotalDataBytes),
            nowTime,
            lastProcessedTime);
        LOG_STAT("egress_data_rate_mbps") << currentRateMbps;
    }
    LOG_STAT("egress_data_volume_bytes") << telem->totalDataBytes;

    lastTotalDataBytes = telem->totalDataBytes;
    lastProcessedTime = nowTime;
}

void TelemetryLogger::LogTelemetry(StorageTelemetry_t* telem)
{
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

/**
 * @file TelemetryLogger.cpp
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
 */

#include <vector>

#include "TelemetryLogger.h"
#include "StatsLogger.h"


TelemetryLogger::TelemetryLogger()
    : m_startTime(boost::posix_time::microsec_clock::universal_time())
{}

void TelemetryLogger::LogTelemetry(
    const AllInductTelemetry_t& inductTelem,
    const AllOutductTelemetry_t& outductTelem,
    const StorageTelemetry_t& storageTelem
)
{
    std::vector<hdtn::StatsLogger::metric_t> metrics;
    metrics.push_back(hdtn::StatsLogger::metric_t("ingress_data_rate_mbps", GetIngressMbpsRate(inductTelem)));
    metrics.push_back(hdtn::StatsLogger::metric_t(
        "ingress_total_bytes_sent",
        inductTelem.m_bundleByteCountEgress + inductTelem.m_bundleByteCountStorage
    ));
    metrics.push_back(hdtn::StatsLogger::metric_t("ingress_bytes_sent_egress", inductTelem.m_bundleByteCountEgress));
    metrics.push_back(hdtn::StatsLogger::metric_t("ingress_bytes_sent_storage", inductTelem.m_bundleByteCountStorage));
    metrics.push_back(hdtn::StatsLogger::metric_t("storage_used_space_bytes", storageTelem.m_usedSpaceBytes));
    metrics.push_back(hdtn::StatsLogger::metric_t("storage_free_space_bytes", storageTelem.m_freeSpaceBytes));
    metrics.push_back(hdtn::StatsLogger::metric_t("storage_bundle_bytes_on_disk", storageTelem.m_numBundleBytesOnDisk));
    metrics.push_back(hdtn::StatsLogger::metric_t(
        "storage_bundles_erased",
        storageTelem.m_totalBundlesErasedFromStorageNoCustodyTransfer + storageTelem.m_totalBundlesErasedFromStorageWithCustodyTransfer
    ));
    metrics.push_back(hdtn::StatsLogger::metric_t(
        "storage_bundles_rewritten_from_failed_egress_send",
        storageTelem.m_totalBundlesRewrittenToStorageFromFailedEgressSend
    ));
    metrics.push_back(hdtn::StatsLogger::metric_t(
        "storage_bytes_sent_to_egress_cutthrough",
        storageTelem.m_totalBundleBytesSentToEgressFromStorageForwardCutThrough
    ));
    metrics.push_back(hdtn::StatsLogger::metric_t(
        "storage_bytes_sent_to_egress_from_disk",
        storageTelem.m_totalBundleBytesSentToEgressFromStorageReadFromDisk
    ));
    metrics.push_back(hdtn::StatsLogger::metric_t("egress_data_rate_mbps", GetEgressMbpsRate(outductTelem)));
    metrics.push_back(hdtn::StatsLogger::metric_t(
        "egress_total_bytes_sent_success",
        outductTelem.m_totalBundleBytesSuccessfullySent
    ));
    metrics.push_back(hdtn::StatsLogger::metric_t(
        "egress_total_bytes_attempted",
        outductTelem.m_totalBundleBytesGivenToOutducts
    ));
    hdtn::StatsLogger::Log("all_sampled_stats", metrics);
}

double TelemetryLogger::GetIngressMbpsRate(const AllInductTelemetry_t& telem)
{
    boost::posix_time::ptime nowTime = boost::posix_time::microsec_clock::universal_time();
    static boost::posix_time::ptime lastProcessedTime = nowTime;
    const uint64_t totalDataBytes = telem.m_bundleByteCountEgress + telem.m_bundleByteCountStorage;
    static uint64_t lastTotalDataBytes = totalDataBytes;

    if (nowTime <= lastProcessedTime) {
        return 0;
    }

    double rate = CalculateMbpsRate(
            static_cast<double>(totalDataBytes),
            static_cast<double>(lastTotalDataBytes),
            nowTime,
            lastProcessedTime
    );

    lastTotalDataBytes = totalDataBytes;
    lastProcessedTime = nowTime;
    return rate;
}

double TelemetryLogger::GetEgressMbpsRate(const AllOutductTelemetry_t& telem)
{
    boost::posix_time::ptime nowTime = boost::posix_time::microsec_clock::universal_time();
    static boost::posix_time::ptime lastProcessedTime = nowTime;
    const uint64_t totalDataBytes = telem.m_totalBundleBytesSuccessfullySent;
    static uint64_t lastTotalDataBytes = totalDataBytes;

    if (nowTime <= lastProcessedTime) {
        return 0;
    }

    double rate = CalculateMbpsRate(
            static_cast<double>(totalDataBytes),
            static_cast<double>(lastTotalDataBytes),
            nowTime,
            lastProcessedTime
    );

    lastTotalDataBytes = totalDataBytes;
    lastProcessedTime = nowTime;
    return rate;
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

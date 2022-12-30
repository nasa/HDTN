#include "TelemetryLogger.h"
#include "StatsLogger.h"

TelemetryLogger::TelemetryLogger()
    : m_startTime(boost::posix_time::microsec_clock::universal_time())
{}

void TelemetryLogger::LogTelemetry(Telemetry_t* telem)
{
    switch (telem->GetType()) {
        case TelemetryType::ingress:
            LogTelemetry(dynamic_cast<IngressTelemetry_t*>(telem));
            break;
        case TelemetryType::egress:
            LogTelemetry(dynamic_cast<EgressTelemetry_t*>(telem));
            break;
        case TelemetryType::storage:
            LogTelemetry(dynamic_cast<StorageTelemetry_t*>(telem));
            break;
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
            telem->totalDataBytes,
            lastTotalDataBytes,
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
            telem->totalDataBytes,
            lastTotalDataBytes,
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

#include "TelemetryLogger.h"
#include "StatsLogger.h"


void TelemetryLogger::LogMetrics(Metrics::metrics_t metrics)
{
    LOG_STAT("ingress_data_rate_mbps") << metrics.ingressCurrentRateMbps;
    LOG_STAT("ingress_data_volume_bytes") << metrics.ingressCurrentDataBytes;
    LOG_STAT("egress_data_rate_mbps") << metrics.egressCurrentRateMbps;
    LOG_STAT("egress_data_volume_bytes") << metrics.egressCurrentDataBytes;
}

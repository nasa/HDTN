#include "Metrics.h"

Metrics::metrics_t::metrics_t() :
    ingressCurrentRateMbps(0),
    ingressAverageRateMbps(0),
    ingressCurrentDataBytes(0),
    ingressTotalDataBytes(0),
    bundleCountSentToEgress(0),
    bundleCountSentToStorage(0),

    egressTotalDataBytes(0),
    egressCurrentDataBytes(0),
    egressBundleCount(0),
    egressMessageCount(0),
    egressCurrentRateMbps(0),
    egressAverageRateMbps(0),

    totalBundlesErasedFromStorage(0),
    totalBunglesSentFromEgressToStorage(0)
{}

Metrics::Metrics()
{
    m_startTime = boost::posix_time::microsec_clock::universal_time();
}

Metrics::metrics_t Metrics::Get()
{
    return m_metrics;
}

void Metrics::Clear()
{
    m_metrics = {};
}

double Metrics::CalculateMbpsRate(
    double currentBytes,
    double prevBytes, 
    boost::posix_time::ptime nowTime,
    boost::posix_time::ptime lastProcessedTime
)
{
    boost::posix_time::time_duration duration = nowTime - lastProcessedTime;
    return (8.0 * (currentBytes - prevBytes)) / duration.total_microseconds();
}

void Metrics::ProcessIngressTelem(IngressTelemetry_t& currentTelem)
{
    boost::posix_time::ptime nowTime = boost::posix_time::microsec_clock::universal_time();
    static boost::posix_time::ptime lastProcessedTime = nowTime;
    static IngressTelemetry_t prevTelem = currentTelem;

    // Skip calculating the bitrates the first time through
    if (nowTime > lastProcessedTime) {
        m_metrics.ingressCurrentRateMbps = Metrics::CalculateMbpsRate(
            currentTelem.totalDataBytes,
            prevTelem.totalDataBytes,
            nowTime,
            lastProcessedTime
        );
        m_metrics.ingressAverageRateMbps = Metrics::CalculateMbpsRate(
            currentTelem.totalDataBytes,
            0,
            nowTime,
            m_startTime
        );
    }
    m_metrics.bundleCountSentToEgress = currentTelem.bundleCountEgress;
    m_metrics.bundleCountSentToStorage = currentTelem.bundleCountStorage;
    m_metrics.ingressTotalDataBytes = currentTelem.totalDataBytes;
    m_metrics.ingressCurrentDataBytes = currentTelem.totalDataBytes - prevTelem.totalDataBytes;

    prevTelem = currentTelem;
    lastProcessedTime = nowTime;
}

void Metrics::ProcessEgressTelem(EgressTelemetry_t& currentTelem)
{
    boost::posix_time::ptime nowTime = boost::posix_time::microsec_clock::universal_time();
    static boost::posix_time::ptime lastProcessedTime = nowTime;
    static EgressTelemetry_t prevTelem = currentTelem;

    // Skip calculating the bitrates the first time through
    if (nowTime > lastProcessedTime) {
        m_metrics.egressCurrentRateMbps = Metrics::CalculateMbpsRate(
            currentTelem.totalDataBytes,
            prevTelem.totalDataBytes,
            nowTime,
            lastProcessedTime);
        m_metrics.egressAverageRateMbps = Metrics::CalculateMbpsRate(
            currentTelem.totalDataBytes,
            0,
            nowTime,
            m_startTime);
    }
    m_metrics.egressBundleCount = currentTelem.egressBundleCount;
    m_metrics.egressMessageCount = currentTelem.egressMessageCount;
    m_metrics.egressTotalDataBytes = currentTelem.totalDataBytes;
    m_metrics.egressCurrentDataBytes = currentTelem.totalDataBytes - prevTelem.totalDataBytes;

    prevTelem = currentTelem;
    lastProcessedTime = nowTime;
}

void Metrics::ProcessStorageTelem(StorageTelemetry_t& currentTelem)
{
    m_metrics.totalBundlesErasedFromStorage = currentTelem.totalBundlesErasedFromStorage;
    m_metrics.totalBunglesSentFromEgressToStorage = currentTelem.totalBundlesSentToEgressFromStorage;
}

#include "LtpOverUdpInduct.h"
#include <iostream>
#include <boost/make_unique.hpp>
#include <memory>


LtpOverUdpInduct::LtpOverUdpInduct(const InductProcessBundleCallback_t & inductProcessBundleCallback, const induct_element_config_t & inductConfig, const uint64_t maxBundleSizeBytes) :
    Induct(inductProcessBundleCallback, inductConfig)
{
    m_ltpBundleSinkPtr = boost::make_unique<LtpBundleSink>(
        m_inductProcessBundleCallback,
        inductConfig.thisLtpEngineId, inductConfig.remoteLtpEngineId, inductConfig.ltpReportSegmentMtu,
        boost::posix_time::milliseconds(inductConfig.oneWayLightTimeMs), boost::posix_time::milliseconds(inductConfig.oneWayMarginTimeMs),
        inductConfig.boundPort, inductConfig.numRxCircularBufferElements,
        inductConfig.preallocatedRedDataBytes, inductConfig.ltpMaxRetriesPerSerialNumber,
        (inductConfig.ltpRandomNumberSizeBits == 32), inductConfig.ltpRemoteUdpHostname, inductConfig.ltpRemoteUdpPort, maxBundleSizeBytes,
        inductConfig.ltpMaxExpectedSimultaneousSessions, inductConfig.ltpRxDataSegmentSessionNumberRecreationPreventerHistorySize);

}
LtpOverUdpInduct::~LtpOverUdpInduct() {
    m_ltpBundleSinkPtr.reset();
}

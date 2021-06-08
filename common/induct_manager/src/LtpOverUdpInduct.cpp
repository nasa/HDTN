#include "LtpOverUdpInduct.h"
#include <iostream>
#include <boost/make_unique.hpp>
#include <boost/make_shared.hpp>


LtpOverUdpInduct::LtpOverUdpInduct(const InductProcessBundleCallback_t & inductProcessBundleCallback, const induct_element_config_t & inductConfig) :
    Induct(inductProcessBundleCallback, inductConfig)
{
    m_ltpBundleSinkPtr = boost::make_unique<LtpBundleSink>(
        m_inductProcessBundleCallback,
        inductConfig.thisLtpEngineId, 1, inductConfig.ltpReportSegmentMtu,
        boost::posix_time::milliseconds(inductConfig.oneWayLightTimeMs), boost::posix_time::milliseconds(inductConfig.oneWayMarginTimeMs),
        inductConfig.boundPort, inductConfig.numRxCircularBufferElements,
        inductConfig.numRxCircularBufferBytesPerElement, inductConfig.preallocatedRedDataBytes);

}
LtpOverUdpInduct::~LtpOverUdpInduct() {
    m_ltpBundleSinkPtr.reset();
}

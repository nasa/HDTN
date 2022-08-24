#include "UdpInduct.h"
#include <iostream>
#include <boost/make_unique.hpp>
#include <memory>


UdpInduct::UdpInduct(const InductProcessBundleCallback_t & inductProcessBundleCallback, const induct_element_config_t & inductConfig) :
    Induct(inductProcessBundleCallback, inductConfig)
{
    m_udpBundleSinkPtr = boost::make_unique<UdpBundleSink>(m_ioService, inductConfig.boundPort,
        m_inductProcessBundleCallback,
        m_inductConfig.numRxCircularBufferElements,
        m_inductConfig.numRxCircularBufferBytesPerElement,
        boost::bind(&UdpInduct::ConnectionReadyToBeDeletedNotificationReceived, this));
    

    m_ioServiceThreadPtr = boost::make_unique<boost::thread>(boost::bind(&boost::asio::io_service::run, &m_ioService));
}
UdpInduct::~UdpInduct() {
    
    m_udpBundleSinkPtr.reset();
    
    if (m_ioServiceThreadPtr) {
        m_ioServiceThreadPtr->join();
        m_ioServiceThreadPtr.reset(); //delete it
    }
}


void UdpInduct::RemoveInactiveConnection() {
    if (m_udpBundleSinkPtr && m_udpBundleSinkPtr->ReadyToBeDeleted()) {
        m_udpBundleSinkPtr.reset();
    }
}

void UdpInduct::ConnectionReadyToBeDeletedNotificationReceived() {
    boost::asio::post(m_ioService, boost::bind(&UdpInduct::RemoveInactiveConnection, this));
}
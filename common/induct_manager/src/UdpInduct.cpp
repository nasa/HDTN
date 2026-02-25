/**
 * @file UdpInduct.cpp
 * @author  Brian Tomko <brian.j.tomko@nasa.gov>
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
 */

#include "UdpInduct.h"
#include "Logger.h"
#include <iostream>
#include <boost/make_unique.hpp>
#include <memory>
#include "ThreadNamer.h"

static constexpr hdtn::Logger::SubProcess subprocess = hdtn::Logger::SubProcess::none;

UdpInduct::UdpInduct(const InductProcessBundleCallback_t & inductProcessBundleCallback, const induct_element_config_t & inductConfig) :
    Induct(inductProcessBundleCallback, inductConfig)
{
    m_udpBundleSinkPtr = boost::make_unique<UdpBundleSink>(m_ioService, inductConfig.boundPort,
        m_inductProcessBundleCallback,
        m_inductConfig.numRxCircularBufferElements,
        m_inductConfig.numRxCircularBufferBytesPerElement,
        boost::bind(&UdpInduct::ConnectionReadyToBeDeletedNotificationReceived, this));
    

    m_ioServiceThreadPtr = boost::make_unique<boost::thread>(boost::bind(&boost::asio::io_service::run, &m_ioService));
    ThreadNamer::SetIoServiceThreadName(m_ioService, "ioServiceUdpInduct");
}
UdpInduct::~UdpInduct() {
    
    m_udpBundleSinkPtr.reset();
    
    if (m_ioServiceThreadPtr) {
        try {
            m_ioServiceThreadPtr->join();
            m_ioServiceThreadPtr.reset(); //delete it
        }
        catch (const boost::thread_resource_error&) {
            LOG_ERROR(subprocess) << "error stopping UdpInduct io_service";
        }
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

void UdpInduct::PopulateInductTelemetry(InductTelemetry_t& inductTelem) {
    inductTelem.m_convergenceLayer = "udp";
    inductTelem.m_listInductConnections.clear();
    std::unique_ptr<UdpInductConnectionTelemetry_t> t = boost::make_unique<UdpInductConnectionTelemetry_t>();
    m_udpBundleSinkPtr->GetTelemetry(*t);
    inductTelem.m_listInductConnections.emplace_back(std::move(t));
}

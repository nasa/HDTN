/**
 * @file StcpInduct.cpp
 * @author  Brian Tomko <brian.j.tomko@nasa.gov>
 *
 * @copyright Copyright (c) 2021 United States Government as represented by
 * the National Aeronautics and Space Administration.
 * No copyright is claimed in the United States under Title 17, U.S.Code.
 * All Other Rights Reserved.
 *
 * @section LICENSE
 * Released under the NASA Open Source Agreement (NOSA)
 * See LICENSE.md in the source root directory for more information.
 */

#include "StcpInduct.h"
#include "Logger.h"
#include <boost/make_unique.hpp>
#include "ThreadNamer.h"

static constexpr hdtn::Logger::SubProcess subprocess = hdtn::Logger::SubProcess::none;

StcpInduct::StcpInduct(const InductProcessBundleCallback_t & inductProcessBundleCallback,
    const induct_element_config_t & inductConfig, const uint64_t maxBundleSizeBytes,
    const OnNewOpportunisticLinkCallback_t& onNewOpportunisticLinkCallback, //for telemetry (so know when a new connection is made)
    const OnDeletedOpportunisticLinkCallback_t& onDeletedOpportunisticLinkCallback) :
    Induct(inductProcessBundleCallback, inductConfig),
    m_tcpAcceptor(m_ioService, boost::asio::ip::tcp::endpoint(boost::asio::ip::tcp::v4(), inductConfig.boundPort)),
    m_workPtr(boost::make_unique<boost::asio::io_service::work>(m_ioService)),
    m_allowRemoveInactiveTcpConnections(true),
    M_MAX_BUNDLE_SIZE_BYTES(maxBundleSizeBytes)
{
    m_onNewOpportunisticLinkCallback = onNewOpportunisticLinkCallback;
    m_onDeletedOpportunisticLinkCallback = onDeletedOpportunisticLinkCallback;

    StartTcpAccept();
    m_ioServiceThreadPtr = boost::make_unique<boost::thread>(boost::bind(&boost::asio::io_service::run, &m_ioService));
    ThreadNamer::SetIoServiceThreadName(m_ioService, "ioServiceStcpInduct");
}
StcpInduct::~StcpInduct() {
    if (m_tcpAcceptor.is_open()) {
        try {
            m_tcpAcceptor.close();
        }
        catch (const boost::system::system_error & e) {
            LOG_ERROR(subprocess) << "Error closing TCP Acceptor in StcpInduct::~StcpInduct:  " << e.what();
        }
    }
    boost::asio::post(m_ioService, boost::bind(&StcpInduct::DisableRemoveInactiveTcpConnections, this));
    while (m_allowRemoveInactiveTcpConnections) {
        try {
            boost::this_thread::sleep(boost::posix_time::milliseconds(100));
        }
        catch (const boost::thread_resource_error&) {}
        catch (const boost::thread_interrupted&) {}
        catch (const boost::condition_error&) {}
        catch (const boost::lock_error&) {}
    }
    m_listStcpBundleSinks.clear(); //tcp bundle sink destructor is thread safe
    m_workPtr.reset();

    if (m_ioServiceThreadPtr) {
        try {
            m_ioServiceThreadPtr->join();
            m_ioServiceThreadPtr.reset(); //delete it
        }
        catch (const boost::thread_resource_error&) {
            LOG_ERROR(subprocess) << "error stopping StcpInduct io_service";
        }
    }
}

void StcpInduct::StartTcpAccept() {
    LOG_INFO(subprocess) << "waiting for stcp tcp connections";
    std::shared_ptr<boost::asio::ip::tcp::socket> newTcpSocketPtr = std::make_shared<boost::asio::ip::tcp::socket>(m_ioService); //get_io_service() is deprecated: Use get_executor()

    m_tcpAcceptor.async_accept(*newTcpSocketPtr,
        boost::bind(&StcpInduct::HandleTcpAccept, this, newTcpSocketPtr, boost::asio::placeholders::error));
}

void StcpInduct::HandleTcpAccept(std::shared_ptr<boost::asio::ip::tcp::socket> & newTcpSocketPtr, const boost::system::error_code& error) {
    if (!error) {
        LOG_INFO(subprocess) << "stcp tcp connection: " << newTcpSocketPtr->remote_endpoint().address() << ":" << newTcpSocketPtr->remote_endpoint().port();
        {
            boost::mutex::scoped_lock lock(m_listStcpBundleSinksMutex);
            m_listStcpBundleSinks.emplace_back(newTcpSocketPtr, m_ioService,
                m_inductProcessBundleCallback,
                m_inductConfig.numRxCircularBufferElements,
                M_MAX_BUNDLE_SIZE_BYTES,
                boost::bind(&StcpInduct::ConnectionReadyToBeDeletedNotificationReceived, this));
        }
        if (m_onNewOpportunisticLinkCallback) {
            m_onNewOpportunisticLinkCallback(0, this, &m_listStcpBundleSinks.back());
        }

        StartTcpAccept(); //only accept if there was no error
    }
    else if (error != boost::asio::error::operation_aborted) {
        LOG_ERROR(subprocess) << "tcp accept error: " << error.message();
    }


}

void StcpInduct::RemoveInactiveTcpConnections() {
    const OnDeletedOpportunisticLinkCallback_t& callbackRef = m_onDeletedOpportunisticLinkCallback;
    if (m_allowRemoveInactiveTcpConnections.load(std::memory_order_acquire)) {
        boost::mutex::scoped_lock lock(m_listStcpBundleSinksMutex);
        m_listStcpBundleSinks.remove_if([&callbackRef, this](StcpBundleSink& sink) {
            if (sink.ReadyToBeDeleted()) {
                if (callbackRef) {
                    callbackRef(0, this, &sink);
                }
                return true;
            }
            else {
                return false;
            }
        });
    }
}

void StcpInduct::DisableRemoveInactiveTcpConnections() {
    m_allowRemoveInactiveTcpConnections = false;
}

void StcpInduct::ConnectionReadyToBeDeletedNotificationReceived() {
    boost::asio::post(m_ioService, boost::bind(&StcpInduct::RemoveInactiveTcpConnections, this));
}

void StcpInduct::PopulateInductTelemetry(InductTelemetry_t& inductTelem) {
    inductTelem.m_convergenceLayer = "stcp";
    inductTelem.m_listInductConnections.clear();
    {
        boost::mutex::scoped_lock lock(m_listStcpBundleSinksMutex);
        for (std::list<StcpBundleSink>::const_iterator it = m_listStcpBundleSinks.cbegin(); it != m_listStcpBundleSinks.cend(); ++it) {
            std::unique_ptr<StcpInductConnectionTelemetry_t> t = boost::make_unique<StcpInductConnectionTelemetry_t>();
            it->GetTelemetry(*t);
            inductTelem.m_listInductConnections.emplace_back(std::move(t));
        }
    }
    if (inductTelem.m_listInductConnections.empty()) {
        std::unique_ptr<StcpInductConnectionTelemetry_t> c = boost::make_unique<StcpInductConnectionTelemetry_t>();
        c->m_connectionName = "null";
        c->m_inputName = std::string("*:") + boost::lexical_cast<std::string>(m_tcpAcceptor.local_endpoint().port());
        inductTelem.m_listInductConnections.emplace_back(std::move(c));
    }
}

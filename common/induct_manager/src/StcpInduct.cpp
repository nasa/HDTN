#include "StcpInduct.h"
#include <iostream>
#include <boost/make_unique.hpp>
#include <boost/make_shared.hpp>


StcpInduct::StcpInduct(const InductProcessBundleCallback_t & inductProcessBundleCallback, const induct_element_config_t & inductConfig) :
    Induct(inductProcessBundleCallback, inductConfig),
    m_tcpAcceptor(m_ioService, boost::asio::ip::tcp::endpoint(boost::asio::ip::tcp::v4(), inductConfig.boundPort)),
    m_workPtr(boost::make_unique<boost::asio::io_service::work>(m_ioService)),
    m_allowRemoveInactiveTcpConnections(true)
{
    StartTcpAccept();
    m_ioServiceThreadPtr = boost::make_unique<boost::thread>(boost::bind(&boost::asio::io_service::run, &m_ioService));
}
StcpInduct::~StcpInduct() {
    if (m_tcpAcceptor.is_open()) {
        try {
            m_tcpAcceptor.close();
        }
        catch (const boost::system::system_error & e) {
            std::cerr << "Error closing TCP Acceptor in StcpInduct::~StcpInduct:  " << e.what() << std::endl;
        }
    }
    boost::asio::post(m_ioService, boost::bind(&StcpInduct::DisableRemoveInactiveTcpConnections, this));
    while (m_allowRemoveInactiveTcpConnections) {
        boost::this_thread::sleep(boost::posix_time::milliseconds(100));
    }
    m_listStcpBundleSinks.clear(); //tcp bundle sink destructor is thread safe
    m_workPtr.reset();
    if (m_ioServiceThreadPtr) {
        m_ioServiceThreadPtr->join();
        m_ioServiceThreadPtr.reset(); //delete it
    }
}

void StcpInduct::StartTcpAccept() {
    std::cout << "waiting for stcp tcp connections" << std::endl;
    boost::shared_ptr<boost::asio::ip::tcp::socket> newTcpSocketPtr = boost::make_shared<boost::asio::ip::tcp::socket>(m_ioService); //get_io_service() is deprecated: Use get_executor()

    m_tcpAcceptor.async_accept(*newTcpSocketPtr,
        boost::bind(&StcpInduct::HandleTcpAccept, this, newTcpSocketPtr, boost::asio::placeholders::error));
}

void StcpInduct::HandleTcpAccept(boost::shared_ptr<boost::asio::ip::tcp::socket> & newTcpSocketPtr, const boost::system::error_code& error) {
    if (!error) {
        std::cout << "stcp tcp connection: " << newTcpSocketPtr->remote_endpoint().address() << ":" << newTcpSocketPtr->remote_endpoint().port() << std::endl;
        m_listStcpBundleSinks.emplace_back(newTcpSocketPtr, m_ioService,
            m_inductProcessBundleCallback,
            m_inductConfig.numRxCircularBufferElements,
            boost::bind(&StcpInduct::ConnectionReadyToBeDeletedNotificationReceived, this));

        StartTcpAccept(); //only accept if there was no error
    }
    else if (error != boost::asio::error::operation_aborted) {
        std::cerr << "tcp accept error: " << error.message() << std::endl;
    }


}

void StcpInduct::RemoveInactiveTcpConnections() {
    if (m_allowRemoveInactiveTcpConnections) {
        m_listStcpBundleSinks.remove_if([](StcpBundleSink & sink) { return sink.ReadyToBeDeleted(); });
    }
}

void StcpInduct::DisableRemoveInactiveTcpConnections() {
    m_allowRemoveInactiveTcpConnections = false;
}

void StcpInduct::ConnectionReadyToBeDeletedNotificationReceived() {
    boost::asio::post(m_ioService, boost::bind(&StcpInduct::RemoveInactiveTcpConnections, this));
}
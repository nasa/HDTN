#include "TcpclV4Induct.h"
#include <iostream>
#include <boost/make_unique.hpp>
#include <boost/make_shared.hpp>
#include <boost/lexical_cast.hpp>

//TCPCL INDUCT
TcpclV4Induct::TcpclV4Induct(const InductProcessBundleCallback_t & inductProcessBundleCallback, const induct_element_config_t & inductConfig,
    const uint64_t myNodeId, const uint64_t maxBundleSizeBytes, const OnNewOpportunisticLinkCallback_t & onNewOpportunisticLinkCallback,
    const OnDeletedOpportunisticLinkCallback_t & onDeletedOpportunisticLinkCallback) :
    Induct(inductProcessBundleCallback, inductConfig),
    m_tcpAcceptor(m_ioService, boost::asio::ip::tcp::endpoint(boost::asio::ip::tcp::v4(), inductConfig.boundPort)),
    m_workPtr(boost::make_unique<boost::asio::io_service::work>(m_ioService)),
    M_MY_NODE_ID(myNodeId),
    m_allowRemoveInactiveTcpConnections(true),
    M_MAX_BUNDLE_SIZE_BYTES(maxBundleSizeBytes)    
{
    m_onNewOpportunisticLinkCallback = onNewOpportunisticLinkCallback;
    m_onDeletedOpportunisticLinkCallback = onDeletedOpportunisticLinkCallback;

    StartTcpAccept();
    m_ioServiceThreadPtr = boost::make_unique<boost::thread>(boost::bind(&boost::asio::io_service::run, &m_ioService));
}
TcpclV4Induct::~TcpclV4Induct() {
    if (m_tcpAcceptor.is_open()) {
        try {
            m_tcpAcceptor.close();
        }
        catch (const boost::system::system_error & e) {
            std::cerr << "Error closing TCP Acceptor in TcpclV4Induct::~TcpclInduct:  " << e.what() << std::endl;
        }
    }
    boost::asio::post(m_ioService, boost::bind(&TcpclV4Induct::DisableRemoveInactiveTcpConnections, this));
    while (m_allowRemoveInactiveTcpConnections) {
        boost::this_thread::sleep(boost::posix_time::milliseconds(100));
    }
    m_listTcpclV4BundleSinks.clear(); //tcp bundle sink destructor is thread safe
    m_workPtr.reset();
    if (m_ioServiceThreadPtr) {
        m_ioServiceThreadPtr->join();
        m_ioServiceThreadPtr.reset(); //delete it
    }
}

void TcpclV4Induct::StartTcpAccept() {
    std::cout << "waiting for tcpclv4 tcp connections" << std::endl;
    boost::shared_ptr<boost::asio::ip::tcp::socket> newTcpSocketPtr = boost::make_shared<boost::asio::ip::tcp::socket>(m_ioService); //get_io_service() is deprecated: Use get_executor()

    m_tcpAcceptor.async_accept(*newTcpSocketPtr,
        boost::bind(&TcpclV4Induct::HandleTcpAccept, this, newTcpSocketPtr, boost::asio::placeholders::error));
}

void TcpclV4Induct::HandleTcpAccept(boost::shared_ptr<boost::asio::ip::tcp::socket> & newTcpSocketPtr, const boost::system::error_code& error) {
    if (!error) {
        std::cout << "tcpclv4 tcp connection: " << newTcpSocketPtr->remote_endpoint().address() << ":" << newTcpSocketPtr->remote_endpoint().port() << std::endl;
        m_listTcpclV4BundleSinks.emplace_back(
            m_inductConfig.keepAliveIntervalSeconds,
            newTcpSocketPtr,
            m_ioService,
            m_inductProcessBundleCallback,
            m_inductConfig.numRxCircularBufferElements,
            m_inductConfig.numRxCircularBufferBytesPerElement,
            M_MY_NODE_ID,
            M_MAX_BUNDLE_SIZE_BYTES,
            boost::bind(&TcpclV4Induct::ConnectionReadyToBeDeletedNotificationReceived, this),
            boost::bind(&TcpclV4Induct::OnContactHeaderCallback_FromIoServiceThread, this, boost::placeholders::_1),
            10, //const unsigned int maxUnacked, (todo)
            100000000); //const uint64_t maxFragmentSize = 100000000); (todo)

        StartTcpAccept(); //only accept if there was no error
    }
    else if (error != boost::asio::error::operation_aborted) {
        std::cerr << "tcp accept error: " << error.message() << std::endl;
    }


}

void TcpclV4Induct::RemoveInactiveTcpConnections() {
    const OnDeletedOpportunisticLinkCallback_t & callbackRef = m_onDeletedOpportunisticLinkCallback;
    if (m_allowRemoveInactiveTcpConnections) {
        m_listTcpclV4BundleSinks.remove_if([&callbackRef](TcpclV4BundleSink & sink) {
            if (sink.ReadyToBeDeleted()) {
                if (callbackRef) {
                    callbackRef(sink.GetRemoteNodeId());
                }
                return true;
            }
            else {
                return false;
            }
        });
    }
}

void TcpclV4Induct::DisableRemoveInactiveTcpConnections() {
    m_allowRemoveInactiveTcpConnections = false;
}

void TcpclV4Induct::ConnectionReadyToBeDeletedNotificationReceived() {
    boost::asio::post(m_ioService, boost::bind(&TcpclV4Induct::RemoveInactiveTcpConnections, this));
}

void TcpclV4Induct::OnContactHeaderCallback_FromIoServiceThread(TcpclV4BundleSink * thisTcpclBundleSinkPtr) {
    m_mapNodeIdToOpportunisticBundleQueueMutex.lock();
    OpportunisticBundleQueue & opportunisticBundleQueue = m_mapNodeIdToOpportunisticBundleQueue[thisTcpclBundleSinkPtr->GetRemoteNodeId()];
    m_mapNodeIdToOpportunisticBundleQueueMutex.unlock();
    thisTcpclBundleSinkPtr->SetTryGetOpportunisticDataFunction(boost::bind(&TcpclV4Induct::BundleSinkTryGetData_FromIoServiceThread, this, boost::ref(opportunisticBundleQueue), boost::placeholders::_1));
    thisTcpclBundleSinkPtr->SetNotifyOpportunisticDataAckedCallback(boost::bind(&TcpclV4Induct::BundleSinkNotifyOpportunisticDataAcked_FromIoServiceThread, this, boost::ref(opportunisticBundleQueue)));
    if (m_onNewOpportunisticLinkCallback) {
        m_onNewOpportunisticLinkCallback(thisTcpclBundleSinkPtr->GetRemoteNodeId(), this);
    }
}

void TcpclV4Induct::NotifyBundleReadyToSend_FromIoServiceThread(const uint64_t remoteNodeId) {
    for (std::list<TcpclV4BundleSink>::iterator it = m_listTcpclV4BundleSinks.begin(); it != m_listTcpclV4BundleSinks.end(); ++it) {
        if (it->GetRemoteNodeId() == remoteNodeId) {
            it->TrySendOpportunisticBundleIfAvailable_FromIoServiceThread();
        }
    }
}

void TcpclV4Induct::Virtual_PostNotifyBundleReadyToSend_FromIoServiceThread(const uint64_t remoteNodeId) {
    boost::asio::post(m_ioService, boost::bind(&TcpclV4Induct::NotifyBundleReadyToSend_FromIoServiceThread, this, remoteNodeId));
}

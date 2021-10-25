#include "TcpclInduct.h"
#include <iostream>
#include <boost/make_unique.hpp>
#include <boost/make_shared.hpp>
#include <boost/lexical_cast.hpp>

//TCPCL INDUCT
TcpclInduct::TcpclInduct(const InductProcessBundleCallback_t & inductProcessBundleCallback, const induct_element_config_t & inductConfig,
    const uint64_t myNodeId, const uint64_t maxBundleSizeBytes, const OnNewOpportunisticLinkCallback_t & onNewOpportunisticLinkCallback,
    const OnDeletedOpportunisticLinkCallback_t & onDeletedOpportunisticLinkCallback) :
    Induct(inductProcessBundleCallback, inductConfig),
    m_tcpAcceptor(m_ioService, boost::asio::ip::tcp::endpoint(boost::asio::ip::tcp::v4(), inductConfig.boundPort)),
    m_workPtr(boost::make_unique<boost::asio::io_service::work>(m_ioService)),
    M_MY_NODE_ID(myNodeId),
    m_allowRemoveInactiveTcpConnections(true),
    M_MAX_BUNDLE_SIZE_BYTES(maxBundleSizeBytes),
    m_onNewOpportunisticLinkCallback(onNewOpportunisticLinkCallback),
    m_onDeletedOpportunisticLinkCallback(onDeletedOpportunisticLinkCallback)
{
    StartTcpAccept();
    m_ioServiceThreadPtr = boost::make_unique<boost::thread>(boost::bind(&boost::asio::io_service::run, &m_ioService));
}
TcpclInduct::~TcpclInduct() {
    if (m_tcpAcceptor.is_open()) {
        try {
            m_tcpAcceptor.close();
        }
        catch (const boost::system::system_error & e) {
            std::cerr << "Error closing TCP Acceptor in TcpclInduct::~TcpclInduct:  " << e.what() << std::endl;
        }
    }
    boost::asio::post(m_ioService, boost::bind(&TcpclInduct::DisableRemoveInactiveTcpConnections, this));
    while (m_allowRemoveInactiveTcpConnections) {
        boost::this_thread::sleep(boost::posix_time::milliseconds(100));
    }
    m_listTcpclBundleSinks.clear(); //tcp bundle sink destructor is thread safe
    m_workPtr.reset();
    if (m_ioServiceThreadPtr) {
        m_ioServiceThreadPtr->join();
        m_ioServiceThreadPtr.reset(); //delete it
    }
}

void TcpclInduct::StartTcpAccept() {
    std::cout << "waiting for tcpcl tcp connections" << std::endl;
    boost::shared_ptr<boost::asio::ip::tcp::socket> newTcpSocketPtr = boost::make_shared<boost::asio::ip::tcp::socket>(m_ioService); //get_io_service() is deprecated: Use get_executor()

    m_tcpAcceptor.async_accept(*newTcpSocketPtr,
        boost::bind(&TcpclInduct::HandleTcpAccept, this, newTcpSocketPtr, boost::asio::placeholders::error));
}

void TcpclInduct::HandleTcpAccept(boost::shared_ptr<boost::asio::ip::tcp::socket> & newTcpSocketPtr, const boost::system::error_code& error) {
    if (!error) {
        std::cout << "tcpcl tcp connection: " << newTcpSocketPtr->remote_endpoint().address() << ":" << newTcpSocketPtr->remote_endpoint().port() << std::endl;
        m_listTcpclBundleSinks.emplace_back(newTcpSocketPtr, m_ioService,
            m_inductProcessBundleCallback,
            m_inductConfig.numRxCircularBufferElements,
            m_inductConfig.numRxCircularBufferBytesPerElement,
            M_MY_NODE_ID,
            M_MAX_BUNDLE_SIZE_BYTES,
            boost::bind(&TcpclInduct::ConnectionReadyToBeDeletedNotificationReceived, this),
            boost::bind(&TcpclInduct::OnContactHeaderCallback_FromIoServiceThread, this, boost::placeholders::_1),
            10, //const unsigned int maxUnacked, (todo)
            100000000); //const uint64_t maxFragmentSize = 100000000); (todo)

        StartTcpAccept(); //only accept if there was no error
    }
    else if (error != boost::asio::error::operation_aborted) {
        std::cerr << "tcp accept error: " << error.message() << std::endl;
    }


}

void TcpclInduct::RemoveInactiveTcpConnections() {
    const OnDeletedOpportunisticLinkCallback_t & callbackRef = m_onDeletedOpportunisticLinkCallback;
    if (m_allowRemoveInactiveTcpConnections) {
        m_listTcpclBundleSinks.remove_if([&callbackRef](TcpclBundleSink & sink) {
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

void TcpclInduct::DisableRemoveInactiveTcpConnections() {
    m_allowRemoveInactiveTcpConnections = false;
}

void TcpclInduct::ConnectionReadyToBeDeletedNotificationReceived() {
    boost::asio::post(m_ioService, boost::bind(&TcpclInduct::RemoveInactiveTcpConnections, this));
}

bool TcpclInduct::ForwardOnOpportunisticLink(const uint64_t remoteNodeId, std::vector<uint8_t> & dataVec, const uint32_t timeoutSeconds) {
    return ForwardOnOpportunisticLink(remoteNodeId, NULL, &dataVec, timeoutSeconds);
}
bool TcpclInduct::ForwardOnOpportunisticLink(const uint64_t remoteNodeId, zmq::message_t & dataZmq, const uint32_t timeoutSeconds) {
    return ForwardOnOpportunisticLink(remoteNodeId, &dataZmq, NULL, timeoutSeconds);
}
bool TcpclInduct::ForwardOnOpportunisticLink(const uint64_t remoteNodeId, const uint8_t* bundleData, const std::size_t size, const uint32_t timeoutSeconds) {
    std::vector<uint8_t> dataVec(bundleData, bundleData + size);
    return ForwardOnOpportunisticLink(remoteNodeId, NULL, &dataVec, timeoutSeconds);
}
bool TcpclInduct::ForwardOnOpportunisticLink(const uint64_t remoteNodeId, zmq::message_t * zmqMsgPtr, std::vector<uint8_t> * vec8Ptr, const uint32_t timeoutSeconds) {
    m_mapNodeIdToOpportunisticBundleQueueMutex.lock();
    OpportunisticBundleQueue & opportunisticBundleQueue = m_mapNodeIdToOpportunisticBundleQueue[remoteNodeId];
    m_mapNodeIdToOpportunisticBundleQueueMutex.unlock();
    boost::posix_time::ptime timeoutExpiry((timeoutSeconds != 0) ?
        boost::posix_time::special_values::not_a_date_time :
        boost::posix_time::special_values::neg_infin); //allow zero time to fail immediately if full
    while (opportunisticBundleQueue.GetQueueSize() > 5) { //todo
        if (timeoutExpiry == boost::posix_time::special_values::not_a_date_time) {
            timeoutExpiry = boost::posix_time::microsec_clock::universal_time() + boost::posix_time::seconds(timeoutSeconds);
        }
        else if (timeoutExpiry < boost::posix_time::microsec_clock::universal_time()) {
            std::string msg = "notice in TcpclInduct::ForwardOnOpportunisticLink: timed out after " +
                boost::lexical_cast<std::string>(timeoutSeconds) +
                " seconds because it has too many pending opportunistic bundles the queue for remoteNodeId " +
                boost::lexical_cast<std::string>(remoteNodeId);
            return false;

        }
        opportunisticBundleQueue.WaitUntilNotifiedOr250MsTimeout();
        //thread is now unblocked, and the lock is reacquired by invoking lock.lock()
    }
    if (zmqMsgPtr) {
        opportunisticBundleQueue.PushMove_ThreadSafe(*zmqMsgPtr);
    }
    else {
        opportunisticBundleQueue.PushMove_ThreadSafe(*vec8Ptr);
    }
    boost::asio::post(m_ioService, boost::bind(&TcpclInduct::NotifyBundleReadyToSend_FromIoServiceThread, this, remoteNodeId));
    return true;
}


bool TcpclInduct::BundleSinkTryGetData_FromIoServiceThread(OpportunisticBundleQueue & opportunisticBundleQueue, std::pair<std::unique_ptr<zmq::message_t>, std::vector<uint8_t> > & bundleDataPair) {
    return opportunisticBundleQueue.TryPop_ThreadSafe(bundleDataPair);
}
void TcpclInduct::OnContactHeaderCallback_FromIoServiceThread(TcpclBundleSink * thisTcpclBundleSinkPtr) {
    m_mapNodeIdToOpportunisticBundleQueueMutex.lock();
    OpportunisticBundleQueue & opportunisticBundleQueue = m_mapNodeIdToOpportunisticBundleQueue[thisTcpclBundleSinkPtr->GetRemoteNodeId()];
    m_mapNodeIdToOpportunisticBundleQueueMutex.unlock();
    thisTcpclBundleSinkPtr->SetTryGetOpportunisticDataFunction(boost::bind(&TcpclInduct::BundleSinkTryGetData_FromIoServiceThread, this, boost::ref(opportunisticBundleQueue), boost::placeholders::_1));
    thisTcpclBundleSinkPtr->SetNotifyOpportunisticDataAckedCallback(boost::bind(&TcpclInduct::BundleSinkNotifyOpportunisticDataAcked_FromIoServiceThread, this, boost::ref(opportunisticBundleQueue)));
    if (m_onNewOpportunisticLinkCallback) {
        m_onNewOpportunisticLinkCallback(thisTcpclBundleSinkPtr->GetRemoteNodeId());
    }
}

void TcpclInduct::BundleSinkNotifyOpportunisticDataAcked_FromIoServiceThread(OpportunisticBundleQueue & opportunisticBundleQueue) {
    opportunisticBundleQueue.NotifyAll();
}

bool TcpclInduct::NotifyBundleReadyToSend_FromIoServiceThread(const uint64_t remoteNodeId) {
    for (std::list<TcpclBundleSink>::iterator it = m_listTcpclBundleSinks.begin(); it != m_listTcpclBundleSinks.end(); ++it) {
        if (it->GetRemoteNodeId() == remoteNodeId) {
            it->TrySendOpportunisticBundleIfAvailable_FromIoServiceThread();
        }
    }
    return false;
}

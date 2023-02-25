/**
 * @file TcpclV4Induct.cpp
 * @author  Brian Tomko <brian.j.tomko@nasa.gov>
 *
 * @copyright Copyright © 2021 United States Government as represented by
 * the National Aeronautics and Space Administration.
 * No copyright is claimed in the United States under Title 17, U.S.Code.
 * All Other Rights Reserved.
 *
 * @section LICENSE
 * Released under the NASA Open Source Agreement (NOSA)
 * See LICENSE.md in the source root directory for more information.
 */

#include "TcpclV4Induct.h"
#include "Logger.h"
#include <boost/make_unique.hpp>
#include <boost/lexical_cast.hpp>
#include "ThreadNamer.h"

static constexpr hdtn::Logger::SubProcess subprocess = hdtn::Logger::SubProcess::none;

//TCPCL INDUCT
TcpclV4Induct::TcpclV4Induct(const InductProcessBundleCallback_t & inductProcessBundleCallback, const induct_element_config_t & inductConfig,
    const uint64_t myNodeId, const uint64_t maxBundleSizeBytes, const OnNewOpportunisticLinkCallback_t & onNewOpportunisticLinkCallback,
    const OnDeletedOpportunisticLinkCallback_t & onDeletedOpportunisticLinkCallback) :
    Induct(inductProcessBundleCallback, inductConfig),
#ifdef OPENSSL_SUPPORT_ENABLED
    m_shareableSslContext(boost::asio::ssl::context::sslv23_server),
#endif
    m_tcpAcceptor(m_ioService, boost::asio::ip::tcp::endpoint(boost::asio::ip::tcp::v4(), inductConfig.boundPort)),
    m_workPtr(boost::make_unique<boost::asio::io_service::work>(m_ioService)),
    M_MY_NODE_ID(myNodeId),
    m_allowRemoveInactiveTcpConnections(true),
    M_MAX_BUNDLE_SIZE_BYTES(maxBundleSizeBytes)    
{
    m_tlsSuccessfullyConfigured = false;
#ifdef OPENSSL_SUPPORT_ENABLED
    if ((!inductConfig.certificatePemFile.empty()) && (!inductConfig.privateKeyPemFile.empty())) { //(M_BASE_TRY_USE_TLS) {
        try {
            //tcpclv4 server supports tls 1.2 and 1.3 only
            m_shareableSslContext.set_options(
                boost::asio::ssl::context::default_workarounds
                | boost::asio::ssl::context::no_sslv2
                | boost::asio::ssl::context::no_sslv3
                | boost::asio::ssl::context::no_tlsv1
                | boost::asio::ssl::context::no_tlsv1_1
                | boost::asio::ssl::context::single_dh_use);
            //m_shareableSslContext.set_password_callback(boost::bind(&server::get_password, this));
            //m_shareableSslContext.use_certificate_chain_file("server.pem");
            m_shareableSslContext.use_certificate_file(inductConfig.certificatePemFile.string(), boost::asio::ssl::context::pem); //"C:/hdtn_ssl_certificates/cert.pem"
            m_shareableSslContext.use_private_key_file(inductConfig.privateKeyPemFile.string(), boost::asio::ssl::context::pem); //"C:/hdtn_ssl_certificates/privatekey.pem"
            if (!inductConfig.diffieHellmanParametersPemFile.empty()) {
                LOG_WARNING(subprocess) << "tcpclv4 induct using diffie hellman parameters file";
                m_shareableSslContext.use_tmp_dh_file(inductConfig.diffieHellmanParametersPemFile.string()); //"C:/hdtn_ssl_certificates/dh4096.pem"
            }
            m_tlsSuccessfullyConfigured = true;
        }
        catch (boost::system::system_error & e) {
            LOG_ERROR(subprocess) << "error in TcpclV4Induct constructor: " << e.what() << ": tls shall be disabled for this induct";
        }
    }
    if ((!m_tlsSuccessfullyConfigured) && inductConfig.tlsIsRequired) {
        LOG_ERROR(subprocess) << "error in TcpclV4Induct constructor: TLS is required but tls is not properly configured.. this induct shall be disabled for safety.";
        return;
    }
#endif
    m_onNewOpportunisticLinkCallback = onNewOpportunisticLinkCallback;
    m_onDeletedOpportunisticLinkCallback = onDeletedOpportunisticLinkCallback;

    StartTcpAccept();
    m_ioServiceThreadPtr = boost::make_unique<boost::thread>(boost::bind(&boost::asio::io_service::run, &m_ioService));
    ThreadNamer::SetIoServiceThreadName(m_ioService, "ioServiceTcpclV4Induct");
}
TcpclV4Induct::~TcpclV4Induct() {
    if (m_tcpAcceptor.is_open()) {
        try {
            m_tcpAcceptor.close();
        }
        catch (const boost::system::system_error & e) {
            LOG_ERROR(subprocess) << "Error closing TCP Acceptor in TcpclV4Induct::~TcpclInduct:  " << e.what();
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
    LOG_INFO(subprocess) << "waiting for tcpclv4 tcp connections";
#ifdef OPENSSL_SUPPORT_ENABLED
    std::shared_ptr<boost::asio::ssl::stream<boost::asio::ip::tcp::socket> > newSslStreamPtr =
        std::make_shared<boost::asio::ssl::stream<boost::asio::ip::tcp::socket> >(m_ioService, m_shareableSslContext);
    m_tcpAcceptor.async_accept(newSslStreamPtr->next_layer(),
        boost::bind(&TcpclV4Induct::HandleTcpAccept, this, newSslStreamPtr, boost::asio::placeholders::error));
#else
    std::shared_ptr<boost::asio::ip::tcp::socket> newTcpSocketPtr = std::make_shared<boost::asio::ip::tcp::socket>(m_ioService); //get_io_service() is deprecated: Use get_executor()
    m_tcpAcceptor.async_accept(*newTcpSocketPtr,
        boost::bind(&TcpclV4Induct::HandleTcpAccept, this, newTcpSocketPtr, boost::asio::placeholders::error));
#endif
    

    
}

#ifdef OPENSSL_SUPPORT_ENABLED
void TcpclV4Induct::HandleTcpAccept(std::shared_ptr<boost::asio::ssl::stream<boost::asio::ip::tcp::socket> > & newSslStreamSharedPtr, const boost::system::error_code & error) {
    if (!error) {
        LOG_INFO(subprocess) << "tcpclv4 tcp connection: " << newSslStreamSharedPtr->next_layer().remote_endpoint().address() << ":" << newSslStreamSharedPtr->next_layer().remote_endpoint().port();
        {
            boost::mutex::scoped_lock lock(m_listTcpclV4BundleSinksMutex);
            m_listTcpclV4BundleSinks.emplace_back(
                newSslStreamSharedPtr,
#else
void TcpclV4Induct::HandleTcpAccept(std::shared_ptr<boost::asio::ip::tcp::socket> &newTcpSocketPtr, const boost::system::error_code & error) {
    if (!error) {
        LOG_INFO(subprocess) << "tcpclv4 tcp connection: " << newTcpSocketPtr->remote_endpoint().address() << ":" << newTcpSocketPtr->remote_endpoint().port();
        {
            boost::mutex::scoped_lock lock(m_listTcpclV4BundleSinksMutex);
            m_listTcpclV4BundleSinks.emplace_back(
                newTcpSocketPtr,
#endif
                m_tlsSuccessfullyConfigured,
                m_inductConfig.tlsIsRequired,
                m_inductConfig.keepAliveIntervalSeconds,
                m_ioService,
                m_inductProcessBundleCallback,
                m_inductConfig.numRxCircularBufferElements,
                m_inductConfig.numRxCircularBufferBytesPerElement,
                M_MY_NODE_ID,
                M_MAX_BUNDLE_SIZE_BYTES,
                boost::bind(&TcpclV4Induct::ConnectionReadyToBeDeletedNotificationReceived, this),
                boost::bind(&TcpclV4Induct::OnContactHeaderCallback_FromIoServiceThread, this, boost::placeholders::_1),
                10, //const unsigned int maxUnacked, (todo)
                m_inductConfig.tcpclV4MyMaxRxSegmentSizeBytes); //const uint64_t maxFragmentSize = 100000000); (todo)
        }
        StartTcpAccept(); //only accept if there was no error
    }
    else if (error != boost::asio::error::operation_aborted) {
        LOG_ERROR(subprocess) << "tcp accept error: " << error.message();
    }


}


void TcpclV4Induct::RemoveInactiveTcpConnections() {
    const OnDeletedOpportunisticLinkCallback_t & callbackRef = m_onDeletedOpportunisticLinkCallback;
    if (m_allowRemoveInactiveTcpConnections) {
        boost::mutex::scoped_lock lock(m_listTcpclV4BundleSinksMutex);
        m_listTcpclV4BundleSinks.remove_if([&callbackRef, this](TcpclV4BundleSink & sink) {
            if (sink.ReadyToBeDeleted()) {
                if (callbackRef) {
                    callbackRef(sink.GetRemoteNodeId(), this, &sink);
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
    m_mapNodeIdToOpportunisticBundleQueue.erase(thisTcpclBundleSinkPtr->GetRemoteNodeId());
    OpportunisticBundleQueue & opportunisticBundleQueue = m_mapNodeIdToOpportunisticBundleQueue[thisTcpclBundleSinkPtr->GetRemoteNodeId()];
    //opportunisticBundleQueue.m_bidirectionalLinkPtr = thisTcpclBundleSinkPtr;
    opportunisticBundleQueue.m_maxTxBundlesInPipeline = thisTcpclBundleSinkPtr->Virtual_GetMaxTxBundlesInPipeline();
    opportunisticBundleQueue.m_remoteNodeId = thisTcpclBundleSinkPtr->GetRemoteNodeId();
    m_mapNodeIdToOpportunisticBundleQueueMutex.unlock();
    thisTcpclBundleSinkPtr->SetTryGetOpportunisticDataFunction(boost::bind(&TcpclV4Induct::BundleSinkTryGetData_FromIoServiceThread, this, boost::ref(opportunisticBundleQueue), boost::placeholders::_1));
    thisTcpclBundleSinkPtr->SetNotifyOpportunisticDataAckedCallback(boost::bind(&TcpclV4Induct::BundleSinkNotifyOpportunisticDataAcked_FromIoServiceThread, this, boost::ref(opportunisticBundleQueue)));
    if (m_onNewOpportunisticLinkCallback) {
        m_onNewOpportunisticLinkCallback(thisTcpclBundleSinkPtr->GetRemoteNodeId(), this, thisTcpclBundleSinkPtr);
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

void TcpclV4Induct::PopulateInductTelemetry(InductTelemetry_t& inductTelem) {
    inductTelem.m_convergenceLayer = "tcpcl_v4";
    inductTelem.m_listInductConnections.clear();
    {
        boost::mutex::scoped_lock lock(m_listTcpclV4BundleSinksMutex);
        for (std::list<TcpclV4BundleSink>::const_iterator it = m_listTcpclV4BundleSinks.cbegin(); it != m_listTcpclV4BundleSinks.cend(); ++it) {
            inductTelem.m_listInductConnections.emplace_back(boost::make_unique<TcpclV4InductConnectionTelemetry_t>(it->m_base_inductConnectionTelemetry));
        }
    }
    if (inductTelem.m_listInductConnections.empty()) {
        std::unique_ptr<TcpclV4InductConnectionTelemetry_t> c = boost::make_unique<TcpclV4InductConnectionTelemetry_t>();
        c->m_connectionName = "null";
        c->m_inputName = std::string("*:") + boost::lexical_cast<std::string>(m_tcpAcceptor.local_endpoint().port());
        inductTelem.m_listInductConnections.emplace_back(std::move(c));
    }
}

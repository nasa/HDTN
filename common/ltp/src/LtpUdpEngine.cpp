#include "LtpUdpEngine.h"
#include <boost/make_unique.hpp>

LtpUdpEngine::LtpUdpEngine(const uint64_t thisEngineId, const uint64_t mtuClientServiceData, uint64_t mtuReportSegment,
    const boost::posix_time::time_duration & oneWayLightTime, const boost::posix_time::time_duration & oneWayMarginTime,
    const uint16_t udpPort, const bool isReceiver, const bool senderRequireRemoteEndpointMatchOnReceivePacket, const unsigned int numUdpRxCircularBufferVectors, const unsigned int maxUdpRxPacketSizeBytes,
    const uint64_t ESTIMATED_BYTES_TO_RECEIVE_PER_SESSION,
    uint32_t checkpointEveryNthDataPacketSender, uint32_t maxRetriesPerSerialNumber, const bool force32BitRandomNumbers) :
    LtpEngine(thisEngineId, mtuClientServiceData, mtuReportSegment, oneWayLightTime, oneWayMarginTime,
        ESTIMATED_BYTES_TO_RECEIVE_PER_SESSION, true, checkpointEveryNthDataPacketSender, maxRetriesPerSerialNumber, force32BitRandomNumbers),
    M_MY_BOUND_UDP_PORT(udpPort),
    M_IS_RECEIVER(isReceiver),
    M_SENDER_REQUIRE_REMOTE_ENDPOINT_MATCH_ON_RECEIVE_PACKET(senderRequireRemoteEndpointMatchOnReceivePacket),
    m_resolver(m_ioServiceUdp),
    m_udpSocket(m_ioServiceUdp),
    M_NUM_CIRCULAR_BUFFER_VECTORS(numUdpRxCircularBufferVectors),
    M_MAX_UDP_PACKET_SIZE_BYTES(maxUdpRxPacketSizeBytes),
    m_circularIndexBuffer(M_NUM_CIRCULAR_BUFFER_VECTORS),
    m_udpReceiveBuffersCbVec(M_NUM_CIRCULAR_BUFFER_VECTORS),
    m_remoteEndpointsCbVec(M_NUM_CIRCULAR_BUFFER_VECTORS),
    m_sessionOriginatorEngineIdDecodedCallbackCbVec(M_NUM_CIRCULAR_BUFFER_VECTORS),
    m_udpReceiveDiscardBuffer(M_MAX_UDP_PACKET_SIZE_BYTES),
    m_readyToForward(false),
    m_printedCbTooSmallNotice(false),
    m_countAsyncSendCalls(0),
    m_countAsyncSendCallbackCalls(0),
    m_countCircularBufferOverruns(0)
{
    for (unsigned int i = 0; i < M_NUM_CIRCULAR_BUFFER_VECTORS; ++i) {
        m_udpReceiveBuffersCbVec[i].resize(M_MAX_UDP_PACKET_SIZE_BYTES);
        m_sessionOriginatorEngineIdDecodedCallbackCbVec[i] = boost::bind(&LtpUdpEngine::SessionOriginatorEngineIdDecodedCallbackFromThisUdpEndpoint, this, &m_remoteEndpointsCbVec[i], boost::placeholders::_1);
    }

    //Receiver UDP
    try {
        m_udpSocket.open(boost::asio::ip::udp::v4());
        m_udpSocket.bind(boost::asio::ip::udp::endpoint(boost::asio::ip::udp::v4(), udpPort)); // //if udpPort is 0 then bind to random ephemeral port
    }
    catch (const boost::system::system_error & e) {
        std::cerr << "Could not bind on UDP port " << udpPort << std::endl;
        std::cerr << "  Error: " << e.what() << std::endl;

        return;
    }
    printf("LtpUdpEngine bound successfully on UDP port %d ...", m_udpSocket.local_endpoint().port());
    StartUdpReceive(); //call before creating io_service thread so that it has "work"

    m_ioServiceUdpThreadPtr = boost::make_unique<boost::thread>(boost::bind(&boost::asio::io_service::run, &m_ioServiceUdp));
}

LtpUdpEngine::~LtpUdpEngine() {
    Stop();
    //std::cout << "end of ~LtpUdpEngine with port " << M_MY_BOUND_UDP_PORT << std::endl;
    std::cout << "~LtpUdpEngine m_countAsyncSendCalls " << m_countAsyncSendCalls << " m_countCircularBufferOverruns " << m_countCircularBufferOverruns << std::endl;
}

void LtpUdpEngine::Stop() {
    if (m_ioServiceUdpThreadPtr) {
        boost::asio::post(m_ioServiceUdp, boost::bind(&LtpUdpEngine::DoUdpShutdown, this));
        m_ioServiceUdpThreadPtr->join();
        m_ioServiceUdpThreadPtr.reset(); //delete it
    }

    //print stats
    
}

void LtpUdpEngine::Reset() {
    LtpEngine::Reset();
    m_countAsyncSendCalls = 0;
    m_countAsyncSendCallbackCalls = 0;
    m_countCircularBufferOverruns = 0;
}

void LtpUdpEngine::StartUdpReceive() {
    const unsigned int writeIndex = m_circularIndexBuffer.GetIndexForWrite(); //store the volatile
    if (writeIndex == UINT32_MAX) {
        ++m_countCircularBufferOverruns;
        if (!m_printedCbTooSmallNotice) {
            m_printedCbTooSmallNotice = true;
            std::cout << "notice in LtpUdpEngine::StartUdpReceive(): buffers full.. you might want to increase the circular buffer size! Next UDP packet will be dropped!" << std::endl;
        }
        m_udpSocket.async_receive_from(
            boost::asio::buffer(m_udpReceiveDiscardBuffer),
            m_remoteEndpointDiscard,
            boost::bind(&LtpUdpEngine::HandleUdpReceiveDiscard, this,
                boost::asio::placeholders::error,
                boost::asio::placeholders::bytes_transferred));
    }
    else {
        m_udpSocket.async_receive_from(
            boost::asio::buffer(m_udpReceiveBuffersCbVec[writeIndex]),
            m_remoteEndpointsCbVec[writeIndex],
            boost::bind(&LtpUdpEngine::HandleUdpReceive, this,
                boost::asio::placeholders::error,
                boost::asio::placeholders::bytes_transferred,
                writeIndex));
    }
}

void LtpUdpEngine::SendPacket(std::vector<boost::asio::const_buffer> & constBufferVec, boost::shared_ptr<std::vector<std::vector<uint8_t> > > & underlyingDataToDeleteOnSentCallback, const uint64_t sessionOriginatorEngineId) {
    //called by LtpEngine Thread
    ++m_countAsyncSendCalls;
    if (m_udpDropSimulatorFunction && m_udpDropSimulatorFunction(*((uint8_t*)constBufferVec[0].data()))) {
        boost::asio::post(m_ioServiceUdp, boost::bind(&LtpUdpEngine::HandleUdpSend, this, underlyingDataToDeleteOnSentCallback, boost::system::error_code(), 0));
    }
    else {
        if (M_IS_RECEIVER) { //i am the receiver/destination/server
            if (m_readyToForward) { //the server called connect because there is a configuration requiring a fixed endpoint to send report segments to
                m_udpSocket.async_send_to(constBufferVec, m_udpDestinationResolvedEndpointDataSourceToDataSink,
                    boost::bind(&LtpUdpEngine::HandleUdpSend, this, underlyingDataToDeleteOnSentCallback,
                        boost::asio::placeholders::error,
                        boost::asio::placeholders::bytes_transferred));
            }
            else {
                std::map<uint64_t, boost::asio::ip::udp::endpoint>::iterator it = m_mapSessionOriginatorEngineIdToReceiverReplyEndpointsToSender_usedByLtpEngineThreadOnly.find(sessionOriginatorEngineId);
                if (it != m_mapSessionOriginatorEngineIdToReceiverReplyEndpointsToSender_usedByLtpEngineThreadOnly.end()) { //found
                    m_udpSocket.async_send_to(constBufferVec, it->second,
                        boost::bind(&LtpUdpEngine::HandleUdpSend, this, underlyingDataToDeleteOnSentCallback,
                            boost::asio::placeholders::error,
                            boost::asio::placeholders::bytes_transferred));
                }
                else {
                    std::cerr << "error in LtpUdpEngine::SendPacket: sessionOriginatorEngineId " << sessionOriginatorEngineId << " not found" << std::endl;
                }
            }
        }
        else { //i am the source/client
            m_udpSocket.async_send_to(constBufferVec, m_udpDestinationResolvedEndpointDataSourceToDataSink,
                boost::bind(&LtpUdpEngine::HandleUdpSend, this, underlyingDataToDeleteOnSentCallback,
                    boost::asio::placeholders::error,
                    boost::asio::placeholders::bytes_transferred));           
        }
    }
}

void LtpUdpEngine::HandleUdpReceive(const boost::system::error_code & error, std::size_t bytesTransferred, unsigned int writeIndex) {
    if (!error) {
        Ltp::SessionOriginatorEngineIdDecodedCallback_t * sessionOriginatorEngineIdDecodedCallbackPtr = NULL;
        if (M_IS_RECEIVER) { //i am the receiver/destination/server
            //call this function during beginning stages of Ltp packet decode to compare ltp engine id to the udp endpoint to see if there are changes or new additions
            sessionOriginatorEngineIdDecodedCallbackPtr = &m_sessionOriginatorEngineIdDecodedCallbackCbVec[writeIndex];
        }
        else { //i am the source/client
            if (M_SENDER_REQUIRE_REMOTE_ENDPOINT_MATCH_ON_RECEIVE_PACKET) {
                const boost::asio::ip::udp::endpoint & rxEndpoint = m_remoteEndpointsCbVec[writeIndex];
                if (m_udpDestinationResolvedEndpointDataSourceToDataSink != rxEndpoint) { //and my received server endpoint doesnt match what i resolved from the Connect function
                    std::cerr << "error in LtpUdpEngine::HandleUdpReceive: source/client received udp from unexpected server endpoint " << rxEndpoint.address() << ":" << rxEndpoint.port() << std::endl;
                    DoUdpShutdown();
                    return;
                }
            }
        }
        
        m_circularIndexBuffer.CommitWrite(); //write complete at this point
        PacketIn_ThreadSafe(m_udpReceiveBuffersCbVec[writeIndex].data(), bytesTransferred, sessionOriginatorEngineIdDecodedCallbackPtr); //Post to the LtpEngine IoService so its thread will process
        StartUdpReceive(); //restart operation only if there was no error
    }
    else if (error != boost::asio::error::operation_aborted) {
        std::cerr << "critical error in LtpUdpEngine::HandleUdpReceive(): " << error.message() << std::endl;
        DoUdpShutdown();
    }
}

void LtpUdpEngine::HandleUdpReceiveDiscard(const boost::system::error_code & error, std::size_t bytesTransferred) {
    if (!error) {
        if (M_SENDER_REQUIRE_REMOTE_ENDPOINT_MATCH_ON_RECEIVE_PACKET && (!M_IS_RECEIVER) && (m_udpDestinationResolvedEndpointDataSourceToDataSink != m_remoteEndpointDiscard)) { //i am the source/client and received udp from unexpected server endpoint
            std::cerr << "error in LtpUdpEngine::HandleUdpReceiveDiscard: source/client received udp from unexpected server endpoint " << m_remoteEndpointDiscard.address() << ":" << m_remoteEndpointDiscard.port() << std::endl;
            DoUdpShutdown();
            return;            
        }

        StartUdpReceive(); //restart operation only if there was no error
    }
    else if (error != boost::asio::error::operation_aborted) {
        std::cerr << "critical error in LtpUdpEngine::HandleUdpReceiveDiscard(): " << error.message() << std::endl;
        DoUdpShutdown();
    }
}

void LtpUdpEngine::SessionOriginatorEngineIdDecodedCallbackFromThisUdpEndpoint(const boost::asio::ip::udp::endpoint * const udpEndpointPtr, const uint64_t sessionOriginatorEngineId) {
    //Called by LTP Engine thread
    std::map<uint64_t, boost::asio::ip::udp::endpoint>::iterator it = m_mapSessionOriginatorEngineIdToReceiverReplyEndpointsToSender_usedByLtpEngineThreadOnly.find(sessionOriginatorEngineId);
    if (it == m_mapSessionOriginatorEngineIdToReceiverReplyEndpointsToSender_usedByLtpEngineThreadOnly.end()) { //not found
        std::cout << "notice: receiver/destination/server received udp from new session originator engine id "
            << sessionOriginatorEngineId << " with address " << udpEndpointPtr->address() << ":" << udpEndpointPtr->port() << std::endl;
        m_mapSessionOriginatorEngineIdToReceiverReplyEndpointsToSender_usedByLtpEngineThreadOnly[sessionOriginatorEngineId] = *udpEndpointPtr;

    }
    else if (it->second != (*udpEndpointPtr)) {
        std::cout << "notice: receiver/destination/server received udp from old session originator engine id "
            << sessionOriginatorEngineId << " with updated address " << udpEndpointPtr->address() << ":" << udpEndpointPtr->port() << std::endl;
        it->second = *udpEndpointPtr;
    }
}

void LtpUdpEngine::PacketInFullyProcessedCallback(bool success) {
    //Called by LTP Engine thread
    //std::cout << "PacketInFullyProcessedCallback " << std::endl;
    m_circularIndexBuffer.CommitRead(); //LtpEngine IoService thread will CommitRead
}

void LtpUdpEngine::Connect(const std::string & hostname, const std::string & port) {

    static const boost::asio::ip::resolver_query_base::flags UDP_RESOLVER_FLAGS = boost::asio::ip::resolver_query_base::canonical_name; //boost resolver flags
    std::cout << "udp resolving " << hostname << ":" << port << std::endl;
    boost::asio::ip::udp::resolver::query query(boost::asio::ip::udp::v4(), hostname, port, UDP_RESOLVER_FLAGS);
    m_resolver.async_resolve(query, boost::bind(&LtpUdpEngine::OnResolve,
        this, boost::asio::placeholders::error,
        boost::asio::placeholders::results));
}

void LtpUdpEngine::OnResolve(const boost::system::error_code & ec, boost::asio::ip::udp::resolver::results_type results) { // Resolved endpoints as a range.
    if (ec) {
        std::cerr << "Error resolving: " << ec.message() << std::endl;
    }
    else {
        m_udpDestinationResolvedEndpointDataSourceToDataSink = *results;
        std::cout << "resolved host to " << m_udpDestinationResolvedEndpointDataSourceToDataSink.address() << ":" << m_udpDestinationResolvedEndpointDataSourceToDataSink.port() << std::endl;
        std::cout << "UDP READY" << std::endl;
        m_readyToForward = true;
    }
}


void LtpUdpEngine::HandleUdpSend(boost::shared_ptr<std::vector<std::vector<uint8_t> > > underlyingDataToDeleteOnSentCallback, const boost::system::error_code& error, std::size_t bytes_transferred) {
    ++m_countAsyncSendCallbackCalls;
    if (error) {
        std::cerr << "error in LtpUdpEngine::HandleUdpSend: " << error.message() << std::endl;
        DoUdpShutdown();
    }
    else {
        //TODO RATE STUFF
        //std::cout << "sent " << bytes_transferred << std::endl;

        if (m_countAsyncSendCallbackCalls == m_countAsyncSendCalls) { //prevent too many sends from stacking up in ioService queue
            SignalReadyForSend_ThreadSafe();
        }
    }
}

void LtpUdpEngine::DoUdpShutdown() {
    //final code to shut down tcp sockets
    m_readyToForward = false;
    if (m_udpSocket.is_open()) {
        try {
            std::cout << "closing LtpUdpEngine UDP socket.." << std::endl;
            m_udpSocket.close();
        }
        catch (const boost::system::system_error & e) {
            std::cout << "notice in LtpUdpEngine::DoUdpShutdown calling udpSocket.close: " << e.what() << std::endl;
        }
    }
}

bool LtpUdpEngine::ReadyToForward() {
    return m_readyToForward;
}

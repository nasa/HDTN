/**
 * @file LtpUdpEngineManager.cpp
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

#include "LtpUdpEngineManager.h"
#include <boost/make_unique.hpp>
#include <boost/lexical_cast.hpp>
#include "Sdnv.h"

//c++ shared singleton using weak pointer
//https://codereview.stackexchange.com/questions/14343/c-shared-singleton
std::map<uint16_t, std::weak_ptr<LtpUdpEngineManager> > LtpUdpEngineManager::m_staticMapBoundPortToLtpUdpEngineManagerPtr;
boost::mutex LtpUdpEngineManager::m_staticMutex;
uint64_t LtpUdpEngineManager::M_STATIC_MAX_UDP_RX_PACKET_SIZE_BYTES_FOR_ALL_LTP_UDP_ENGINES = 0;

//static function
void LtpUdpEngineManager::SetMaxUdpRxPacketSizeBytesForAllLtp(const uint64_t maxUdpRxPacketSizeBytesForAllLtp) {
    boost::mutex::scoped_lock theLock(m_staticMutex);
    if (maxUdpRxPacketSizeBytesForAllLtp == 0) {
        std::cerr << "Error in LtpUdpEngineManager::SetMaxUdpRxPacketSizeBytesForAllLtp: LTP Max RX UDP packet size cannot be zero\n";
    }
    else if (maxUdpRxPacketSizeBytesForAllLtp < 100) {
        std::cerr << "Error in LtpUdpEngineManager::SetMaxUdpRxPacketSizeBytesForAllLtp: LTP Max RX UDP packet size must be at least 100 bytes\n";
    }
    else if (M_STATIC_MAX_UDP_RX_PACKET_SIZE_BYTES_FOR_ALL_LTP_UDP_ENGINES == 0) {
        M_STATIC_MAX_UDP_RX_PACKET_SIZE_BYTES_FOR_ALL_LTP_UDP_ENGINES = maxUdpRxPacketSizeBytesForAllLtp;
        std::cout << "All LTP UDP engines can receive a maximum of " << M_STATIC_MAX_UDP_RX_PACKET_SIZE_BYTES_FOR_ALL_LTP_UDP_ENGINES << " bytes per packet\n";
    }
    else if (M_STATIC_MAX_UDP_RX_PACKET_SIZE_BYTES_FOR_ALL_LTP_UDP_ENGINES != maxUdpRxPacketSizeBytesForAllLtp) {
        std::cerr << "Error in LtpUdpEngineManager::SetMaxUdpRxPacketSizeBytesForAllLtp: LTP Max RX UDP packet size cannot be changed from " 
            << M_STATIC_MAX_UDP_RX_PACKET_SIZE_BYTES_FOR_ALL_LTP_UDP_ENGINES << " to " << maxUdpRxPacketSizeBytesForAllLtp << "\n";
    }
}

//static function
std::shared_ptr<LtpUdpEngineManager> LtpUdpEngineManager::GetOrCreateInstance(const uint16_t myBoundUdpPort, const bool autoStart) {
    boost::mutex::scoped_lock theLock(m_staticMutex);
    std::shared_ptr<LtpUdpEngineManager> sp;
    if (M_STATIC_MAX_UDP_RX_PACKET_SIZE_BYTES_FOR_ALL_LTP_UDP_ENGINES == 0) {
        std::cerr << "Error in LtpUdpEngineManager::GetOrCreateInstance: LTP Max RX UDP packet size not set.. call SetMaxUdpRxPacketSizeBytesForAllLtp() first\n";
    }
    else {
        std::map<uint16_t, std::weak_ptr<LtpUdpEngineManager> >::iterator it = m_staticMapBoundPortToLtpUdpEngineManagerPtr.find(myBoundUdpPort);
        if ((it == m_staticMapBoundPortToLtpUdpEngineManagerPtr.end()) || (it->second.expired())) { //create new instance
            sp.reset(new LtpUdpEngineManager(myBoundUdpPort, autoStart));
            m_staticMapBoundPortToLtpUdpEngineManagerPtr[myBoundUdpPort] = sp;
        }
        else {
            sp = it->second.lock();
        }
    }
    return sp;
}

//private constructor
LtpUdpEngineManager::LtpUdpEngineManager(const uint16_t myBoundUdpPort, const bool autoStart) :
    M_MY_BOUND_UDP_PORT(myBoundUdpPort),
    m_resolver(m_ioServiceUdp),
    m_udpSocket(m_ioServiceUdp),
    m_retryAfterSocketErrorTimer(m_ioServiceUdp),
    m_socketRestoredTimer(m_ioServiceUdp),
    m_udpReceiveBuffer(M_STATIC_MAX_UDP_RX_PACKET_SIZE_BYTES_FOR_ALL_LTP_UDP_ENGINES),
    m_vecEngineIndexToLtpUdpEngineTransmitterPtr(256, NULL),
    m_nextEngineIndex(1),
    m_readyToForward(false)
{
    if (autoStart) {
        StartIfNotAlreadyRunning(); //TODO EVALUATE IF AUTO START SAFE
    }
}

//call Start after all UDP engines have been added
bool LtpUdpEngineManager::StartIfNotAlreadyRunning() {
    if (!m_ioServiceUdpThreadPtr) {
        //Receiver UDP
        try {
            m_udpSocket.open(boost::asio::ip::udp::v4());
            m_udpSocket.bind(boost::asio::ip::udp::endpoint(boost::asio::ip::udp::v4(), M_MY_BOUND_UDP_PORT)); // //if udpPort is 0 then bind to random ephemeral port
        }
        catch (const boost::system::system_error & e) {
            std::cerr << "Could not bind on UDP port " << M_MY_BOUND_UDP_PORT << std::endl;
            std::cerr << "  Error: " << e.what() << std::endl;

            return false;
        }
        printf("LtpUdpEngineManager bound successfully on UDP port %d\n", m_udpSocket.local_endpoint().port());

        StartUdpReceive(); //call before creating io_service thread so that it has "work"

        m_ioServiceUdpThreadPtr = boost::make_unique<boost::thread>(boost::bind(&boost::asio::io_service::run, &m_ioServiceUdp));
        m_readyToForward = true;
    }
    return true;
}

LtpUdpEngine * LtpUdpEngineManager::GetLtpUdpEnginePtrByRemoteEngineId(const uint64_t remoteEngineId, const bool isInduct) {
    std::map<uint64_t, std::unique_ptr<LtpUdpEngine> > * const whichMap = (isInduct) ? &m_mapRemoteEngineIdToLtpUdpEngineReceiverPtr : &m_mapRemoteEngineIdToLtpUdpEngineTransmitterPtr;
    std::map<uint64_t, std::unique_ptr<LtpUdpEngine> >::iterator it = whichMap->find(remoteEngineId);
    return (it == whichMap->end()) ? NULL : it->second.get();
}

void LtpUdpEngineManager::RemoveLtpUdpEngineByRemoteEngineId_ThreadSafe(const uint64_t remoteEngineId, const bool isInduct, const boost::function<void()> & callback) {
    boost::asio::post(m_ioServiceUdp, boost::bind(&LtpUdpEngineManager::RemoveLtpUdpEngineByRemoteEngineId_NotThreadSafe, this, remoteEngineId, isInduct, callback));
}
void LtpUdpEngineManager::RemoveLtpUdpEngineByRemoteEngineId_NotThreadSafe(const uint64_t remoteEngineId, const bool isInduct, const boost::function<void()> & callback) {
    std::map<uint64_t, std::unique_ptr<LtpUdpEngine> > * const whichMap = (isInduct) ? &m_mapRemoteEngineIdToLtpUdpEngineReceiverPtr : &m_mapRemoteEngineIdToLtpUdpEngineTransmitterPtr;
    std::map<uint64_t, std::unique_ptr<LtpUdpEngine> >::iterator it = whichMap->find(remoteEngineId);
    if (it == whichMap->end()) {
        std::cerr << "error in LtpUdpEngineManager::RemoveLtpUdpEngineByRemoteEngineId_NotThreadSafe: remoteEngineId " << remoteEngineId
            << " for type " << ((isInduct) ? "induct" : "outduct") << " does not exist" << std::endl;
    }
    else {
        whichMap->erase(it);
        std::cout << "remoteEngineId " << remoteEngineId << " for type " << ((isInduct) ? "induct" : "outduct") << " successfully removed" << std::endl;
    }
    if (callback) {
        callback();
    }
}

//a max of 254 engines can be added for one outduct with the same udp port
bool LtpUdpEngineManager::AddLtpUdpEngine(const uint64_t thisEngineId, const uint64_t remoteEngineId, const bool isInduct, const uint64_t mtuClientServiceData, uint64_t mtuReportSegment,
    const boost::posix_time::time_duration & oneWayLightTime, const boost::posix_time::time_duration & oneWayMarginTime,
    const std::string & remoteHostname, const uint16_t remotePort, const unsigned int numUdpRxCircularBufferVectors,
    const uint64_t ESTIMATED_BYTES_TO_RECEIVE_PER_SESSION, const uint64_t maxRedRxBytesPerSession, uint32_t checkpointEveryNthDataPacketSender,
    uint32_t maxRetriesPerSerialNumber, const bool force32BitRandomNumbers, const uint64_t maxSendRateBitsPerSecOrZeroToDisable, const uint64_t maxSimultaneousSessions,
    const uint64_t rxDataSegmentSessionNumberRecreationPreventerHistorySizeOrZeroToDisable,
    const uint64_t maxUdpPacketsToSendPerSystemCall, const uint64_t senderPingSecondsOrZeroToDisable,
    const uint64_t delaySendingOfReportSegmentsTimeMsOrZeroToDisable,
    const uint64_t delaySendingOfDataSegmentsTimeMsOrZeroToDisable)
{   
    if ((delaySendingOfReportSegmentsTimeMsOrZeroToDisable != 0) && (!isInduct)) {
        std::cerr << "error in LtpUdpEngineManager::AddLtpUdpEngine: delaySendingOfReportSegmentsTimeMsOrZeroToDisable must be set to 0 for an outduct\n";
        return false;
    }
    if ((delaySendingOfDataSegmentsTimeMsOrZeroToDisable != 0) && (isInduct)) {
        std::cerr << "error in LtpUdpEngineManager::AddLtpUdpEngine: delaySendingOfDataSegmentsTimeMsOrZeroToDisable must be set to 0 for an induct\n";
        return false;
    }
    if ((m_nextEngineIndex > 255) && (!isInduct)) {
        std::cerr << "error in LtpUdpEngineManager::AddLtpUdpEngine: a max of 254 engines can be added for one outduct with the same udp port\n";
        return false;
    }
    if (maxUdpPacketsToSendPerSystemCall == 0) {
        std::cerr << "error in LtpUdpEngineManager::AddLtpUdpEngine: maxUdpPacketsToSendPerSystemCall must be non-zero.\n";
        return false;
    }
#ifdef UIO_MAXIOV
    //sendmmsg() is Linux-specific. NOTES The value specified in vlen is capped to UIO_MAXIOV (1024).
    if (maxUdpPacketsToSendPerSystemCall > UIO_MAXIOV) {
        std::cerr << "error in LtpUdpEngineManager::AddLtpUdpEngine: maxUdpPacketsToSendPerSystemCall ("
            << maxUdpPacketsToSendPerSystemCall << ") must be <= UIO_MAXIOV (" << UIO_MAXIOV << ").\n";
        return false;
    }
#endif //UIO_MAXIOV
    if (senderPingSecondsOrZeroToDisable && isInduct) {
        std::cerr << "error in LtpUdpEngineManager::AddLtpUdpEngine: senderPingSecondsOrZeroToDisable cannot be used with an induct (must be set to 0).\n";
        return false;
    }
    std::map<uint64_t, std::unique_ptr<LtpUdpEngine> > * const whichMap = (isInduct) ? &m_mapRemoteEngineIdToLtpUdpEngineReceiverPtr : &m_mapRemoteEngineIdToLtpUdpEngineTransmitterPtr;
    std::map<uint64_t, std::unique_ptr<LtpUdpEngine> >::iterator it = whichMap->find(remoteEngineId);
    if (it != whichMap->end()) {
        std::cerr << "error in LtpUdpEngineManager::AddLtpUdpEngine: remote engine Id " << remoteEngineId
            << " for type " << ((isInduct) ? "induct" : "outduct") << " already exists" << std::endl;
        return false;
    }

    static const boost::asio::ip::resolver_query_base::flags UDP_RESOLVER_FLAGS = boost::asio::ip::resolver_query_base::canonical_name; //boost resolver flags
    boost::asio::ip::udp::endpoint remoteEndpoint;
    try {
        remoteEndpoint = *m_resolver.resolve(boost::asio::ip::udp::resolver::query(boost::asio::ip::udp::v4(), remoteHostname, boost::lexical_cast<std::string>(remotePort), UDP_RESOLVER_FLAGS));
    }
    catch (const boost::system::system_error & e) {
        std::cout << "Error resolving udp in LtpUdpEngineManager::AddLtpUdpEngine: " << e.what() << "  code=" << e.code() << std::endl;
        return false;
    }
    std::cout << "Adding LTP engineId: " << thisEngineId << " who will talk with remote " <<  remoteEndpoint.address() << ":" << remoteEndpoint.port() << std::endl;

    const uint8_t engineIndex = static_cast<uint8_t>(m_nextEngineIndex); //this is a don't care for inducts, only needed for outducts
    std::unique_ptr<LtpUdpEngine> newLtpUdpEnginePtr = boost::make_unique<LtpUdpEngine>(m_ioServiceUdp,
        m_udpSocket, thisEngineId, engineIndex, mtuClientServiceData, mtuReportSegment, oneWayLightTime, oneWayMarginTime,
        remoteEndpoint, numUdpRxCircularBufferVectors, ESTIMATED_BYTES_TO_RECEIVE_PER_SESSION, maxRedRxBytesPerSession, checkpointEveryNthDataPacketSender,
        maxRetriesPerSerialNumber, force32BitRandomNumbers, M_STATIC_MAX_UDP_RX_PACKET_SIZE_BYTES_FOR_ALL_LTP_UDP_ENGINES, maxSendRateBitsPerSecOrZeroToDisable, maxSimultaneousSessions,
        rxDataSegmentSessionNumberRecreationPreventerHistorySizeOrZeroToDisable, maxUdpPacketsToSendPerSystemCall, senderPingSecondsOrZeroToDisable,
        delaySendingOfReportSegmentsTimeMsOrZeroToDisable, delaySendingOfDataSegmentsTimeMsOrZeroToDisable);
    if (!isInduct) {
        ++m_nextEngineIndex;
        m_vecEngineIndexToLtpUdpEngineTransmitterPtr[engineIndex] = newLtpUdpEnginePtr.get();
    }
    (*whichMap)[remoteEngineId] = std::move(newLtpUdpEnginePtr);
    
    return true;
}

LtpUdpEngineManager::~LtpUdpEngineManager() {
    Stop();
    std::cout << "~LtpUdpEngineManager" << std::endl;
}

void LtpUdpEngineManager::Stop() {
    if (m_ioServiceUdpThreadPtr) {
        boost::asio::post(m_ioServiceUdp, boost::bind(&LtpUdpEngineManager::DoUdpShutdown, this));
        m_ioServiceUdpThreadPtr->join();
        m_ioServiceUdpThreadPtr.reset(); //delete it
    }

    //print stats
    
}


void LtpUdpEngineManager::StartUdpReceive() {
    m_udpSocket.async_receive_from(
        boost::asio::buffer(m_udpReceiveBuffer),
        m_remoteEndpointReceived,
        boost::bind(&LtpUdpEngineManager::HandleUdpReceive, this,
            boost::asio::placeholders::error,
            boost::asio::placeholders::bytes_transferred));
}


void LtpUdpEngineManager::HandleUdpReceive(const boost::system::error_code & error, std::size_t bytesTransferred) {
    if (!error) {
        if (bytesTransferred <= 2) {
            std::cerr << "error in LtpUdpEngineManager::HandleUdpReceive(): bytesTransferred <= 2 .. ignoring packet" << std::endl;
            StartUdpReceive();
            return;
        }
        
        const uint8_t segmentTypeFlags = m_udpReceiveBuffer[0]; // & 0x0f; //upper 4 bits must be 0 for version 0
        bool isSenderToReceiver;
        if (!Ltp::GetMessageDirectionFromSegmentFlags(segmentTypeFlags, isSenderToReceiver)) {
            std::cerr << "critical error in LtpUdpEngineManager::HandleUdpReceive(): received invalid ltp packet with segment type flag " << (int)segmentTypeFlags << std::endl;
            DoUdpShutdown();
            return;
        }
#if defined(USE_SDNV_FAST) && defined(SDNV_SUPPORT_AVX2_FUNCTIONS)
        uint64_t decodedValues[2];
        const unsigned int numSdnvsToDecode = 2u - isSenderToReceiver;
        uint8_t totalBytesDecoded;
        unsigned int numValsDecodedThisIteration = SdnvDecodeMultiple256BitU64Fast(&m_udpReceiveBuffer[1], &totalBytesDecoded, decodedValues, numSdnvsToDecode);
        if (numValsDecodedThisIteration != numSdnvsToDecode) { //all required sdnvs were not decoded, possibly due to a decode error
            std::cerr << "error in LtpUdpEngineManager::HandleUdpReceive(): cannot read 1 or more of sessionOriginatorEngineId or sessionNumber.. ignoring packet" << std::endl;
            StartUdpReceive();
            return;
        }
        const uint64_t & sessionOriginatorEngineId = decodedValues[0];
        const uint64_t & sessionNumber = decodedValues[1];
#else
        uint8_t sdnvSize;
        const uint64_t sessionOriginatorEngineId = SdnvDecodeU64(&m_udpReceiveBuffer[1], &sdnvSize, (100 - 1)); //no worries about hardware accelerated sdnv read out of bounds due to minimum 100 byte size
        if (sdnvSize == 0) {
            std::cerr << "error in LtpUdpEngineManager::HandleUdpReceive(): cannot read sessionOriginatorEngineId.. ignoring packet" << std::endl;
            StartUdpReceive();
            return;
        }
        uint64_t sessionNumber;
        if (!isSenderToReceiver) {
            sessionNumber = SdnvDecodeU64(&m_udpReceiveBuffer[1 + sdnvSize], &sdnvSize, ((100 - 10) - 1)); //no worries about hardware accelerated sdnv read out of bounds due to minimum 100 byte size
            if (sdnvSize == 0) {
                std::cerr << "error in LtpUdpEngineManager::HandleUdpReceive(): cannot read sessionNumber.. ignoring packet" << std::endl;
                StartUdpReceive();
                return;
            }
        }
#endif

        LtpUdpEngine * ltpUdpEnginePtr;
        if (isSenderToReceiver) { //received an isSenderToReceiver message type => isInduct (this ltp engine received a message type that only travels from an outduct (sender) to an induct (receiver))
            //sessionOriginatorEngineId is the remote engine id in the case of an induct
            std::map<uint64_t, std::unique_ptr<LtpUdpEngine> >::iterator it = m_mapRemoteEngineIdToLtpUdpEngineReceiverPtr.find(sessionOriginatorEngineId);
            if (it == m_mapRemoteEngineIdToLtpUdpEngineReceiverPtr.end()) {
                std::cerr << "error in LtpUdpEngineManager::HandleUdpReceive: an induct received packet with unknown remote engine Id "
                    << sessionOriginatorEngineId << ".. ignoring packet" << std::endl;
                StartUdpReceive();
                return;
            }
            ltpUdpEnginePtr = it->second.get();
        }
        else { //received an isReceiverToSender message type => isOutduct (this ltp engine received a message type that only travels from an induct (receiver) to an outduct (sender))
            //sessionOriginatorEngineId is my engine id in the case of an outduct.. need to get the session number to find the proper LtpUdpEngine
            const uint8_t engineIndex = LtpRandomNumberGenerator::GetEngineIndexFromRandomSessionNumber(sessionNumber);
            ltpUdpEnginePtr = m_vecEngineIndexToLtpUdpEngineTransmitterPtr[engineIndex];
            if (ltpUdpEnginePtr == NULL) {
                std::cerr << "error in LtpUdpEngineManager::HandleUdpReceive: an outduct received packet of type " << (int)segmentTypeFlags << " with unknown session number "
                    << sessionNumber << ".. ignoring packet" << std::endl;
                StartUdpReceive();
                return;
            }
        }
        
        ltpUdpEnginePtr->PostPacketFromManager_ThreadSafe(m_udpReceiveBuffer, bytesTransferred);
        if (m_udpReceiveBuffer.size() != M_STATIC_MAX_UDP_RX_PACKET_SIZE_BYTES_FOR_ALL_LTP_UDP_ENGINES) {
            std::cerr << "error in LtpUdpEngineManager::HandleUdpReceive: swapped packet not size " 
                << M_STATIC_MAX_UDP_RX_PACKET_SIZE_BYTES_FOR_ALL_LTP_UDP_ENGINES << "... resizing" << std::endl;
            m_udpReceiveBuffer.resize(M_STATIC_MAX_UDP_RX_PACKET_SIZE_BYTES_FOR_ALL_LTP_UDP_ENGINES);
        }
        StartUdpReceive(); //restart operation only if there was no error
    }
    else if (error != boost::asio::error::operation_aborted) {
        //this happens with windows loopback (localhost) peer udp sockets being terminated
        m_readyToForward = false;
        std::cerr << "critical error in LtpUdpEngineManager::HandleUdpReceive(): " << error.message() << std::endl
            << "Will try to Receive after 2 seconds" << std::endl;
        for (std::map<uint64_t, std::unique_ptr<LtpUdpEngine> >::iterator it = m_mapRemoteEngineIdToLtpUdpEngineTransmitterPtr.begin();
            it != m_mapRemoteEngineIdToLtpUdpEngineTransmitterPtr.end(); ++it)
        {
            it->second->PostExternalLinkDownEvent_ThreadSafe();
        }
        m_socketRestoredTimer.cancel();
        m_retryAfterSocketErrorTimer.expires_from_now(boost::posix_time::seconds(2));
        m_retryAfterSocketErrorTimer.async_wait(boost::bind(&LtpUdpEngineManager::OnRetryAfterSocketError_TimerExpired, this, boost::asio::placeholders::error));
        //DoUdpShutdown();
    }
}

void LtpUdpEngineManager::OnRetryAfterSocketError_TimerExpired(const boost::system::error_code& e) {
    if (e != boost::asio::error::operation_aborted) {
        // Timer was not cancelled, take necessary action.
        std::cout << "Trying to receive..." << std::endl;
        m_socketRestoredTimer.expires_from_now(boost::posix_time::seconds(5));
        m_socketRestoredTimer.async_wait(boost::bind(&LtpUdpEngineManager::SocketRestored_TimerExpired, this, boost::asio::placeholders::error));
        StartUdpReceive();
    }
}

void LtpUdpEngineManager::SocketRestored_TimerExpired(const boost::system::error_code& e) {
    if (e != boost::asio::error::operation_aborted) {
        // Timer was not cancelled, take necessary action.
        std::cout << "Notice: LtpUdpEngineManager socket back to receiving" << std::endl;
        m_readyToForward = true;
    }
}




void LtpUdpEngineManager::DoUdpShutdown() {
    //final code to shut down tcp sockets
    m_readyToForward = false;
    m_retryAfterSocketErrorTimer.cancel();
    m_socketRestoredTimer.cancel();
    if (m_udpSocket.is_open()) {
        try {
            std::cout << "closing LtpUdpEngineManager UDP socket.." << std::endl;
            m_udpSocket.close();
        }
        catch (const boost::system::system_error & e) {
            std::cout << "notice in LtpUdpEngineManager::DoUdpShutdown calling udpSocket.close: " << e.what() << std::endl;
        }
    }
    m_mapRemoteEngineIdToLtpUdpEngineReceiverPtr.clear();
    m_mapRemoteEngineIdToLtpUdpEngineTransmitterPtr.clear();
}

bool LtpUdpEngineManager::ReadyToForward() {
    return m_readyToForward;
}

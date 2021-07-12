#include "LtpUdpEngineManager.h"
#include <boost/make_unique.hpp>
#include <boost/lexical_cast.hpp>
#include "Sdnv.h"

std::map<uint64_t, std::unique_ptr<LtpUdpEngineManager> > LtpUdpEngineManager::m_staticMapBoundPortToLtpUdpEngineManagerPtr;
boost::mutex LtpUdpEngineManager::m_staticMutex;


LtpUdpEngineManager * LtpUdpEngineManager::GetOrCreateInstance(const uint16_t myBoundUdpPort) {
    boost::mutex::scoped_lock theLock(m_staticMutex);
    std::map<uint64_t, std::unique_ptr<LtpUdpEngineManager> >::iterator it = m_staticMapBoundPortToLtpUdpEngineManagerPtr.find(myBoundUdpPort);
    if (it == m_staticMapBoundPortToLtpUdpEngineManagerPtr.end()) {
        LtpUdpEngineManager * const engineManagerPtr = new LtpUdpEngineManager(myBoundUdpPort);
        m_staticMapBoundPortToLtpUdpEngineManagerPtr[myBoundUdpPort] = std::unique_ptr<LtpUdpEngineManager>(engineManagerPtr);
        return engineManagerPtr;
    }
    return it->second.get();
}

LtpUdpEngineManager::LtpUdpEngineManager(const uint16_t myBoundUdpPort) :
    M_MY_BOUND_UDP_PORT(myBoundUdpPort),
    m_resolver(m_ioServiceUdp),
    m_udpSocket(m_ioServiceUdp),
    m_udpReceiveBuffer(UINT16_MAX),
    m_readyToForward(false)
{
    //Receiver UDP
    try {
        m_udpSocket.open(boost::asio::ip::udp::v4());
        m_udpSocket.bind(boost::asio::ip::udp::endpoint(boost::asio::ip::udp::v4(), M_MY_BOUND_UDP_PORT)); // //if udpPort is 0 then bind to random ephemeral port
    }
    catch (const boost::system::system_error & e) {
        std::cerr << "Could not bind on UDP port " << M_MY_BOUND_UDP_PORT << std::endl;
        std::cerr << "  Error: " << e.what() << std::endl;

        return;
    }
    printf("LtpUdpEngineManager bound successfully on UDP port %d ...", m_udpSocket.local_endpoint().port());
    
    Start(); //TODO EVALUATE IF AUTO START SAFE
}

//call Start after all UDP engines have been added
void LtpUdpEngineManager::Start() {
    StartUdpReceive(); //call before creating io_service thread so that it has "work"

    m_ioServiceUdpThreadPtr = boost::make_unique<boost::thread>(boost::bind(&boost::asio::io_service::run, &m_ioServiceUdp));
}

LtpUdpEngine * LtpUdpEngineManager::GetLtpUdpEnginePtr(const uint64_t expectedSessionOriginatorEngineId, const bool isInduct) {
    const std::pair<uint64_t, bool> mapKey(expectedSessionOriginatorEngineId, isInduct);
    std::map<std::pair<uint64_t, bool>, std::unique_ptr<LtpUdpEngine> >::iterator it = m_mapSessionOriginatorEngineIdPlusIsInductToLtpUdpEnginePtr.find(mapKey);
    return (it == m_mapSessionOriginatorEngineIdPlusIsInductToLtpUdpEnginePtr.end()) ? NULL : it->second.get();
}

void LtpUdpEngineManager::RemoveLtpUdpEngine_ThreadSafe(const uint64_t expectedSessionOriginatorEngineId, const bool isInduct, const boost::function<void()> & callback) {
    boost::asio::post(m_ioServiceUdp, boost::bind(&LtpUdpEngineManager::RemoveLtpUdpEngine_NotThreadSafe, this, expectedSessionOriginatorEngineId, isInduct, callback));
}
void LtpUdpEngineManager::RemoveLtpUdpEngine_NotThreadSafe(const uint64_t expectedSessionOriginatorEngineId, const bool isInduct, const boost::function<void()> & callback) {
    const std::pair<uint64_t, bool> mapKey(expectedSessionOriginatorEngineId, isInduct);
    std::map<std::pair<uint64_t, bool>, std::unique_ptr<LtpUdpEngine> >::iterator it = m_mapSessionOriginatorEngineIdPlusIsInductToLtpUdpEnginePtr.find(mapKey);
    if (it == m_mapSessionOriginatorEngineIdPlusIsInductToLtpUdpEnginePtr.end()) {
        std::cerr << "error in LtpUdpEngineManager::RemoveLtpUdpEngine_NotThreadSafe: expectedSessionOriginatorEngineId " << expectedSessionOriginatorEngineId
            << " for type " << ((isInduct) ? "induct" : "outduct") << " does not exist" << std::endl;
    }
    else {
        m_mapSessionOriginatorEngineIdPlusIsInductToLtpUdpEnginePtr.erase(it);
        std::cout << "expectedSessionOriginatorEngineId " << expectedSessionOriginatorEngineId << " for type " << ((isInduct) ? "induct" : "outduct") << " successfully removed" << std::endl;
    }
    if (callback) {
        callback();
    }
}

bool LtpUdpEngineManager::AddLtpUdpEngine(const uint64_t thisEngineId, const uint64_t expectedSessionOriginatorEngineId, const bool isInduct, const uint64_t mtuClientServiceData, uint64_t mtuReportSegment,
    const boost::posix_time::time_duration & oneWayLightTime, const boost::posix_time::time_duration & oneWayMarginTime,
    const std::string & remoteHostname, const uint16_t remotePort, const unsigned int numUdpRxCircularBufferVectors,
    const uint64_t ESTIMATED_BYTES_TO_RECEIVE_PER_SESSION, uint32_t checkpointEveryNthDataPacketSender, uint32_t maxRetriesPerSerialNumber, const bool force32BitRandomNumbers)
{   
    if (isInduct) {
        if (thisEngineId == expectedSessionOriginatorEngineId) {
            std::cerr << "error in LtpUdpEngineManager::AddLtpUdpEngine: trying to add induct LtpUdpEngine but thisEngineId(" << thisEngineId
                << ") == expectedSessionOriginatorEngineId" << std::endl;
            return false;
        }
    }
    else { //outduct
        if (thisEngineId != expectedSessionOriginatorEngineId) {
            std::cerr << "error in LtpUdpEngineManager::AddLtpUdpEngine: trying to add outduct LtpUdpEngine but thisEngineId(" << thisEngineId
                << ") == expectedSessionOriginatorEngineId(" << expectedSessionOriginatorEngineId << ")" << std::endl;
            return false;
        }
    }
    //std::map<uint64_t, std::unique_ptr<LtpUdpEngine> > m_mapSessionOriginatorEngineIdToLtpUdpEnginePtr;
    const std::pair<uint64_t, bool> mapKey(expectedSessionOriginatorEngineId, isInduct);
    if (m_mapSessionOriginatorEngineIdPlusIsInductToLtpUdpEnginePtr.find(mapKey) != m_mapSessionOriginatorEngineIdPlusIsInductToLtpUdpEnginePtr.end()) {
        std::cerr << "error in LtpUdpEngineManager::AddLtpUdpEngine: engine Id " << thisEngineId 
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

    m_mapSessionOriginatorEngineIdPlusIsInductToLtpUdpEnginePtr[mapKey] = boost::make_unique<LtpUdpEngine>(m_ioServiceUdp,
        m_udpSocket, thisEngineId, mtuClientServiceData, mtuReportSegment, oneWayLightTime, oneWayMarginTime,
        remoteEndpoint, numUdpRxCircularBufferVectors, ESTIMATED_BYTES_TO_RECEIVE_PER_SESSION, checkpointEveryNthDataPacketSender,
        maxRetriesPerSerialNumber, force32BitRandomNumbers);
    
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
            std::cerr << "critical error in LtpUdpEngine::HandleUdpReceive(): received invalid ltp packet with segment type flag " << (int)segmentTypeFlags << std::endl;
            DoUdpShutdown();
            return;
        }
        
        uint8_t sdnvSize;
        const uint64_t sessionOriginatorEngineId = SdnvDecodeU64(&m_udpReceiveBuffer[1], &sdnvSize); //no worries about hardware accelerated sdnv read out of bounds due to vector size UINT16_MAX
        if (sdnvSize == 0) {
            std::cerr << "error in LtpUdpEngineManager::HandleUdpReceive(): cannot read sessionOriginatorEngineId.. ignoring packet" << std::endl;
            StartUdpReceive();
            return;
        }

        const std::pair<uint64_t, bool> mapKey(sessionOriginatorEngineId, isSenderToReceiver); //isSenderToReceiver == isInduct
        std::map<std::pair<uint64_t, bool>, std::unique_ptr<LtpUdpEngine> >::iterator it = m_mapSessionOriginatorEngineIdPlusIsInductToLtpUdpEnginePtr.find(mapKey);
        if (it == m_mapSessionOriginatorEngineIdPlusIsInductToLtpUdpEnginePtr.end()) {
            std::cerr << "error in LtpUdpEngineManager::HandleUdpReceive: an " << ((isSenderToReceiver) ? "induct" : "outduct") << " received packet with unknown engine Id "
                << sessionOriginatorEngineId << ".. ignoring packet" << std::endl;
            StartUdpReceive();
            return;
        }

        it->second->PostPacketFromManager_ThreadSafe(m_udpReceiveBuffer, bytesTransferred);
        if (m_udpReceiveBuffer.size() != UINT16_MAX) {
            std::cerr << "error in LtpUdpEngineManager::HandleUdpReceive: swapped packet not size UINT16_MAX... resizing" << std::endl;
            m_udpReceiveBuffer.resize(UINT16_MAX);
        }
        StartUdpReceive(); //restart operation only if there was no error
    }
    else if (error != boost::asio::error::operation_aborted) {
        std::cerr << "critical error in LtpUdpEngine::HandleUdpReceive(): " << error.message() << std::endl;
        DoUdpShutdown();
    }
}




void LtpUdpEngineManager::DoUdpShutdown() {
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
    m_mapSessionOriginatorEngineIdPlusIsInductToLtpUdpEnginePtr.clear();
}

bool LtpUdpEngineManager::ReadyToForward() {
    return m_readyToForward;
}

#ifndef _LTP_UDP_ENGINE_MANAGER_H
#define _LTP_UDP_ENGINE_MANAGER_H 1

#include <string>
#include <boost/thread.hpp>
#include <boost/asio.hpp>
#include <vector>
#include <map>
#include "LtpUdpEngine.h"

//Every "link" should have a unique engine ID, managed by using the remote eid that the link will be connecting to as the engine id for LTP
//We track a link as a paired induct/outduct and for each link there is one engine id
class LtpUdpEngineManager {
private:
    LtpUdpEngineManager();
    LtpUdpEngineManager(const uint16_t myBoundUdpPort); //LtpUdpEngineManager can only be created by the GetOrCreateInstance() function
public:
    ~LtpUdpEngineManager();

    void Start();

    bool AddLtpUdpEngine(const uint64_t thisEngineId, const uint64_t expectedSessionOriginatorEngineId, const bool isInduct, const uint64_t mtuClientServiceData, uint64_t mtuReportSegment,
        const boost::posix_time::time_duration & oneWayLightTime, const boost::posix_time::time_duration & oneWayMarginTime,
        const std::string & remoteHostname, const uint16_t remotePort, const unsigned int numUdpRxCircularBufferVectors = 100,
        const uint64_t ESTIMATED_BYTES_TO_RECEIVE_PER_SESSION = 0, uint32_t checkpointEveryNthDataPacketSender = 0,
        uint32_t maxRetriesPerSerialNumber = 5, const bool force32BitRandomNumbers = false);

    LtpUdpEngine * GetLtpUdpEnginePtr(const uint64_t engineId, const bool isInduct);
    void RemoveLtpUdpEngine_ThreadSafe(const uint64_t engineId, const bool isInduct, const boost::function<void()> & callback);
    void RemoveLtpUdpEngine_NotThreadSafe(const uint64_t engineId, const bool isInduct, const boost::function<void()> & callback);
    void Stop();
    void DoUdpShutdown();
    
    bool ReadyToForward();
private:
    void StartUdpReceive();
    void HandleUdpReceive(const boost::system::error_code & error, std::size_t bytesTransferred);
public:
    static LtpUdpEngineManager * GetOrCreateInstance(const uint16_t myBoundUdpPort);
private:
    //LtpUdpEngineManager(); 
    static std::map<uint64_t, std::unique_ptr<LtpUdpEngineManager> > m_staticMapBoundPortToLtpUdpEngineManagerPtr;
    static boost::mutex m_staticMutex;
    
    


    const uint16_t M_MY_BOUND_UDP_PORT;
    boost::asio::io_service m_ioServiceUdp;
    boost::asio::ip::udp::resolver m_resolver;
    boost::asio::ip::udp::socket m_udpSocket;
    boost::asio::ip::udp::endpoint m_udpDestinationResolvedEndpointDataSourceToDataSink;
    std::unique_ptr<boost::thread> m_ioServiceUdpThreadPtr;

    
    std::vector<boost::uint8_t> m_udpReceiveBuffer;
    boost::asio::ip::udp::endpoint m_remoteEndpointReceived;
    std::map<std::pair<uint64_t, bool>, std::unique_ptr<LtpUdpEngine> > m_mapSessionOriginatorEngineIdPlusIsInductToLtpUdpEnginePtr;

    volatile bool m_readyToForward;
public:
};



#endif //_LTP_UDP_ENGINE_MANAGER_H

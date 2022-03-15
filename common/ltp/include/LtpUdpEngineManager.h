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
    LTP_LIB_EXPORT LtpUdpEngineManager(const uint16_t myBoundUdpPort, const bool autoStart); //LtpUdpEngineManager can only be created by the GetOrCreateInstance() function
public:
    LTP_LIB_EXPORT ~LtpUdpEngineManager();

    /** First bind the LTP UDP socket to myBoundUdpPort from the constructor.
     * If binding succeeds, start the LTP udp socket io_service thread and start UDP asynchronous receiving.
     * It is recommended this be called after all UDP engines have been added in case remote peers are already sending UDP packets to this manager.
     *
     * @return True if the operation completed successfully (or completed successfully in the past).
     * Subsequent calls to StartIfNotAlreadyRunning() will succeed if the first call to StartIfNotAlreadyRunning() succeeded.
     * False if the operation failed (socket didn't bind).
     * @post If and only if this is the first call, then the socket is bound, a dedicated io_service thread for the udp socket is running,
     * and the udp socket is listening for incoming packets on the bound port.
     */
    LTP_LIB_EXPORT bool StartIfNotAlreadyRunning();

    LTP_LIB_EXPORT bool AddLtpUdpEngine(const uint64_t thisEngineId, const uint64_t remoteEngineId, const bool isInduct, const uint64_t mtuClientServiceData, uint64_t mtuReportSegment,
        const boost::posix_time::time_duration & oneWayLightTime, const boost::posix_time::time_duration & oneWayMarginTime,
        const std::string & remoteHostname, const uint16_t remotePort, const unsigned int numUdpRxCircularBufferVectors,
        const uint64_t ESTIMATED_BYTES_TO_RECEIVE_PER_SESSION, const uint64_t maxRedRxBytesPerSession, uint32_t checkpointEveryNthDataPacketSender,
        uint32_t maxRetriesPerSerialNumber, const bool force32BitRandomNumbers, const uint64_t maxSendRateBitsPerSecOrZeroToDisable);

    LTP_LIB_EXPORT LtpUdpEngine * GetLtpUdpEnginePtrByRemoteEngineId(const uint64_t remoteEngineId, const bool isInduct);
    LTP_LIB_EXPORT void RemoveLtpUdpEngineByRemoteEngineId_ThreadSafe(const uint64_t remoteEngineId, const bool isInduct, const boost::function<void()> & callback);
    LTP_LIB_EXPORT void RemoveLtpUdpEngineByRemoteEngineId_NotThreadSafe(const uint64_t remoteEngineId, const bool isInduct, const boost::function<void()> & callback);
    LTP_LIB_EXPORT void Stop();
    LTP_LIB_EXPORT void DoUdpShutdown();
    
    LTP_LIB_EXPORT bool ReadyToForward();
private:
    LTP_LIB_NO_EXPORT void StartUdpReceive();
    LTP_LIB_NO_EXPORT void HandleUdpReceive(const boost::system::error_code & error, std::size_t bytesTransferred);
public:
    LTP_LIB_EXPORT static std::shared_ptr<LtpUdpEngineManager> GetOrCreateInstance(const uint16_t myBoundUdpPort, const bool autoStart);
    LTP_LIB_EXPORT static void SetMaxUdpRxPacketSizeBytesForAllLtp(const uint64_t maxUdpRxPacketSizeBytesForAllLtp);
private:
    //LtpUdpEngineManager(); 
    static std::map<uint16_t, std::weak_ptr<LtpUdpEngineManager> > m_staticMapBoundPortToLtpUdpEngineManagerPtr;
    static boost::mutex m_staticMutex;
    static uint64_t M_STATIC_MAX_UDP_RX_PACKET_SIZE_BYTES_FOR_ALL_LTP_UDP_ENGINES;
    


    const uint16_t M_MY_BOUND_UDP_PORT;
    boost::asio::io_service m_ioServiceUdp;
    boost::asio::ip::udp::resolver m_resolver;
    boost::asio::ip::udp::socket m_udpSocket;
    boost::asio::ip::udp::endpoint m_udpDestinationResolvedEndpointDataSourceToDataSink;
    std::unique_ptr<boost::thread> m_ioServiceUdpThreadPtr;

    
    std::vector<boost::uint8_t> m_udpReceiveBuffer;
    boost::asio::ip::udp::endpoint m_remoteEndpointReceived;
    //std::map<std::pair<uint64_t, bool>, std::unique_ptr<LtpUdpEngine> > m_mapSessionOriginatorEngineIdPlusIsInductToLtpUdpEnginePtr;
    std::map<uint64_t, std::unique_ptr<LtpUdpEngine> > m_mapRemoteEngineIdToLtpUdpEngineReceiverPtr; //inducts (differentiate by remote engine id using this map)
    std::map<uint64_t, std::unique_ptr<LtpUdpEngine> > m_mapRemoteEngineIdToLtpUdpEngineTransmitterPtr; //outducts (differentiate by engine index encoded into the session number, cannot use this map)
    std::vector<LtpUdpEngine*> m_vecEngineIndexToLtpUdpEngineTransmitterPtr;
    unsigned int m_nextEngineIndex;

    volatile bool m_readyToForward;
public:
};



#endif //_LTP_UDP_ENGINE_MANAGER_H

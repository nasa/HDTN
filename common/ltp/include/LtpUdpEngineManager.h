/**
 * @file LtpUdpEngineManager.h
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
 *
 * @section DESCRIPTION
 *
 * This LtpUdpEngineManager class is singleton pattern class used to share
 * UDP sockets that bind to the same UDP port with various LtpUdpEngine(s).
 * It manages a bidirectional udp socket paired with its own boost::asio::io_service and thread.
 * It quickly examines the first few bytes of incoming UDP packets so that it can
 * route them to their proper LtpUdpEngine.
 */

#ifndef _LTP_UDP_ENGINE_MANAGER_H
#define _LTP_UDP_ENGINE_MANAGER_H 1

#include <string>
#include <boost/thread.hpp>
#include <boost/asio.hpp>
#include <vector>
#include <map>
#include "LtpUdpEngine.h"
#include "LtpEngineConfig.h"
#include <boost/core/noncopyable.hpp>

//Every "link" should have a unique engine ID, managed by using the remote eid that the link will be connecting to as the engine id for LTP
//We track a link as a paired induct/outduct and for each link there is one engine id
class LtpUdpEngineManager : private boost::noncopyable {
private:
    LtpUdpEngineManager() = delete;
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

    /** Add an LtpUdpEngine to the LtpUdpEngineManager.
     * If bidirectionallity is desired (receiving client service data in both directions), call this function twice
     * with isInduct set to True in one call and False in the other call.
     * A max of 254 engines can be added for one outduct with the same udp port.
     * 
     * @return True if the operation completed successfully (or false otherwise).
     */
    LTP_LIB_EXPORT bool AddLtpUdpEngine(const LtpEngineConfig& ltpRxOrTxCfg);

    LTP_LIB_EXPORT LtpUdpEngine * GetLtpUdpEnginePtrByRemoteEngineId(const uint64_t remoteEngineId, const bool isInduct);
    LTP_LIB_EXPORT void RemoveLtpUdpEngineByRemoteEngineId_ThreadSafe(const uint64_t remoteEngineId, const bool isInduct, const boost::function<void()> & callback);
    LTP_LIB_EXPORT void RemoveLtpUdpEngineByRemoteEngineId_NotThreadSafe(const uint64_t remoteEngineId, const bool isInduct, const boost::function<void()> & callback);
    LTP_LIB_EXPORT void Stop();
    LTP_LIB_EXPORT void DoUdpShutdown();
    
    LTP_LIB_EXPORT bool ReadyToForward();
private:
    LTP_LIB_NO_EXPORT void StartUdpReceive();
    LTP_LIB_NO_EXPORT void HandleUdpReceive(const boost::system::error_code & error, std::size_t bytesTransferred);
    LTP_LIB_NO_EXPORT void OnRetryAfterSocketError_TimerExpired(const boost::system::error_code& e);
    LTP_LIB_NO_EXPORT void SocketRestored_TimerExpired(const boost::system::error_code& e);
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
    boost::asio::deadline_timer m_retryAfterSocketErrorTimer;
    boost::asio::deadline_timer m_socketRestoredTimer;
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

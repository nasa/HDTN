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

    /** Add an LtpUdpEngine to the LtpUdpEngineManager.  If bidirectionallity is desired (receiving client service data in both directions), call this function twice
     * with isInduct set to True in one call and False in the other call.  A max of 254 engines can be added for one outduct with the same udp port.
     * @param thisEngineId This LTP engine's engine ID.
     * @param remoteEngineId The LTP remote engine ID.
     * @param isInduct True if this Engine will be an LTP receiver.  False if it will be an LTP transmitter.
     * @param mtuClientServiceData  The max size of the data portion (excluding LTP headers and UDP headers and IP headers) of an LTP sender's Red data segment being sent.
     * Set this low enough to avoid exceeding ethernet MTU to avoid IP fragmentation.
     * @param mtuReportSegment The max size of the data portion (excluding LTP headers and UDP headers and IP headers) of an LTP receiver's report segment being sent.
     * Set this low enough to avoid exceeding ethernet MTU to avoid IP fragmentation.
     * @param oneWayLightTime The one way light time.  Round trip time (retransmission time) is computed by (2 * (oneWayLightTime + oneWayMarginTime)).
     * @param oneWayMarginTime The one way margin (packet processing) time.  Round trip time (retransmission time) is computed by (2 * (oneWayLightTime + oneWayMarginTime)).
     * @param remoteHostname The remote IP address or hostname of the sender or receiver.
     * @param remotePort The remote UDP port of the sender or receiver.
     * @param numUdpRxCircularBufferVectors The max number of unprocessed LTP received UDP packets to buffer.
     * @param ESTIMATED_BYTES_TO_RECEIVE_PER_SESSION The number of Red data contiguous bytes to initialized on a receiver.
     * Make this large enough to accomidate the max Red data size so that
     * the Ltp receiver doesn't have to reallocate/copy/delete data while it is receiving Red data.
     * Make this small enough so that the system doesn't have to allocate too much extra memory per receiving session.
     * @param maxRedRxBytesPerSession A protection to prevent an LTP Red data segment with a huge memory offset from crashing the system.
     * Set this to the worst case largest Red data size for an LTP session.
     * @param checkpointEveryNthDataPacketSender Enables accelerated retransmission for an LTP sender by making every Nth UDP packet a checkpoint.
     * @param maxRetriesPerSerialNumber The max number of retries/resends of a single LTP packet with a serial number before the session is terminated.
     * @param force32BitRandomNumbers True will constrain LTP's headers containining SDNV random numbers to be CCSDS compliant 32-bit values.
     * False will allow LTP to generate 10-byte SDNV (64-bit values) random numbers.
     * @param maxSendRateBitsPerSecOrZeroToDisable Rate limiting UDP send rate in bits per second.
     * A zero value will send UDP packets as fast as the operating system will allow.
     * @param maxSimultaneousSessions The number of expected simultaneous LTP sessions for this engine (important to Ltp receivers),
     * used to initialize hash maps' bucket size for SessionNumberToSessionSender and SessionIdToSessionReceiver.
     * @param rxDataSegmentSessionNumberRecreationPreventerHistorySizeOrZeroToDisable The number of recent Ltp receiver history of session numbers to remember.
     * If an Ltp receiver's session has been closed and it receives a session number that's within the history, the receiver will refuse the session to prevent
     * a potentially old session from being reopened, which has been known to happen with IP fragmentation enabled.
     * @param maxUdpPacketsToSendPerSystemCall The max number of udp packets to send per system call.
     * If 1 is used, then the receiving udp socket is used to send udp packets from the specified bound port that it is on
     * and one boost::asio:::async_send_to is called per one udp packet to send.
     * If more than 1 is used, a dedicated sender udp socket is created and bound to a random ephemeral port,
     * the socket is then permanently "UDP connected" to the remoteHostname:remotePort,
     * and packets will be sent using this socket's sendmmsg on POSIX or LPFN_TRANSMITPACKETS on Windows.
     * @param senderPingSecondsOrZeroToDisable The number of seconds between ltp session sender pings during times of zero data segment activity.
     * An LTP ping is defined as a sender sending a cancel segment of a known non-existent session number to a receiver,
     * in which the receiver shall respond with a cancel ack in order to determine if the link is active.
     * A link down callback will be called if a cancel ack is not received after (RTT * maxRetriesPerSerialNumber).
     * This parameter should be set to zero for a receiver as there is currently no use case for a receiver to detect link-up.
     * @param delaySendingOfReportSegmentsTimeMsOrZeroToDisable The number of milliseconds the ltp engine
     * should wait for gaps to be filled.
     * When red part data is segmented and delivered to the receiving engine out-of-order,
     * the checkpoint(s) and EORP can be received before the earlier-in-block data segments.
     * If a synchronous report is sent immediately upon receiving the checkpoint there will be
     * data segments in-flight and about to be delivered that will be seen as reception gaps in the report.
     * Instead of sending the synchronous report immediately upon receiving a checkpoint segment
     * the receiving engine should wait this period of time before sending the report segment.
     * The delay time will reset upon any data segments which fill gaps.
     * This parameter should be set to zero for a sender.
     * @param delaySendingOfDataSegmentsTimeMsOrZeroToDisable The number of milliseconds the ltp engine
     * should wait after receiving a report segment before resending data segments.
     * This parameter should be set to zero for a receiver.
     * 
     * @return True if the operation completed successfully (or false otherwise).
     */
    LTP_LIB_EXPORT bool AddLtpUdpEngine(const uint64_t thisEngineId, const uint64_t remoteEngineId, const bool isInduct, const uint64_t mtuClientServiceData, uint64_t mtuReportSegment,
        const boost::posix_time::time_duration & oneWayLightTime, const boost::posix_time::time_duration & oneWayMarginTime,
        const std::string & remoteHostname, const uint16_t remotePort, const unsigned int numUdpRxCircularBufferVectors,
        const uint64_t ESTIMATED_BYTES_TO_RECEIVE_PER_SESSION, const uint64_t maxRedRxBytesPerSession, uint32_t checkpointEveryNthDataPacketSender,
        uint32_t maxRetriesPerSerialNumber, const bool force32BitRandomNumbers, const uint64_t maxSendRateBitsPerSecOrZeroToDisable, const uint64_t maxSimultaneousSessions,
        const uint64_t rxDataSegmentSessionNumberRecreationPreventerHistorySizeOrZeroToDisable,
        const uint64_t maxUdpPacketsToSendPerSystemCall, const uint64_t senderPingSecondsOrZeroToDisable,
        const uint64_t delaySendingOfReportSegmentsTimeMsOrZeroToDisable,
        const uint64_t delaySendingOfDataSegmentsTimeMsOrZeroToDisable);

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

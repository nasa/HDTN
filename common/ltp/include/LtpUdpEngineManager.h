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
    
    /**
     * Bind I/O components to m_ioServiceUdp.
     * Preallocate space in m_vecEngineIndexToLtpUdpEngineTransmitterPtr to cover the addressing space for indexing by engine index (2 ^ [8 engine index bytes]).
     * Set m_readyToForward to False to initialize engine manager as idle.
     * If autoStart is set to True, call LtpUdpEngineManager::StartIfNotAlreadyRunning() to start the engine manager.
     * @param myBoundUdpPort Our UDP port.
     * @param autoStart Whether the engine manager should start automatically on construction.
     */
    LTP_LIB_EXPORT LtpUdpEngineManager(const uint16_t myBoundUdpPort, const bool autoStart); //LtpUdpEngineManager can only be created by the GetOrCreateInstance() function
public:
    /// Call LtpUdpEngineManager::Stop() to release managed resources
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
     * A max of 7 engines can be added for one outduct with the same udp port.
     * 
     * @return True if the operation completed successfully (or false otherwise).
     */
    LTP_LIB_EXPORT bool AddLtpUdpEngine(const LtpEngineConfig& ltpRxOrTxCfg);

    /** Find registered engine by engine ID.
     *
     * @param remoteEngineId The engine ID.
     * @param isInduct Whether to search through inducts (or outducts).
     * @return A pointer to the engine if exists and is of the correct type indicated by isInduct, or NULL otherwise.
     */
    LTP_LIB_EXPORT LtpUdpEngine * GetLtpUdpEnginePtrByRemoteEngineId(const uint64_t remoteEngineId, const bool isInduct);
    
    /** Initiate a request to remove a registered engine by engine ID (thread-safe).
     *
     * Initiates an asynchronous request to LtpUdpEngineManager::RemoveLtpUdpEngineByRemoteEngineId_NotThreadSafe().
     * @param remoteEngineId The engine ID.
     * @param isInduct Whether to search through inducts (or outducts).
     * @param callback The callback to invoke on completion.
     */
    LTP_LIB_EXPORT void RemoveLtpUdpEngineByRemoteEngineId_ThreadSafe(const uint64_t remoteEngineId, const bool isInduct, const boost::function<void()> & callback);
    
    /** Remove a registered engine by engine ID.
     *
     * Removes the registered engine if exists and is of the correct type indicated by isInduct.
     * On removal, invalidates the cache if appropriate, then cleans up the remaining reference in m_vecEngineIndexToLtpUdpEngineTransmitterPtr.
     * Invokes callback on completion.
     * @param remoteEngineId The engine ID.
     * @param isInduct Whether to search through inducts (or outducts).
     * @param callback The callback to invoke on completion.
     * @post The necessary state held by the engine manager on the to-be-removed engine will have been cleaned up by the time callback is invoked.
     */
    LTP_LIB_EXPORT void RemoveLtpUdpEngineByRemoteEngineId_NotThreadSafe(const uint64_t remoteEngineId, const bool isInduct, const boost::function<void()> & callback);
    
    /** Perform engine manager shutdown.
     *
     * Initiates an asynchronous call to LtpUdpEngineManager::DoUdpShutdown() to release UDP resources, then releases all underlying I/O resources.
     * @post The object is ready to be reused after the next successful call to LtpUdpEngineManager::StartIfNotAlreadyRunning().
     */
    LTP_LIB_EXPORT void Stop();
   
    /** Perform engine manager shutdown.
     *
     * Stops timers, closes managed socket, sets engine manager to idle, clears registered engines and invalidates cache.
     * @post The LTP resources are ready to be reused, but the engine manager will remain in the running state until the next call to LtpUdpEngineManager::Stop().
     */
    LTP_LIB_EXPORT void DoUdpShutdown();

    /** Query whether the engine manager should be considered operational.
     *
     * @return True if the engine manager is marked as operational, or False otherwise.
     */
    LTP_LIB_EXPORT bool ReadyToForward();
private:
    /** Start the receive loop for the remote endpoint to receive from.
     *
     * Initiates an asynchronous receive operation with LtpUdpEngineManager::HandleUdpReceive() as a completion handler.
     */
    LTP_LIB_NO_EXPORT void StartUdpReceive();
    
    /** Handle a UDP receive operation.
     *
     * If a fatal error occurred during the receive operation, returns immediately letting m_ioServiceUdp run out of work and leaving the engine manager in an idle state.
     * In case of a recoverable error, the engine manager is set to idle, a link-down event is emitted to the underlying LtpUdpEngine with PostExternalLinkDownEvent_ThreadSafe(),
     * and to retry after a specified duration, the socket error timer is started asynchronously with LtpUdpEngineManager::OnRetryAfterSocketError_TimerExpired() as a completion handler.
     *
     * Attempts to parse the start an LTP header to get the session originator and session number, if packet is malformed it gets dropped and the receive loop starts again.
     * Parses the direction of the packet with Ltp::GetMessageDirectionFromSegmentFlags().
     * 1. When (sender -> receiver), we have received a message type that only travels from an outduct (sender) to an induct (receiver),
     * the session originator value can be used directly to search through the cache or registered inducts to access the referenced engine.
     * 2. When (receiver -> sender), we have received a message type that only travels from an induct (receiver) to an outduct (sender),
     * the session originator value is our engine ID, to get a proper identifier we can use we need to feed the session number to LtpRandomNumberGenerator::GetEngineIndexFromRandomSessionNumber(),
     * the returned engine index can then be used directly to index the outduct pointer vector to access the referenced engine.
     *
     * On successful processing, signals the underlying LtpUdpEngine that a packet has been received with PostPacketFromManager_ThreadSafe(),
     * then calls LtpUdpEngineManager::StartUdpReceive() to achieve a receive loop.
     * @param error The error code.
     * @param bytesTransferred The number of bytes received.
     */
    LTP_LIB_NO_EXPORT void HandleUdpReceive(const boost::system::error_code & error, std::size_t bytesTransferred);
    
    /** Handle socket error retry timer expiry.
     *
     * If the expiry occurred due to the timer being manually cancelled, returns immediately.
     * Else, starts the restore from quarantine timer asynchronously with LtpUdpEngineManager::SocketRestored_TimerExpired(),
     * then calls LtpUdpEngineManager::StartUdpReceive() to try and resume the receive loop.
     * @param e The error code.
     */
    LTP_LIB_NO_EXPORT void OnRetryAfterSocketError_TimerExpired(const boost::system::error_code& e);
    
    /** Handle restore from quarantine timer expiry.
     *
     * If the expiry occurred due to the timer being manually cancelled, returns immediately.
     * Else, this indicates that the engine has not experienced any socket errors and has been running normally for the duration the timer was active
     * and can now safely be considered operational and marked as such for external services to then be able to query and resume their own operations.
     * @param e The error code.
     */
    LTP_LIB_NO_EXPORT void SocketRestored_TimerExpired(const boost::system::error_code& e);
public:
    /** Get or create an engine manager instance.
     *
     * @param myBoundUdpPort The UDP port already managed or to-be-bound by the returned engine manager instance.
     * @param autoStart Whether a new instance should be automatically started on construction, does NOT affect existing instances.
     * @return A shared pointer to the existing or newly-created engien manager instance.
     */
    LTP_LIB_EXPORT static std::shared_ptr<LtpUdpEngineManager> GetOrCreateInstance(const uint16_t myBoundUdpPort, const bool autoStart);
    
    /** Set the maximum UDP packet size in bytes across all registered engines.
     *
     * Needs to be called at least once in the lifetime of the program before starting any external I/O.
     * @param maxUdpRxPacketSizeBytesForAllLtp The maximum UDP packet size in bytes to set.
     */
    LTP_LIB_EXPORT static void SetMaxUdpRxPacketSizeBytesForAllLtp(const uint64_t maxUdpRxPacketSizeBytesForAllLtp);
private:
    //LtpUdpEngineManager();
    /// Registered engine managers, mapped by bound port
    static std::map<uint16_t, std::weak_ptr<LtpUdpEngineManager> > m_staticMapBoundPortToLtpUdpEngineManagerPtr;
    /// Engine manger registry mutex
    static boost::mutex m_staticMutex;
    /// Maximum UDP packet size in bytes, applies to all registered engines
    static uint64_t M_STATIC_MAX_UDP_RX_PACKET_SIZE_BYTES_FOR_ALL_LTP_UDP_ENGINES;
    

    /// Our managed UDP socket port, if the port number 0 the socket is bound to a random ephemeral port
    const uint16_t M_MY_BOUND_UDP_PORT;
    /// I/O execution context
    boost::asio::io_service m_ioServiceUdp;
    /// UDP endpoint resolver
    boost::asio::ip::udp::resolver m_resolver;
    /// Our managed UDP socket
    boost::asio::ip::udp::socket m_udpSocket;
    
    /// Socket error retry timer, the time to wait before retrying failed operation
    boost::asio::deadline_timer m_retryAfterSocketErrorTimer;
    /// Socket restore from quarantine timer, the duration without experiencing socket errors required before the engine manager is marked as operational
    boost::asio::deadline_timer m_socketRestoredTimer;
    /// ...
    boost::asio::ip::udp::endpoint m_udpDestinationResolvedEndpointDataSourceToDataSink;
    /// Thread that invokes m_ioServiceUdp.run()
    std::unique_ptr<boost::thread> m_ioServiceUdpThreadPtr;

    
    /// Packet receive buffer
    std::vector<boost::uint8_t> m_udpReceiveBuffer;
    /// Remote UDP endpoint to receive from
    boost::asio::ip::udp::endpoint m_remoteEndpointReceived;
    //std::map<std::pair<uint64_t, bool>, std::unique_ptr<LtpUdpEngine> > m_mapSessionOriginatorEngineIdPlusIsInductToLtpUdpEnginePtr;
    /// Registered inducts, mapped by engine ID
    std::map<uint64_t, LtpUdpEngine> m_mapRemoteEngineIdToLtpUdpEngineReceiver; //inducts (differentiate by remote engine id using this map)
    /// Cached iterator of induct, references m_mapRemoteEngineIdToLtpUdpEngineReceiver, can be invalid/stale do check before use
    std::map<uint64_t, LtpUdpEngine>::iterator m_cachedItRemoteEngineIdToLtpUdpEngineReceiver;
    /// Registered outducts, mapped by engine ID
    std::map<uint64_t, LtpUdpEngine> m_mapRemoteEngineIdToLtpUdpEngineTransmitter; //outducts (differentiate by engine index encoded into the session number, cannot use this map)
    /// Registered engines bound to our port, indexed by engine index, the index comes from parsing the session number with LtpRandomNumberGenerator::GetEngineIndexFromRandomSessionNumber()
    std::vector<LtpUdpEngine*> m_vecEngineIndexToLtpUdpEngineTransmitterPtr;
    /// Engine index to assign to the next registered outduct
    unsigned int m_nextEngineIndex;

    /// Whether the engine manager should currently be considered operational
    volatile bool m_readyToForward;
public:
};



#endif //_LTP_UDP_ENGINE_MANAGER_H

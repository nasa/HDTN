/**
 * @file UdpBatchSender.h
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
 * https://stackoverflow.com/questions/69729835/how-to-avoid-combining-multiple-buffers-into-one-udp-packet-with-function-call-w
 * https://github.com/microsoft/Windows-classic-samples/blob/main/Samples/Win7Samples/netds/Qos/Qos2/qossample.c
 * This UdpBatchSender class encapsulates the appropriate UDP functionality
 * to send multiple UDP packets in one system call in order to increase UDP throughput.
 * It also benefits in performance because the UDP socket must be "connected".
 */

#ifndef _UDP_BATCH_SENDER_H
#define _UDP_BATCH_SENDER_H 1

#include <string>
#include <boost/thread.hpp>
#include <boost/asio.hpp>
#include "hdtn_util_export.h"
#include "LtpClientServiceDataToSend.h"
#include <memory>

class UdpBatchSender {
public:
    /**
     * @typedef OnSentPacketsCallback_t Type of callback to be invoked after a packet batch send operation
     */
    typedef boost::function<void(bool success, std::vector<std::vector<boost::asio::const_buffer> >& constBufferVecs,
        std::vector<std::shared_ptr<std::vector<std::vector<uint8_t> > > >& underlyingDataToDeleteOnSentCallbackVec,
        std::vector<std::shared_ptr<LtpClientServiceDataToSend> >& underlyingCsDataToDeleteOnSentCallbackVec)> OnSentPacketsCallback_t;
    
    /// Bind socket to io_service, does not open the socket
    HDTN_UTIL_EXPORT UdpBatchSender();
    
    /// Call UdpBatchSender::Stop() to release managed resources
    HDTN_UTIL_EXPORT ~UdpBatchSender();
    
    /** Perform graceful shutdown.
     *
     * If no previous successful call to UdpBatchSender::Init(), returns immediately.
     * Else, tries to perform graceful shutdown on the socket, then releases all underlying I/O resources.
     * @post The object is ready to be reused after the next successful call to UdpBatchSender::Init().
     */
    HDTN_UTIL_EXPORT void Stop();
    
    /** Initialize the underlying I/O and connect to given host at given port.
     *
     * @param remoteHostname The remote host to connect to.
     * @param remotePort The remote port to connect to.
     * @return True if the connection could be established, or False if connection failed or the object has already been initialized.
     */
    HDTN_UTIL_EXPORT bool Init(const std::string& remoteHostname, const uint16_t remotePort);
    
    /** Initialize the underlying I/O and connect to given UDP endpoint.
     *
     * @param udpDestinationEndpoint The remote UDP endpoint to connect to.
     * @return True if the connection could be established, or False if connection failed or the object has already been initialized.
     */
    HDTN_UTIL_EXPORT bool Init(const boost::asio::ip::udp::endpoint & udpDestinationEndpoint);
    
    /** Get the current UDP endpoint.
     *
     * @return The current UDP endpoint.
     */
    HDTN_UTIL_EXPORT boost::asio::ip::udp::endpoint GetCurrentUdpEndpoint() const;

    /** Initiate a packet batch send operation (thread-safe).
     *
     * Initiates an asynchronous request to UdpBatchSender::PerformSendPacketsOperation().
     * @param constBufferVecs The packets to send.
     * @param underlyingDataToDeleteOnSentCallbackVec (internal) Vector of LtpClientServiceDataToSend::underlyingDataToDeleteOnSentCallback.
     * @param underlyingCsDataToDeleteOnSentCallbackVec (internal) Vector of LtpClientServiceDataToSend::underlyingCsDataToDeleteOnSentCallback.
     */
    HDTN_UTIL_EXPORT void QueueSendPacketsOperation_ThreadSafe(std::vector<std::vector<boost::asio::const_buffer> >& constBufferVecs,
        std::vector<std::shared_ptr<std::vector<std::vector<uint8_t> > > >& underlyingDataToDeleteOnSentCallbackVec,
        std::vector<std::shared_ptr<LtpClientServiceDataToSend> >& underlyingCsDataToDeleteOnSentCallbackVec);
    
    /** Set the onSentPackets callback.
     *
     * @param callback The callback to set.
     */
    HDTN_UTIL_EXPORT void SetOnSentPacketsCallback(const OnSentPacketsCallback_t& callback);
    
    /** Initiate a request to connect (thread-safe).
     *
     * Initiates an asynchronous request to UdpBatchSender::SetEndpointAndReconnect().
     * @param remoteEndpoint The remote UDP endpoint to connect to.
     */
    HDTN_UTIL_EXPORT void SetEndpointAndReconnect_ThreadSafe(const boost::asio::ip::udp::endpoint& remoteEndpoint);
    
    /** Initiate a request to connect (thread-safe).
     *
     * Initiates an asynchronous request to UdpBatchSender::SetEndpointAndReconnect().
     * @param remoteHostname The remote host to connect to.
     * @param remotePort The remote port to connect to.
     */
    HDTN_UTIL_EXPORT void SetEndpointAndReconnect_ThreadSafe(const std::string& remoteHostname, const uint16_t remotePort);

private:
    /** Connect to given UDP endpoint.
     *
     * @param remoteEndpoint The remote UDP endpoint to connect to.
     * @return True if the connection could be established, or False otherwise.
     */
    HDTN_UTIL_NO_EXPORT bool SetEndpointAndReconnect(const boost::asio::ip::udp::endpoint& remoteEndpoint);
    
    /** Connect to given host at given port.
     *
     * @param remoteHostname The remote host to connect to.
     * @param remotePort The remote port to connect to.
     * @return True if the connection could be established, or False otherwise.
     */
    HDTN_UTIL_NO_EXPORT bool SetEndpointAndReconnect(const std::string& remoteHostname, const uint16_t remotePort);
    
    /** Perform a packet batch send operation.
     *
     * Performs a single synchronous batch send operation.
     * After it finishes (successfully or otherwise), the callback stored in m_onSentPacketsCallback (if any) is invoked.
     * Arguments underlyingDataToDeleteOnSentCallbackVec and underlyingCsDataToDeleteOnSentCallbackVec are resources
     * managed by LtpClientServiceDataToSend, we should simply forward them as-is.
     * @param constBufferVecs The packets to send.
     * @param underlyingDataToDeleteOnSentCallbackVec (internal) Vector of LtpClientServiceDataToSend::underlyingDataToDeleteOnSentCallback.
     * @param underlyingCsDataToDeleteOnSentCallbackVec (internal) Vector of LtpClientServiceDataToSend::underlyingCsDataToDeleteOnSentCallback.
     */
    HDTN_UTIL_NO_EXPORT void PerformSendPacketsOperation(
        std::vector<std::vector<boost::asio::const_buffer> >& constBufferVecs,
        std::vector<std::shared_ptr<std::vector<std::vector<uint8_t> > > >& underlyingDataToDeleteOnSentCallbackVec,
        std::vector<std::shared_ptr<LtpClientServiceDataToSend> >& underlyingCsDataToDeleteOnSentCallbackVec);
    
    /** Initiate request for socket shutdown.
     *
     * Initiates an asynchronous request to UdpBatchSender::DoHandleSocketShutdown().
     */
    HDTN_UTIL_NO_EXPORT void DoUdpShutdown();
    
    /** Perform socket shutdown.
     *
     * If a connection is not open, returns immediately.
     * @post The underlying socket mechanism is ready to be reused after a successful call to UdpBatchSender::SetEndpointAndReconnect().
     */
    HDTN_UTIL_NO_EXPORT void DoHandleSocketShutdown();
private:

    /// I/O execution context
    boost::asio::io_service m_ioService;
    /// Explicit work controller for m_ioService, allows for graceful shutdown of running tasks
    std::unique_ptr<boost::asio::io_service::work> m_workPtr;
    /// UDP socket to connect to destination endpoint
    boost::asio::ip::udp::socket m_udpSocketConnectedSenderOnly;
    /// UDP destination endpoint
    boost::asio::ip::udp::endpoint m_udpDestinationEndpoint;
    /// Thread that invokes m_ioService.run()
    std::unique_ptr<boost::thread> m_ioServiceThreadPtr;
#ifdef _WIN32
//# define UDP_BATCH_SENDER_USE_OVERLAPPED 1
    /// WINAPI TransmitPackets function pointer
    LPFN_TRANSMITPACKETS m_transmitPacketsFunctionPointer;
# ifdef UDP_BATCH_SENDER_USE_OVERLAPPED
    /// Overlapped I/O object, always configured to auto-reset
    WSAOVERLAPPED m_sendOverlappedAutoReset;
# endif
    /// Vector of packets to send
    std::vector<TRANSMIT_PACKETS_ELEMENT> m_transmitPacketsElementVec;
#else //not #ifdef _WIN32
    /// Vector of packets to send
    std::vector<struct mmsghdr> m_transmitPacketsElementVec;
#endif
    
    /// Callback to invoke after a packet batch send operation
    OnSentPacketsCallback_t m_onSentPacketsCallback;
};



#endif //_UDP_BATCH_SENDER_H

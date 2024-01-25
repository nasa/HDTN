/**
 * @file UdpBatchSender.h
 * @author  Brian Tomko <brian.j.tomko@nasa.gov>
 *
 * @copyright Copyright � 2021 United States Government as represented by
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
 * Calls to io_service::run must be single threaded!
 */

#ifndef _UDP_BATCH_SENDER_H
#define _UDP_BATCH_SENDER_H 1

#include <string>
#include <boost/asio.hpp>
#include "hdtn_util_export.h"
#include "LtpClientServiceDataToSend.h"
#include <boost/core/noncopyable.hpp>
#include <memory>

class UdpBatchSender : private boost::noncopyable {
public:
    /**
     * @typedef OnSentPacketsCallback_t Type of callback to be invoked after a packet batch send operation
     */
    typedef boost::function<void(bool success,
        std::shared_ptr<std::vector<UdpSendPacketInfo> >& udpSendPacketInfoVecSharedPtr,
        const std::size_t numPacketsSent)> OnSentPacketsCallback_t;
    
    /// Bind socket to io_service, does not open the socket
    HDTN_UTIL_EXPORT UdpBatchSender(boost::asio::io_service& ioServiceSingleThreadedRef);
    
    /// Call UdpBatchSender::Stop() to release managed resources
    HDTN_UTIL_EXPORT ~UdpBatchSender();
    
    /** Perform graceful shutdown.
     *
     * If no previous successful call to UdpBatchSender::Init(), returns immediately.
     * Else, tries to perform graceful shutdown on the socket, then releases all underlying I/O resources.
     * @post The object is ready to be reused after the next successful call to UdpBatchSender::Init().
     */
    HDTN_UTIL_EXPORT void Stop();

    /** Perform a graceful shutdown from within the io_service thread,
     * perhaps called by a signal handler that uses the io_service..
     *
     * If no previous successful call to Init(), returns immediately.
     * Else, tries to perform graceful shutdown on the socket, then releases all underlying I/O resources.
     * @post The object is ready to be reused after the next successful call to Init().
     */
    HDTN_UTIL_EXPORT void Stop_CalledFromWithinIoServiceThread();
    
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
    HDTN_UTIL_EXPORT void QueueSendPacketsOperation_ThreadSafe(
        std::shared_ptr<std::vector<UdpSendPacketInfo> >&& udpSendPacketInfoVecSharedPtr, const std::size_t numPacketsToSend);

    /** Initiate a packet batch send operation, called ONLY from within the same io_service::run thread (not thread-safe).
     *
     * Initiates an asynchronous request to UdpBatchSender::PerformSendPacketsOperation().
     * @param constBufferVecs The packets to send.
     * @param underlyingDataToDeleteOnSentCallbackVec (internal) Vector of LtpClientServiceDataToSend::underlyingDataToDeleteOnSentCallback.
     * @param underlyingCsDataToDeleteOnSentCallbackVec (internal) Vector of LtpClientServiceDataToSend::underlyingCsDataToDeleteOnSentCallback.
     */
    HDTN_UTIL_EXPORT void QueueSendPacketsOperation_CalledFromWithinIoServiceThread(
        std::shared_ptr<std::vector<UdpSendPacketInfo> >&& udpSendPacketInfoVecSharedPtr, const std::size_t numPacketsToSend);
    
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
    
    // Internal implementation class
    class Impl; //public for ostream operators
    // Pointer to the internal implementation
    std::unique_ptr<Impl> m_pimpl;

};



#endif //_UDP_BATCH_SENDER_H

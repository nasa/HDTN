/**
 * @file EncapAsyncDuplexLocalStream.h
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
 * This EncapAsyncDuplexLocalStream class encapsulates a cross-platform local stream.
 * On Windows, this is acomplished using a full-duplex named pipe.
 * On Linux, this is accomplished using a local "AF_UNIX" duplex socket.
 */

#ifndef _ENCAP_ASYNC_DUPLEX_LOCAL_STREAM_H
#define _ENCAP_ASYNC_DUPLEX_LOCAL_STREAM_H 1

#include <string>
#include <boost/thread.hpp>
#include <boost/asio.hpp>
#include <boost/filesystem.hpp>
#include <boost/make_unique.hpp>
#include <boost/function.hpp>
#include <atomic>
#include <memory>
#include "PaddedVectorUint8.h"
#include "CcsdsEncap.h"
#ifdef _WIN32
# define STREAM_USE_WINDOWS_NAMED_PIPE 1
#endif // _WIN32

#ifdef STREAM_USE_WINDOWS_NAMED_PIPE
# include <windows.h>
#endif // _WIN32



class EncapAsyncDuplexLocalStream {
public:
    typedef boost::function<void(padded_vector_uint8_t& receivedFullEncapPacket,
        uint32_t decodedEncapPayloadSize, uint8_t decodedEncapHeaderSize)> OnFullEncapPacketReceivedCallback_t;
    typedef boost::function<void(bool isOnConnectionEvent)> OnLocalStreamConnectionStatusChangedCallback_t;

    EncapAsyncDuplexLocalStream(boost::asio::io_service& ioService,
        const ENCAP_PACKET_TYPE encapPacketType,
        const uint64_t maxEncapRxPacketSizeBytes,
        const OnFullEncapPacketReceivedCallback_t& onFullEncapPacketReceivedCallback,
        const OnLocalStreamConnectionStatusChangedCallback_t& onLocalStreamConnectionStatusChangedCallback,
        const bool rxCallbackDontDiscardEncapHeader = true) :
        M_MAX_ENCAP_RX_PACKET_SIZE_BYTES(maxEncapRxPacketSizeBytes),
        m_ioServiceRef(ioService),
        m_workPtr(boost::make_unique<boost::asio::io_service::work>(ioService)),
        m_onFullEncapPacketReceivedCallback(onFullEncapPacketReceivedCallback),
        m_onLocalStreamConnectionStatusChangedCallback(onLocalStreamConnectionStatusChangedCallback),
#ifndef STREAM_USE_WINDOWS_NAMED_PIPE
        m_reconnectAfterOnConnectErrorTimer(ioService),
#endif
        m_streamHandle(ioService),
        m_numReconnectAttempts(0),
        M_ENCAP_PACKET_TYPE(encapPacketType),
        M_RX_CALLBACK_DONT_DISCARD_ENCAP_HEADER(rxCallbackDontDiscardEncapHeader),
        m_isStreamCreator(false),
        m_running(false),
        m_readyToSend(false),
        m_shutdownComplete(true)
    {
        m_receivedFullEncapPacket_swappable.resize(maxEncapRxPacketSizeBytes);
    }
    
    
    ~EncapAsyncDuplexLocalStream() {
        Stop();
    }
    
    /** Perform graceful shutdown.
     *
     * If no previous successful call to Init(), returns immediately.
     * Else, tries to perform graceful shutdown on the socket, then releases all underlying I/O resources.
     * @post The object is ready to be reused after the next successful call to Init().
     */
    void Stop() {
        m_running = false;
        if (!m_shutdownComplete.load(std::memory_order_acquire)) {
            boost::asio::post(m_ioServiceRef, boost::bind(&EncapAsyncDuplexLocalStream::HandleShutdown, this));
            while (!m_shutdownComplete.load(std::memory_order_acquire)) {
                try {
                    boost::this_thread::sleep(boost::posix_time::milliseconds(250));
                }
                catch (const boost::thread_resource_error&) {}
                catch (const boost::thread_interrupted&) {}
                catch (const boost::condition_error&) {}
                catch (const boost::lock_error&) {}
            }
        }
        
        m_workPtr.reset();
#ifdef STREAM_USE_WINDOWS_NAMED_PIPE
        if (m_threadWaitForConnection) {
            m_threadWaitForConnection->join();
            m_threadWaitForConnection.reset();
        }
#else
        m_reconnectAfterOnConnectErrorTimer.cancel();
#endif
    }
    
    /** Initialize the underlying I/O and connect to given host at given port.
     *
     * @param remoteHostname The remote host to connect to.
     * @param remotePort The remote port to connect to.
     * @return True if the connection could be established, or False if connection failed or the object has already been initialized.
     */
    bool Init(const std::string& socketOrPipePath, bool isStreamCreator) {
        if (m_running) {
            return false;
        }
        m_socketOrPipePath = socketOrPipePath;
        m_isStreamCreator = isStreamCreator;

#ifdef STREAM_USE_WINDOWS_NAMED_PIPE
        if (isStreamCreator) { //binding
            const DWORD bufferSize = 4096 * 2;
            //LPTSTR lpszPipename = TEXT("\\\\.\\pipe\\mynamedpipe");
            //https://learn.microsoft.com/en-us/windows/win32/ipc/named-pipe-server-using-overlapped-i-o
            //https://learn.microsoft.com/en-us/windows/win32/ipc/named-pipe-server-using-completion-routines
            //https://learn.microsoft.com/en-us/windows/win32/api/winbase/nf-winbase-createnamedpipea
            HANDLE hPipeInst = CreateNamedPipeA(
                m_socketOrPipePath.c_str(),            // pipe name 
                PIPE_ACCESS_DUPLEX |     // read/write access 
                FILE_FLAG_OVERLAPPED,    // overlapped mode 
                PIPE_TYPE_MESSAGE |      // message-type pipe 
                PIPE_READMODE_MESSAGE |  // message-read mode 
                PIPE_WAIT,               // blocking mode 
                1,               // number of instances 
                bufferSize,   // output buffer size 
                bufferSize,   // input buffer size 
                5000,            // client time-out 
                NULL);                   // default security attributes 

            if (hPipeInst == INVALID_HANDLE_VALUE) {
                printf("CreateNamedPipe failed with %d.\n", GetLastError());
                return false;
            }
            m_running = true;
            m_threadWaitForConnection = boost::make_unique<boost::thread>(
                boost::bind(&EncapAsyncDuplexLocalStream::WaitForConnectionThreadFunc, this, hPipeInst));
        }
        else { //connecting
            m_running = true;
            m_threadWaitForConnection = boost::make_unique<boost::thread>(
                boost::bind(&EncapAsyncDuplexLocalStream::TryToOpenExistingPipeThreadFunc, this));
        }
#else //unix sockets
        const boost::asio::local::stream_protocol::endpoint streamProtocolEndpoint(m_socketOrPipePath);
        if (isStreamCreator) { //binding
            {
                boost::system::error_code ec;
                if (boost::filesystem::remove(m_socketOrPipePath, ec)) {
                    std::cout << "stream creator removed existing " << m_socketOrPipePath << "\n";
                }
            }
            m_streamAcceptorPtr = boost::make_unique<boost::asio::local::stream_protocol::acceptor>(
                m_ioServiceRef, streamProtocolEndpoint);
            m_streamAcceptorPtr->async_accept(m_streamHandle,
                boost::bind(&EncapAsyncDuplexLocalStream::OnSocketAccept, this, boost::asio::placeholders::error));
            m_running = true;
        }
        else { //connecting
            m_streamHandle.async_connect(streamProtocolEndpoint,
                boost::bind(&EncapAsyncDuplexLocalStream::OnSocketConnect, this, boost::asio::placeholders::error));
            m_running = true;
        }
#endif //STREAM_USE_WINDOWS_NAMED_PIPE
        return true;
    }
private:
    
#ifdef STREAM_USE_WINDOWS_NAMED_PIPE
    void WaitForConnectionThreadFunc(HANDLE hPipeInst) {
        HANDLE hEventWaitForConnection = CreateEvent(
            NULL,    // default security attribute 
            TRUE,    // manual-reset event 
            FALSE,    // initial state = unsignaled 
            NULL);   // unnamed event object 

        if (hEventWaitForConnection == NULL) {
            printf("CreateEvent WaitForConnection failed with %d.\n", GetLastError());
            return;
        }
        OVERLAPPED oOverlap;
        oOverlap.hEvent = hEventWaitForConnection;
        oOverlap.Offset = 0;
        oOverlap.OffsetHigh = 0;
        BOOL fConnected;// , fPendingIO = FALSE;

        // Start an overlapped connection for this pipe instance. 
        //https://learn.microsoft.com/en-us/windows/win32/api/namedpipeapi/nf-namedpipeapi-connectnamedpipe
        fConnected = ConnectNamedPipe(hPipeInst, &oOverlap);

        // Overlapped ConnectNamedPipe should return zero. 
        if (fConnected) {
            printf("ConnectNamedPipe failed with %d.\n", GetLastError());
            return;
        }

        while (m_running) {
            // Wait for a client to connect, or for a read or write 
            // operation to be completed, which causes a completion 
            // routine to be queued for execution. 
            std::cout << "wait\n";
            DWORD dwWait = WaitForSingleObject(
                hEventWaitForConnection,  // event object to wait for 
                250       // 250ms wait
            );

            if (dwWait == WAIT_OBJECT_0) {
                std::cout << "connected by server\n";
                break;
            }
        }
        CloseHandle(hEventWaitForConnection);

        //https://github.com/boostorg/process/issues/83
        boost::system::error_code ecAssign;
        m_streamHandle.assign(hPipeInst, ecAssign);
        if (ecAssign) {
            std::cout << "error: " << ecAssign.what() << "\n";
            return;
        }
        boost::asio::post(m_ioServiceRef, boost::bind(&EncapAsyncDuplexLocalStream::OnConnectionThreadCompleted_NotThreadSafe, this));
    }
    void TryToOpenExistingPipeThreadFunc() {
        while (m_running) {
            //std::cout << "S: Creating pipe " << pipeName << "\n";
            //https://learn.microsoft.com/en-us/windows/win32/api/fileapi/nf-fileapi-createfilea
            HANDLE hPipeInst = CreateFileA(m_socketOrPipePath.c_str(),
                GENERIC_READ | GENERIC_WRITE,
                0, nullptr,
                OPEN_EXISTING,
                FILE_FLAG_OVERLAPPED,
                nullptr);

            if (hPipeInst == INVALID_HANDLE_VALUE) {
                printf("Open pipe failed with %d.. retrying in 2 seconds\n", GetLastError());
                boost::this_thread::sleep(boost::posix_time::seconds(2));
            }
            else {
                printf("Opened pipe\n");
                boost::system::error_code ecAssign;
                m_streamHandle.assign(hPipeInst, ecAssign);
                if (ecAssign) {
                    std::cout << "error: " << ecAssign.what() << "\n";
                    return;
                }
                boost::asio::post(m_ioServiceRef, boost::bind(&EncapAsyncDuplexLocalStream::OnConnectionThreadCompleted_NotThreadSafe, this));
                return;
            }
        }
    }
    void OnConnectionThreadCompleted_NotThreadSafe() {
        if (m_threadWaitForConnection) {
            m_threadWaitForConnection->join();
            m_threadWaitForConnection.reset();
            std::cout << "connection thread erased\n";
        }
        StartReadFirstEncapHeaderByte_NotThreadSafe();
        m_shutdownComplete = false;
        m_readyToSend = true;
        if (m_onLocalStreamConnectionStatusChangedCallback) {
            m_onLocalStreamConnectionStatusChangedCallback(m_readyToSend);
        }
    }
#else //unix sockets
    void OnSocketAccept(const boost::system::error_code& error) {
        if (error) {
            std::cout << "Error OnSocketAccept: " << error.message() << "\n";
        }
        else {
            std::cout << "remote client connected to this local unix socket " << m_socketOrPipePath << "\n";
            StartReadFirstEncapHeaderByte_NotThreadSafe();
            m_shutdownComplete = false;
            m_readyToSend = true;
            if (m_onLocalStreamConnectionStatusChangedCallback) {
                m_onLocalStreamConnectionStatusChangedCallback(m_readyToSend);
            }
        }
    }
    void OnSocketConnect(const boost::system::error_code& error) {
        if (error) {
            if (error != boost::asio::error::operation_aborted) {
                if (m_numReconnectAttempts <= 1) {
                    std::cout << "OnConnect: " << error.message()
                        << "\n Will continue to try to reconnect every 2 seconds\n";
                }
                m_reconnectAfterOnConnectErrorTimer.expires_from_now(boost::posix_time::seconds(2));
                m_reconnectAfterOnConnectErrorTimer.async_wait(
                    boost::bind(&EncapAsyncDuplexLocalStream::OnReconnectAfterOnConnectError_TimerExpired, this, boost::asio::placeholders::error));
            }
        }
        else {
            std::cout << "connected to local unix socket " << m_socketOrPipePath << "\n";
            StartReadFirstEncapHeaderByte_NotThreadSafe();
            m_shutdownComplete = false;
            m_readyToSend = true;
            if (m_onLocalStreamConnectionStatusChangedCallback) {
                m_onLocalStreamConnectionStatusChangedCallback(m_readyToSend);
            }
        }
    }
    void OnReconnectAfterOnConnectError_TimerExpired(const boost::system::error_code& e) {
        if (e != boost::asio::error::operation_aborted) {
            // Timer was not cancelled, take necessary action.

            if (m_numReconnectAttempts++ == 0) {
                std::cout << "Trying to reconnect...\n";
            }
            const boost::asio::local::stream_protocol::endpoint streamProtocolEndpoint(m_socketOrPipePath);
            m_streamHandle.async_connect(streamProtocolEndpoint,
                boost::bind(&EncapAsyncDuplexLocalStream::OnSocketConnect, this, boost::asio::placeholders::error));
        }
    }
#endif //STREAM_USE_WINDOWS_NAMED_PIPE
public:
    void StartReadFirstEncapHeaderByte_NotThreadSafe() {
        m_receivedFullEncapPacket_swappable.resize(8);
        boost::asio::async_read(m_streamHandle,
            boost::asio::buffer(&m_receivedFullEncapPacket_swappable[0], 1),
            boost::bind(&EncapAsyncDuplexLocalStream::HandleFirstEncapByteReadCompleted, this,
                boost::asio::placeholders::error,
                boost::asio::placeholders::bytes_transferred));
    }
    void StartReadFirstEncapHeaderByte_ThreadSafe() {
        boost::asio::post(m_ioServiceRef, boost::bind(&EncapAsyncDuplexLocalStream::StartReadFirstEncapHeaderByte_NotThreadSafe, this));
    }
private:
    void HandleFirstEncapByteReadCompleted(const boost::system::error_code& error, std::size_t bytes_transferred) {
        if (error) {
#ifdef STREAM_USE_WINDOWS_NAMED_PIPE
            //https://learn.microsoft.com/en-us/windows/win32/debug/system-error-codes--0-499-
            //ERROR_BROKEN_PIPE
            if (error.value() != ERROR_MORE_DATA)
#endif
            {
                std::cout << "HandleFirstEncapByteReadCompleted: " << error.message() << "\n";
                return;
            }
        }
        if (bytes_transferred != 1) {
            std::cout << "HandleFirstEncapByteReadCompleted: bytes_transferred != 1\n";
            return;
        }
        const uint8_t decodedEncapHeaderSize = DecodeCcsdsEncapHeaderSizeFromFirstByte(
            M_ENCAP_PACKET_TYPE, m_receivedFullEncapPacket_swappable[0]);
        if (decodedEncapHeaderSize == 0) {
            std::cout << "HandleFirstEncapByteReadCompleted: invalid LTP encap header received\n";
        }
        else if (decodedEncapHeaderSize == 1) { //idle packet (no data)
            StartReadFirstEncapHeaderByte_NotThreadSafe();
        }
        else {
            boost::asio::async_read(m_streamHandle,
                boost::asio::buffer(&m_receivedFullEncapPacket_swappable[1], decodedEncapHeaderSize - 1),
                boost::bind(&EncapAsyncDuplexLocalStream::HandleRemainingEncapHeaderReadCompleted, this,
                    boost::asio::placeholders::error,
                    boost::asio::placeholders::bytes_transferred,
                    decodedEncapHeaderSize));
        }
    }

    void HandleRemainingEncapHeaderReadCompleted(const boost::system::error_code& error, std::size_t bytes_transferred, uint8_t decodedEncapHeaderSize) {
        if (error) {
#ifdef STREAM_USE_WINDOWS_NAMED_PIPE
            if (error.value() != ERROR_MORE_DATA)
#endif
            {
                std::cout << "HandleRemainingEncapHeaderReadCompleted: " << error.message() << "\n";
                return;
            }
        }
        if (bytes_transferred != (decodedEncapHeaderSize - 1)) {
            std::cout << "HandleRemainingEncapHeaderReadCompleted: bytes_transferred != (decodedEncapHeaderSize - 1)\n";
            return;
        }
        uint8_t userDefinedField;
        uint32_t decodedEncapPayloadSize;
        if (!DecodeCcsdsEncapPayloadSizeFromSecondToRemainingBytes(decodedEncapHeaderSize,
            &m_receivedFullEncapPacket_swappable[1],
            userDefinedField,
            decodedEncapPayloadSize))
        {
            std::cout << "HandleRemainingEncapHeaderReadCompleted: invalid LTP encap header received\n";
        }
        else {
            const uint8_t encapHeaderSize = decodedEncapHeaderSize * M_RX_CALLBACK_DONT_DISCARD_ENCAP_HEADER;
            m_receivedFullEncapPacket_swappable.resize(decodedEncapPayloadSize + encapHeaderSize);
            boost::asio::async_read(m_streamHandle,
                boost::asio::buffer(&m_receivedFullEncapPacket_swappable[encapHeaderSize], decodedEncapPayloadSize),
                boost::bind(&EncapAsyncDuplexLocalStream::HandleEncapPayloadReadCompleted, this,
                    boost::asio::placeholders::error,
                    boost::asio::placeholders::bytes_transferred,
                    decodedEncapPayloadSize, encapHeaderSize));
        }
    }

    void HandleEncapPayloadReadCompleted(const boost::system::error_code& error, std::size_t bytes_transferred,
        uint32_t decodedEncapPayloadSize, uint8_t decodedEncapHeaderSize)
    {
        if (error) {
#ifdef STREAM_USE_WINDOWS_NAMED_PIPE
            if (error.value() != ERROR_MORE_DATA)
#endif
            {
                std::cout << "HandleEncapPayloadReadCompleted: " << error.message() << "\n";
                return;
            }
        }
        if (bytes_transferred != decodedEncapPayloadSize) {
            std::cout << "HandleEncapPayloadReadCompleted: bytes_transferred != decodedEncapPayloadSize\n";
            return;
        }
        m_onFullEncapPacketReceivedCallback(m_receivedFullEncapPacket_swappable,
            decodedEncapPayloadSize, decodedEncapHeaderSize);
        //this StartReadFirstEncapHeaderByte_NotThreadSafe() is optionally called within m_onFullEncapPacketReceivedCallback;
    }

    void HandleShutdown() {
        m_readyToSend = false;
        //final code to shut down tcp sockets
        if (m_streamHandle.is_open()) {
            if (m_onLocalStreamConnectionStatusChangedCallback) {
                m_onLocalStreamConnectionStatusChangedCallback(m_readyToSend);
            }
#ifndef STREAM_USE_WINDOWS_NAMED_PIPE
            try {
                std::cout << "shutting down Local Stream..\n";
                m_streamHandle.shutdown(boost::asio::socket_base::shutdown_type::shutdown_both);
            }
            catch (const boost::system::system_error& e) {
                std::cout << "EncapAsyncDuplexLocalStream::HandleShutdown: " << e.what() << "\n";
            }
#endif
            try {
                std::cout << "closing Local Stream..\n";
                m_streamHandle.close();
            }
            catch (const boost::system::system_error& e) {
                std::cout << "EncapAsyncDuplexLocalStream::HandleSocketShutdown: " << e.what() << "\n";
            }
        }
#ifndef STREAM_USE_WINDOWS_NAMED_PIPE
        if (m_isStreamCreator) { //binding
            boost::system::error_code ec;
            if (boost::filesystem::remove(m_socketOrPipePath, ec)) {
                std::cout << "stream creator removed local socket " << m_socketOrPipePath << " after shutdown\n";
            }
        }
#endif
        m_shutdownComplete = true;
    }

    

    const uint64_t M_MAX_ENCAP_RX_PACKET_SIZE_BYTES;
    /// I/O execution context
    boost::asio::io_service& m_ioServiceRef;
    /// Explicit work controller for m_ioService, allows for graceful shutdown of running tasks
    std::unique_ptr<boost::asio::io_service::work> m_workPtr;
    const OnFullEncapPacketReceivedCallback_t m_onFullEncapPacketReceivedCallback;
    const OnLocalStreamConnectionStatusChangedCallback_t m_onLocalStreamConnectionStatusChangedCallback;
    std::string m_socketOrPipePath;
    padded_vector_uint8_t m_receivedFullEncapPacket_swappable;
    /// UDP socket to connect to destination endpoint
#ifdef STREAM_USE_WINDOWS_NAMED_PIPE
    typedef boost::asio::windows::basic_stream_handle<boost::asio::any_io_executor> stream_handle_t;
    std::unique_ptr<boost::thread> m_threadWaitForConnection;
#else
    typedef boost::asio::local::stream_protocol::socket stream_handle_t;
    std::unique_ptr<boost::asio::local::stream_protocol::acceptor> m_streamAcceptorPtr;
    boost::asio::deadline_timer m_reconnectAfterOnConnectErrorTimer;
#endif
    stream_handle_t m_streamHandle;
    uint64_t m_numReconnectAttempts;
    const ENCAP_PACKET_TYPE M_ENCAP_PACKET_TYPE;
    const bool M_RX_CALLBACK_DONT_DISCARD_ENCAP_HEADER;
    std::atomic<bool> m_isStreamCreator;
    std::atomic<bool> m_running;
    std::atomic<bool> m_readyToSend;
    std::atomic<bool> m_shutdownComplete;

public:
    stream_handle_t& GetStreamHandleRef() noexcept {
        return m_streamHandle;
    }
    bool ReadyToSend() const noexcept {
        return m_readyToSend.load(std::memory_order_acquire);
    }
};

#endif //_ENCAP_ASYNC_DUPLEX_LOCAL_STREAM_H

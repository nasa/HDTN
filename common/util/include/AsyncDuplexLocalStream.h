/**
 * @file AsyncDuplexLocalStream.h
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
 * This AsyncDuplexLocalStream class encapsulates a cross-platform local stream.
 * On Windows, this is acomplished using a full-duplex named pipe.
 * On Linux, this is accomplished using a local "AF_UNIX" duplex socket.
 */

#ifndef _ASYNC_DUPLEX_LOCAL_STREAM_H
#define _ASYNC_DUPLEX_LOCAL_STREAM_H 1

#include <string>
#include <boost/thread.hpp>
#include <boost/asio.hpp>
#include <boost/filesystem.hpp>
#include <boost/make_unique.hpp>
#include <boost/function.hpp>
#include <atomic>
#include <memory>
#include "PaddedVectorUint8.h"
#include "LtpEncap.h"
#ifdef _WIN32
# include <windows.h>
#endif // _WIN32



class AsyncDuplexLocalStream {
public:
    typedef boost::function<void(padded_vector_uint8_t& receivedFullEncapPacket,
        uint32_t decodedEncapPayloadSize, uint8_t decodedEncapHeaderSize)> OnFullEncapPacketReceivedCallback_t;

    AsyncDuplexLocalStream(boost::asio::io_service& ioService,
        const uint64_t maxEncapRxPacketSizeBytes,
        const OnFullEncapPacketReceivedCallback_t& onFullEncapPacketReceivedCallback) :
        M_MAX_ENCAP_RX_PACKET_SIZE_BYTES(maxEncapRxPacketSizeBytes),
        m_ioServiceRef(ioService),
        m_workPtr(boost::make_unique<boost::asio::io_service::work>(ioService)),
        m_onFullEncapPacketReceivedCallback(onFullEncapPacketReceivedCallback),
        m_streamHandle(ioService),
        m_running(false),
        m_readyToSend(false)
    {
        m_receivedFullEncapPacket_swappable.resize(maxEncapRxPacketSizeBytes);
    }
    
    
    ~AsyncDuplexLocalStream() {
        Stop();
    }
    
    /** Perform graceful shutdown.
     *
     * If no previous successful call to UdpBatchSender::Init(), returns immediately.
     * Else, tries to perform graceful shutdown on the socket, then releases all underlying I/O resources.
     * @post The object is ready to be reused after the next successful call to UdpBatchSender::Init().
     */
    void Stop() {
        m_running = false;
        if (m_workPtr) {
            m_workPtr.reset();
        }
        if (m_threadWaitForConnection) {
            m_threadWaitForConnection->join();
            m_threadWaitForConnection.reset();
        }
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
        if (isStreamCreator) { //binding
            //LPTSTR lpszPipename = TEXT("\\\\.\\pipe\\mynamedpipe");
            HANDLE hPipeInst = CreateNamedPipeA(
                m_socketOrPipePath.c_str(),            // pipe name 
                PIPE_ACCESS_DUPLEX |     // read/write access 
                FILE_FLAG_OVERLAPPED,    // overlapped mode 
                PIPE_TYPE_MESSAGE |      // message-type pipe 
                PIPE_READMODE_MESSAGE |  // message-read mode 
                PIPE_WAIT,               // blocking mode 
                1,               // number of instances 
                4096 * sizeof(TCHAR),   // output buffer size 
                4096 * sizeof(TCHAR),   // input buffer size 
                5000,            // client time-out 
                NULL);                   // default security attributes 

            if (hPipeInst == INVALID_HANDLE_VALUE) {
                printf("CreateNamedPipe failed with %d.\n", GetLastError());
                return false;
            }
            

            

            m_running = true;
            m_threadWaitForConnection = boost::make_unique<boost::thread>(
                boost::bind(&AsyncDuplexLocalStream::WaitForConnectionThreadFunc, this, hPipeInst));
            
        }
        else { //connecting
            m_running = true;
            m_threadWaitForConnection = boost::make_unique<boost::thread>(
                boost::bind(&AsyncDuplexLocalStream::TryToOpenExistingPipeThreadFunc, this));
        }
        return true;
    }
private:
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
        boost::system::error_code ecAssign;
        m_streamHandle.assign(hPipeInst, ecAssign);
        if (ecAssign) {
            std::cout << "error: " << ecAssign.what() << "\n";
            return;
        }
        boost::asio::post(m_ioServiceRef, boost::bind(&AsyncDuplexLocalStream::OnConnectionThreadCompleted_NotThreadSafe, this));
    }
    void TryToOpenExistingPipeThreadFunc() {
        while (m_running) {
            //std::cout << "S: Creating pipe " << pipeName << "\n";
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
                boost::asio::post(m_ioServiceRef, boost::bind(&AsyncDuplexLocalStream::OnConnectionThreadCompleted_NotThreadSafe, this));
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
        m_readyToSend = true;
    }
public:
    void StartReadFirstEncapHeaderByte_NotThreadSafe() {
        m_receivedFullEncapPacket_swappable.resize(8);
        boost::asio::async_read(m_streamHandle,
            boost::asio::buffer(&m_receivedFullEncapPacket_swappable[0], 1),
            boost::bind(&AsyncDuplexLocalStream::HandleFirstEncapByteReadCompleted, this,
                boost::asio::placeholders::error,
                boost::asio::placeholders::bytes_transferred));
    }
    void StartReadFirstEncapHeaderByte_ThreadSafe() {
        boost::asio::post(m_ioServiceRef, boost::bind(&AsyncDuplexLocalStream::StartReadFirstEncapHeaderByte_NotThreadSafe, this));
    }
private:
    void HandleFirstEncapByteReadCompleted(const boost::system::error_code& error, std::size_t bytes_transferred) {
        if (error) {
            if (error.value() != ERROR_MORE_DATA) {
                std::cout << "HandleFirstEncapByteReadCompleted: " << error.what() << "\n";
                return;
            }
        }
        if (bytes_transferred != 1) {
            std::cout << "HandleFirstEncapByteReadCompleted: bytes_transferred != 1\n";
            return;
        }
        const uint8_t decodedEncapHeaderSize = DecodeCcsdsLtpEncapHeaderSizeFromFirstByte(m_receivedFullEncapPacket_swappable[0]);
        if (decodedEncapHeaderSize == 0) {
            std::cout << "HandleFirstEncapByteReadCompleted: invalid LTP encap header received\n";
        }
        else if (decodedEncapHeaderSize == 1) { //keep alive (no data)
            StartReadFirstEncapHeaderByte_NotThreadSafe();
        }
        else {
            boost::asio::async_read(m_streamHandle,
                boost::asio::buffer(&m_receivedFullEncapPacket_swappable[1], decodedEncapHeaderSize - 1),
                boost::bind(&AsyncDuplexLocalStream::HandleRemainingEncapHeaderReadCompleted, this,
                    boost::asio::placeholders::error,
                    boost::asio::placeholders::bytes_transferred,
                    decodedEncapHeaderSize));
        }
    }

    void HandleRemainingEncapHeaderReadCompleted(const boost::system::error_code& error, std::size_t bytes_transferred, uint8_t decodedEncapHeaderSize) {
        if (error) {
            if (error.value() != ERROR_MORE_DATA) {
                std::cout << "HandleRemainingEncapHeaderReadCompleted: " << error.what() << "\n";
                return;
            }
        }
        if (bytes_transferred != (decodedEncapHeaderSize - 1)) {
            std::cout << "HandleRemainingEncapHeaderReadCompleted: bytes_transferred != (decodedEncapHeaderSize - 1)\n";
            return;
        }
        uint8_t userDefinedField;
        uint32_t decodedEncapPayloadSize;
        if (!DecodeCcsdsLtpEncapPayloadSizeFromSecondToRemainingBytes(decodedEncapHeaderSize,
            &m_receivedFullEncapPacket_swappable[1],
            userDefinedField,
            decodedEncapPayloadSize))
        {
            std::cout << "HandleRemainingEncapHeaderReadCompleted: invalid LTP encap header received\n";
        }
        else {
            m_receivedFullEncapPacket_swappable.resize(decodedEncapPayloadSize + decodedEncapHeaderSize);
            boost::asio::async_read(m_streamHandle,
                boost::asio::buffer(&m_receivedFullEncapPacket_swappable[decodedEncapHeaderSize], decodedEncapPayloadSize),
                boost::bind(&AsyncDuplexLocalStream::HandleEncapPayloadReadCompleted, this,
                    boost::asio::placeholders::error,
                    boost::asio::placeholders::bytes_transferred,
                    decodedEncapPayloadSize, decodedEncapHeaderSize));
        }
    }

    void HandleEncapPayloadReadCompleted(const boost::system::error_code& error, std::size_t bytes_transferred,
        uint32_t decodedEncapPayloadSize, uint8_t decodedEncapHeaderSize)
    {
        if (error) {
            if (error.value() != ERROR_MORE_DATA) {
                std::cout << "HandleEncapPayloadReadCompleted: " << error.what() << "\n";
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

    

    const uint64_t M_MAX_ENCAP_RX_PACKET_SIZE_BYTES;
    /// I/O execution context
    boost::asio::io_service& m_ioServiceRef;
    /// Explicit work controller for m_ioService, allows for graceful shutdown of running tasks
    std::unique_ptr<boost::asio::io_service::work> m_workPtr;
    const OnFullEncapPacketReceivedCallback_t m_onFullEncapPacketReceivedCallback;
    std::string m_socketOrPipePath;
    padded_vector_uint8_t m_receivedFullEncapPacket_swappable;
    /// UDP socket to connect to destination endpoint
#ifdef _WIN32
    typedef boost::asio::windows::basic_stream_handle<boost::asio::any_io_executor> stream_handle_t;
    stream_handle_t m_streamHandle;
    std::unique_ptr<boost::thread> m_threadWaitForConnection;
#endif
    std::atomic<bool> m_running;
    std::atomic<bool> m_readyToSend;

public:
    stream_handle_t& GetStreamHandleRef() noexcept {
        return m_streamHandle;
    }
    bool ReadyToSend() const noexcept {
        return m_readyToSend.load(std::memory_order_acquire);
    }
};





#endif //_ASYNC_DUPLEX_LOCAL_STREAM_H

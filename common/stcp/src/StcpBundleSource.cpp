#include <string>
#include <iostream>
#include "StcpBundleSource.h"
#include <boost/lexical_cast.hpp>
#include <boost/make_shared.hpp>
#include <boost/endian/conversion.hpp>


StcpBundleSource::StcpBundleSource(const uint16_t desiredKeeAliveIntervlSeconds) :
m_work(m_ioService), //prevent stopping of ioservice until destructor
m_resolver(m_ioService),
m_needToSendKeepAliveMessageTimer(m_ioService),
M_KEEP_ALIVE_INTERVAL_SECONDS(desiredKeeAliveIntervlSeconds),
m_readyToForward(false),
m_dataServedAsKeepAlive(true),
m_totalDataSegmentsSent(0),
m_totalBundleBytesSent(0)
{
    m_ioServiceThreadPtr = boost::make_shared<boost::thread>(boost::bind(&boost::asio::io_service::run, &m_ioService));
}

StcpBundleSource::~StcpBundleSource() {
    
    DoStcpShutdown();
    
    //This function does not block, but instead simply signals the io_service to stop
    //All invocations of its run() or run_one() member functions should return as soon as possible.
    //Subsequent calls to run(), run_one(), poll() or poll_one() will return immediately until reset() is called.
    m_ioService.stop(); //ioservice requires stopping before join because of the m_work object

    if(m_ioServiceThreadPtr) {
        m_ioServiceThreadPtr->join();
        m_ioServiceThreadPtr = boost::shared_ptr<boost::thread>();
    }

    //print stats
    std::cout << "m_totalDataSegmentsSent " << m_totalDataSegmentsSent << std::endl;
    std::cout << "m_totalBundleBytesSent " << m_totalBundleBytesSent << std::endl;
}

//An STCP protocol data unit (SPDU) is simply a serialized bundle
//preceded by an integer indicating the length of that serialized
//bundle.
void StcpBundleSource::GenerateDataUnit(std::vector<uint8_t> & dataUnit, const uint8_t * contents, uint32_t sizeContents) {
    uint32_t sizeContentsBigEndian = sizeContents;
    boost::endian::native_to_big_inplace(sizeContentsBigEndian);
    
   
    dataUnit.resize(sizeof(uint32_t) + sizeContents);

    memcpy(&dataUnit[0], &sizeContentsBigEndian, sizeof(uint32_t));
    if (sizeContents) {
        memcpy(&dataUnit[sizeof(uint32_t)], contents, sizeContents);
    }
}

bool StcpBundleSource::Forward(const uint8_t* bundleData, const std::size_t size) {

    if(!m_readyToForward) {
        std::cerr << "link not ready to forward yet" << std::endl;
        return false;
    }
    ++m_totalDataSegmentsSent;
    m_totalBundleBytesSent += size;
    
    boost::shared_ptr<std::vector<uint8_t> > stcpDataUnitPtr = boost::make_shared<std::vector<uint8_t> >();
    StcpBundleSource::GenerateDataUnit(*stcpDataUnitPtr, bundleData, static_cast<uint32_t>(size));

    m_dataServedAsKeepAlive = true;
    boost::asio::async_write(*m_tcpSocketPtr, boost::asio::buffer(*stcpDataUnitPtr),
                                     boost::bind(&StcpBundleSource::HandleTcpSend, this, stcpDataUnitPtr,
                                                 boost::asio::placeholders::error,
                                                 boost::asio::placeholders::bytes_transferred));
    return true;
}



void StcpBundleSource::Connect(const std::string & hostname, const std::string & port) {

    boost::asio::ip::tcp::resolver::query query(hostname, port);
    m_resolver.async_resolve(query, boost::bind(&StcpBundleSource::OnResolve,
                                                this, boost::asio::placeholders::error,
                                                boost::asio::placeholders::results));
}

void StcpBundleSource::OnResolve(const boost::system::error_code & ec, boost::asio::ip::tcp::resolver::results_type results) { // Resolved endpoints as a range.
    if(ec) {
        std::cerr << "Error resolving: " << ec.message() << std::endl;
    }
    else {
        std::cout << "resolved host to " << results->endpoint().address() << ":" << results->endpoint().port() << ".  Connecting..." << std::endl;
        m_tcpSocketPtr = boost::make_shared<boost::asio::ip::tcp::socket>(m_ioService);
        boost::asio::async_connect(
            *m_tcpSocketPtr,
            results,
            boost::bind(
                &StcpBundleSource::OnConnect,
                this,
                boost::asio::placeholders::error));
    }
}

void StcpBundleSource::OnConnect(const boost::system::error_code & ec) {

    if (ec) {
        if (ec != boost::asio::error::operation_aborted) {
            std::cerr << "Error in OnConnect: " << ec.value() << " " << ec.message() << "\n";
        }
        return;
    }

    std::cout << "Stcp connection complete" << std::endl;
    m_readyToForward = true;


    m_needToSendKeepAliveMessageTimer.expires_from_now(boost::posix_time::seconds(M_KEEP_ALIVE_INTERVAL_SECONDS));
    m_needToSendKeepAliveMessageTimer.async_wait(boost::bind(&StcpBundleSource::OnNeedToSendKeepAliveMessage_TimerExpired, this, boost::asio::placeholders::error));

    if(m_tcpSocketPtr) {
        StartTcpReceive();
    }
}



void StcpBundleSource::HandleTcpSend(boost::shared_ptr<std::vector<boost::uint8_t> > dataSentPtr, const boost::system::error_code& error, std::size_t bytes_transferred) {
    if (error) {
        std::cerr << "error in StcpBundleSource::HandleTcpSend: " << error.message() << std::endl;
        DoStcpShutdown();
    }
}

void StcpBundleSource::HandleTcpSendKeepAlive(const boost::system::error_code& error, std::size_t bytes_transferred) {
    if (error) {
        std::cerr << "error in StcpBundleSource::HandleTcpSendKeepAlive: " << error.message() << std::endl;
        DoStcpShutdown();
    }
    else {
        std::cout << "notice: keepalive packet sent" << std::endl;
    }
}

void StcpBundleSource::StartTcpReceive() {
    m_tcpSocketPtr->async_read_some(
        boost::asio::buffer(m_tcpReadSomeBuffer, 10),
        boost::bind(&StcpBundleSource::HandleTcpReceiveSome, this,
                    boost::asio::placeholders::error,
                    boost::asio::placeholders::bytes_transferred));
}
void StcpBundleSource::HandleTcpReceiveSome(const boost::system::error_code & error, std::size_t bytesTransferred) {
    if (!error) {
        std::cerr << "Error in StcpBundleSource::HandleTcpReceiveSome: received " << bytesTransferred << " but should never receive any data" << std::endl;

        //shutdown
    }
    else if (error == boost::asio::error::eof) {
        std::cout << "Tcp connection closed cleanly by peer" << std::endl;
        DoStcpShutdown();
    }
    else if (error != boost::asio::error::operation_aborted) {
        std::cerr << "Error in StcpBundleSource::HandleTcpReceiveSome: " << error.message() << std::endl;
    }
}



void StcpBundleSource::OnNeedToSendKeepAliveMessage_TimerExpired(const boost::system::error_code& e) {
    if (e != boost::asio::error::operation_aborted) {
        // Timer was not cancelled, take necessary action.
        if (m_tcpSocketPtr) {
            m_needToSendKeepAliveMessageTimer.expires_from_now(boost::posix_time::seconds(M_KEEP_ALIVE_INTERVAL_SECONDS));
            m_needToSendKeepAliveMessageTimer.async_wait(boost::bind(&StcpBundleSource::OnNeedToSendKeepAliveMessage_TimerExpired, this, boost::asio::placeholders::error));
            //SEND KEEPALIVE PACKET
            if (!m_dataServedAsKeepAlive) {
                static const uint32_t keepAliveData = 0; //0 is the keep alive signal 
                boost::asio::async_write(*m_tcpSocketPtr, boost::asio::buffer(&keepAliveData, sizeof(uint32_t)),
                                                                              boost::bind(&StcpBundleSource::HandleTcpSendKeepAlive, this,
                                                                                          boost::asio::placeholders::error,
                                                                                          boost::asio::placeholders::bytes_transferred));


            }
            else {
                std::cout << "notice: stcp keepalive packet not needed" << std::endl;
            }
        }
        m_dataServedAsKeepAlive = false;
    }
    else {
        //std::cout << "timer cancelled\n";
    }
}

void StcpBundleSource::DoStcpShutdown() {
    //final code to shut down tcp sockets
    m_readyToForward = false;
    if (m_tcpSocketPtr && m_tcpSocketPtr->is_open()) {
        try {
            std::cout << "shutting down tcp socket.." << std::endl;
            m_tcpSocketPtr->shutdown(boost::asio::socket_base::shutdown_type::shutdown_both);
            std::cout << "closing tcp socket.." << std::endl;
            m_tcpSocketPtr->close();
        }
        catch (const boost::system::system_error & e) {
            std::cerr << "error in DoStcpShutdown: " << e.what() << std::endl;
        }
        //don't delete the tcp socket because the Forward function is multi-threaded without a mutex to
        //increase speed, so prevent a race condition that would cause a null pointer exception
        //std::cout << "deleting tcp socket" << std::endl;
        //m_tcpSocketPtr = boost::shared_ptr<boost::asio::ip::tcp::socket>();
    }
    m_needToSendKeepAliveMessageTimer.cancel();
}

bool StcpBundleSource::ReadyToForward() {
    return m_readyToForward;
}

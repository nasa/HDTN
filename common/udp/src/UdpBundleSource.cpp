#include <string>
#include <iostream>
#include "UdpBundleSource.h"
#include <boost/lexical_cast.hpp>
#include <boost/make_shared.hpp>
#include <boost/make_unique.hpp>
#include <boost/endian/conversion.hpp>


UdpBundleSource::UdpBundleSource(const uint64_t rateBps, const unsigned int maxUnacked) :
m_work(m_ioService), //prevent stopping of ioservice until destructor
m_resolver(m_ioService),
m_rateManagerAsync(m_ioService, rateBps, 5),
m_udpSocket(m_ioService),
m_readyToForward(false),
m_useLocalConditionVariableAckReceived(false) //for destructor only
{
    m_rateManagerAsync.SetPacketsSentCallback(boost::bind(&UdpBundleSource::PacketsSentCallback, this));

    m_ioServiceThreadPtr = boost::make_unique<boost::thread>(boost::bind(&boost::asio::io_service::run, &m_ioService));

}

UdpBundleSource::~UdpBundleSource() {
    Stop();
}

void UdpBundleSource::Stop() {
    //prevent UdpBundleSource from exiting before all bundles sent and acked
    boost::mutex localMutex;
    boost::mutex::scoped_lock lock(localMutex);
    m_useLocalConditionVariableAckReceived = true;
    std::size_t previousUnacked = std::numeric_limits<std::size_t>::max();
    for (unsigned int attempt = 0; attempt < 20; ++attempt) {
        const std::size_t numUnacked = GetTotalUdpPacketsUnacked();
        if (numUnacked) {
            std::cout << "notice: UdpBundleSource destructor waiting on " << numUnacked << " unacked bundles" << std::endl;

//            std::cout << "   acked by rate: " << m_totalUdpPacketsAckedByRate << std::endl;
//            std::cout << "   acked by cb: " << m_totalUdpPacketsAckedByUdpSendCallback << std::endl;
//            std::cout << "   total sent: " << m_totalUdpPacketsSent << std::endl;

            if (previousUnacked > numUnacked) {
                previousUnacked = numUnacked;
                attempt = 0;
            }
            m_localConditionVariableAckReceived.timed_wait(lock, boost::posix_time::milliseconds(500)); // call lock.unlock() and blocks the current thread
            //thread is now unblocked, and the lock is reacquired by invoking lock.lock()
            continue;
        }
        break;
    }

    DoUdpShutdown();
    while (m_udpSocket.is_open()) {
        boost::this_thread::sleep(boost::posix_time::milliseconds(250));
    }

    //This function does not block, but instead simply signals the io_service to stop
    //All invocations of its run() or run_one() member functions should return as soon as possible.
    //Subsequent calls to run(), run_one(), poll() or poll_one() will return immediately until reset() is called.
    if (!m_ioService.stopped()) {
        m_ioService.stop(); //ioservice requires stopping before join because of the m_work object
    }

    if(m_ioServiceThreadPtr) {
        m_ioServiceThreadPtr->join();
        m_ioServiceThreadPtr.reset(); //delete it
    }

}

void UdpBundleSource::UpdateRate(uint64_t rateBitsPerSec) {
    m_rateManagerAsync.SetRate(rateBitsPerSec);
}

bool UdpBundleSource::Forward(std::vector<uint8_t> & dataVec) {

    if(!m_readyToForward) {
        std::cerr << "link not ready to forward yet" << std::endl;
        return false;
    }

    

    if (!m_rateManagerAsync.SignalNewPacketDequeuedForSend(dataVec.size())) { //not thread safe
        return false; //will print error messages inside function
    }
    

    boost::shared_ptr<std::vector<uint8_t> > udpDataToSendPtr = boost::make_shared<std::vector<uint8_t> >(std::move(dataVec));
    //dataVec invalid after this point
    m_udpSocket.async_send_to(boost::asio::buffer(*udpDataToSendPtr), m_udpDestinationEndpoint,
                                     boost::bind(&UdpBundleSource::HandleUdpSend, this, udpDataToSendPtr,
                                                 boost::asio::placeholders::error,
                                                 boost::asio::placeholders::bytes_transferred));
    return true;
}

bool UdpBundleSource::Forward(zmq::message_t & dataZmq) {

    if (!m_readyToForward) {
        std::cerr << "link not ready to forward yet" << std::endl;
        return false;
    }

    if (!m_rateManagerAsync.SignalNewPacketDequeuedForSend(dataZmq.size())) { //not thread safe
        return false; //will print error messages inside function
    }



    boost::shared_ptr<zmq::message_t> zmqDataToSendPtr = boost::make_shared<zmq::message_t>(std::move(dataZmq));
    //dataZmq invalid after this point
    m_udpSocket.async_send_to(boost::asio::buffer(zmqDataToSendPtr->data(), zmqDataToSendPtr->size()), m_udpDestinationEndpoint,
        boost::bind(&UdpBundleSource::HandleUdpSendZmqMessage, this, zmqDataToSendPtr,
            boost::asio::placeholders::error,
            boost::asio::placeholders::bytes_transferred));
    return true;
}

bool UdpBundleSource::Forward(const uint8_t* bundleData, const std::size_t size) {
    std::vector<uint8_t> vec(bundleData, bundleData + size);
    return Forward(vec);
}


std::size_t UdpBundleSource::GetTotalUdpPacketsAcked() {
    return m_rateManagerAsync.GetTotalPacketsCompletelySent();
}

std::size_t UdpBundleSource::GetTotalUdpPacketsSent() {
    return m_rateManagerAsync.GetTotalPacketsDequeuedForSend();
}

std::size_t UdpBundleSource::GetTotalUdpPacketsUnacked() {
    return m_rateManagerAsync.GetTotalPacketsBeingSent();
}

std::size_t UdpBundleSource::GetTotalBundleBytesAcked() {
    return m_rateManagerAsync.GetTotalBytesCompletelySent();
}

std::size_t UdpBundleSource::GetTotalBundleBytesSent() {
    return m_rateManagerAsync.GetTotalBytesDequeuedForSend();
}

std::size_t UdpBundleSource::GetTotalBundleBytesUnacked() {
    return m_rateManagerAsync.GetTotalBytesBeingSent();
}


void UdpBundleSource::Connect(const std::string & hostname, const std::string & port) {

    static const boost::asio::ip::resolver_query_base::flags UDP_RESOLVER_FLAGS = boost::asio::ip::resolver_query_base::canonical_name; //boost resolver flags
    std::cout << "udp resolving " << hostname << ":" << port << std::endl;
    boost::asio::ip::udp::resolver::query query(boost::asio::ip::udp::v4(), hostname, port, UDP_RESOLVER_FLAGS);
    m_resolver.async_resolve(query, boost::bind(&UdpBundleSource::OnResolve,
                                                this, boost::asio::placeholders::error,
                                                boost::asio::placeholders::results));
}

void UdpBundleSource::OnResolve(const boost::system::error_code & ec, boost::asio::ip::udp::resolver::results_type results) { // Resolved endpoints as a range.
    if(ec) {
        std::cerr << "Error resolving: " << ec.message() << std::endl;
    }
    else {
        m_udpDestinationEndpoint = *results;
        std::cout << "resolved host to " << m_udpDestinationEndpoint.address() << ":" << m_udpDestinationEndpoint.port() << ".  Binding..." << std::endl;
        try {            
            m_udpSocket.open(boost::asio::ip::udp::v4());
            m_udpSocket.bind(boost::asio::ip::udp::endpoint(boost::asio::ip::udp::v4(), 0)); //bind to 0 (random ephemeral port)

            std::cout << "UDP Bound on ephemeral port " << m_udpSocket.local_endpoint().port() << std::endl;
            std::cout << "UDP READY" << std::endl;
            m_readyToForward = true;

        }
        catch (const boost::system::system_error & e) {
            std::cerr << "Error in UdpBundleSource::OnResolve(): " << e.what() << std::endl;
            return;
        }
    }
}


void UdpBundleSource::HandleUdpSend(boost::shared_ptr<std::vector<boost::uint8_t> > dataSentPtr, const boost::system::error_code& error, std::size_t bytes_transferred) {
    if (error) {
        std::cerr << "error in UdpBundleSource::HandleUdpSend: " << error.message() << std::endl;
        DoUdpShutdown();
    }
    else {
        m_rateManagerAsync.IoServiceThreadNotifyPacketSentCallback(bytes_transferred); //ioservice thread is calling this function
    }
}

void UdpBundleSource::HandleUdpSendZmqMessage(boost::shared_ptr<zmq::message_t> dataZmqSentPtr, const boost::system::error_code& error, std::size_t bytes_transferred) {
    if (error) {
        std::cerr << "error in UdpBundleSource::HandleUdpSendZmqMessage: " << error.message() << std::endl;
        DoUdpShutdown();
    }
    else {
        m_rateManagerAsync.IoServiceThreadNotifyPacketSentCallback(bytes_transferred); //ioservice thread is calling this function
    }
}


void UdpBundleSource::DoUdpShutdown() {
    boost::asio::post(m_ioService, boost::bind(&UdpBundleSource::DoHandleSocketShutdown, this));
}

void UdpBundleSource::DoHandleSocketShutdown() {
    //final code to shut down tcp sockets
    m_readyToForward = false;
    if (m_udpSocket.is_open()) {
        try {
            std::cout << "shutting down UdpBundleSource UDP socket.." << std::endl;
            m_udpSocket.shutdown(boost::asio::socket_base::shutdown_type::shutdown_both);
        }
        catch (const boost::system::system_error & e) {
            std::cerr << "error in UdpBundleSource::DoUdpShutdown: " << e.what() << std::endl;
        }
        try {
            std::cout << "closing UdpBundleSource UDP socket.." << std::endl;
            m_udpSocket.close();
        }
        catch (const boost::system::system_error & e) {
            std::cerr << "error in UdpBundleSource::DoUdpShutdown: " << e.what() << std::endl;
        }
    }
}

bool UdpBundleSource::ReadyToForward() {
    return m_readyToForward;
}

void UdpBundleSource::SetOnSuccessfulAckCallback(const OnSuccessfulAckCallback_t & callback) {
    m_onSuccessfulAckCallback = callback;
}

void UdpBundleSource::PacketsSentCallback() {
    if (m_onSuccessfulAckCallback) {
        m_onSuccessfulAckCallback();
    }
    if (m_useLocalConditionVariableAckReceived) {
        m_localConditionVariableAckReceived.notify_one();
    }
}
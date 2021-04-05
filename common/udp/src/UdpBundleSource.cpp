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
m_rateTimer(m_ioService),
m_newDataSignalerTimer(m_ioService),
m_udpSocket(m_ioService),
m_rateBitsPerSec(rateBps),
MAX_UNACKED(maxUnacked),
m_bytesToAckByRateCb(MAX_UNACKED),
m_bytesToAckByRateCbVec(MAX_UNACKED),
m_bytesToAckByUdpSendCallbackCb(MAX_UNACKED),
m_bytesToAckByUdpSendCallbackCbVec(MAX_UNACKED),
m_readyToForward(false),
m_rateTimerIsRunning(false),
m_newDataSignalerTimerIsRunning(false),
m_useLocalConditionVariableAckReceived(false), //for destructor only

m_totalUdpPacketsAckedByUdpSendCallback(0),
m_totalBytesAckedByUdpSendCallback(0),
m_totalUdpPacketsAckedByRate(0),
m_totalBytesAckedByRate(0),
m_totalUdpPacketsSent(0),
m_totalBundleBytesSent(0)
{
    m_ioServiceThreadPtr = boost::make_unique<boost::thread>(boost::bind(&boost::asio::io_service::run, &m_ioService));

    RestartNewDataSignaler();
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

    //The DoUdpShutdown should have taken care of this, but just to be sure, we have a single threaded destructor call.
    std::cout << "Checking that newDataSignalerTimer is stopped before the ioService.stop() call.." << std::endl;
    while (m_newDataSignalerTimerIsRunning) {
        std::cout << "newDataSignalerTimer is not stopped yet..." << std::endl;
        m_newDataSignalerTimer.cancel(); //do this after readyToForward is false (DoUdpShutdown did this) (as cancel will just restart it otherwise)
        boost::this_thread::sleep(boost::posix_time::milliseconds(100));
    }
    std::cout << "newDataSignalerTimer is stopped." << std::endl;

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

    //print stats
    std::cout << "m_totalUdpPacketsSent " << m_totalUdpPacketsSent << std::endl;
    std::cout << "m_totalUdpPacketsAckedByUdpSendCallback " << m_totalUdpPacketsAckedByUdpSendCallback << std::endl;
    std::cout << "m_totalUdpPacketsAckedByRate " << m_totalUdpPacketsAckedByRate << std::endl;
    std::cout << "m_totalBundleBytesSent " << m_totalBundleBytesSent << std::endl;
    std::cout << "m_totalBytesAckedByUdpSendCallback " << m_totalBytesAckedByUdpSendCallback << std::endl;
    std::cout << "m_totalBytesAckedByRate " << m_totalBytesAckedByRate << std::endl;
}

void UdpBundleSource::UpdateRate(uint64_t rateBitsPerSec) {
    m_rateBitsPerSec = rateBitsPerSec;
}

bool UdpBundleSource::Forward(std::vector<uint8_t> & dataVec) {

    if(!m_readyToForward) {
        std::cerr << "link not ready to forward yet" << std::endl;
        return false;
    }

    const unsigned int writeIndexRate = m_bytesToAckByRateCb.GetIndexForWrite(); //don't put this in tcp async write callback
    if (writeIndexRate == UINT32_MAX) { //push check
        std::cerr << "Error in StcpBundleSource::Forward.. too many unacked packets by rate" << std::endl;
        return false;
    }

    const unsigned int writeIndexUdpSendCallback = m_bytesToAckByUdpSendCallbackCb.GetIndexForWrite(); //don't put this in tcp async write callback
    if (writeIndexUdpSendCallback == UINT32_MAX) { //push check
        std::cerr << "Error in UdpBundleSource::Forward.. too many unacked packets by udp send callback" << std::endl;
        return false;
    }
    


    ++m_totalUdpPacketsSent;
    m_totalBundleBytesSent += dataVec.size();
    

    m_bytesToAckByRateCbVec[writeIndexRate] = static_cast<uint32_t>(dataVec.size());
    m_bytesToAckByRateCb.CommitWrite(); //pushed

    m_bytesToAckByUdpSendCallbackCbVec[writeIndexUdpSendCallback] = static_cast<uint32_t>(dataVec.size());
    m_bytesToAckByUdpSendCallbackCb.CommitWrite(); //pushed

    SignalNewDataForwarded();

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

    const unsigned int writeIndexRate = m_bytesToAckByRateCb.GetIndexForWrite(); //don't put this in tcp async write callback
    if (writeIndexRate == UINT32_MAX) { //push check
        std::cerr << "Error in StcpBundleSource::Forward.. too many unacked packets by rate" << std::endl;
        return false;
    }

    const unsigned int writeIndexUdpSendCallback = m_bytesToAckByUdpSendCallbackCb.GetIndexForWrite(); //don't put this in tcp async write callback
    if (writeIndexUdpSendCallback == UINT32_MAX) { //push check
        std::cerr << "Error in UdpBundleSource::Forward.. too many unacked packets by udp send callback" << std::endl;
        return false;
    }



    ++m_totalUdpPacketsSent;
    m_totalBundleBytesSent += dataZmq.size();


    m_bytesToAckByRateCbVec[writeIndexRate] = static_cast<uint32_t>(dataZmq.size());
    m_bytesToAckByRateCb.CommitWrite(); //pushed

    m_bytesToAckByUdpSendCallbackCbVec[writeIndexUdpSendCallback] = static_cast<uint32_t>(dataZmq.size());
    m_bytesToAckByUdpSendCallbackCb.CommitWrite(); //pushed

    SignalNewDataForwarded();

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
    const std::size_t totalAckedByTcpSend = m_totalUdpPacketsAckedByUdpSendCallback;
    const std::size_t totalAckedByRate = m_totalUdpPacketsAckedByRate;
    return (totalAckedByTcpSend < totalAckedByRate) ? totalAckedByTcpSend : totalAckedByRate;
}

std::size_t UdpBundleSource::GetTotalUdpPacketsSent() {
    return m_totalUdpPacketsSent;
}

std::size_t UdpBundleSource::GetTotalUdpPacketsUnacked() {
//    std::cout << "GetTotalUdpPacketsSent(): " << GetTotalUdpPacketsSent() << std::endl;
//    std::cout << "GetTotalUdpPacketsAcked(): " << GetTotalUdpPacketsAcked() << std::endl;
    return GetTotalUdpPacketsSent() - GetTotalUdpPacketsAcked();
}

std::size_t UdpBundleSource::GetTotalBundleBytesAcked() {
    const std::size_t totalAckedByTcpSend = m_totalBytesAckedByUdpSendCallback;
    const std::size_t totalAckedByRate = m_totalBytesAckedByRate;
    return (totalAckedByTcpSend < totalAckedByRate) ? totalAckedByTcpSend : totalAckedByRate;
}

std::size_t UdpBundleSource::GetTotalBundleBytesSent() {
    return m_totalBundleBytesSent;
}

std::size_t UdpBundleSource::GetTotalBundleBytesUnacked() {
    return GetTotalBundleBytesSent() - GetTotalBundleBytesAcked();
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
        const unsigned int readIndex = m_bytesToAckByUdpSendCallbackCb.GetIndexForRead();
        if (readIndex == UINT32_MAX) { //empty
            std::cerr << "error in UdpBundleSource::HandleUdpSend: AckCallback called with empty queue" << std::endl;
        }
        else if (m_bytesToAckByUdpSendCallbackCbVec[readIndex] == bytes_transferred) {
            ++m_totalUdpPacketsAckedByUdpSendCallback;
            m_totalBytesAckedByUdpSendCallback += m_bytesToAckByUdpSendCallbackCbVec[readIndex];
            m_bytesToAckByUdpSendCallbackCb.CommitRead();
            if (m_onSuccessfulAckCallback && (m_totalUdpPacketsAckedByUdpSendCallback <= m_totalUdpPacketsAckedByRate)) { //rate segments ahead
                m_onSuccessfulAckCallback();
            }
        }
        else {
            std::cerr << "error in UdpBundleSource::HandleUdpSend: wrong bytes acked: expected " << m_bytesToAckByUdpSendCallbackCbVec[readIndex] << " but got " << bytes_transferred << std::endl;
        }
    }
}

void UdpBundleSource::HandleUdpSendZmqMessage(boost::shared_ptr<zmq::message_t> dataZmqSentPtr, const boost::system::error_code& error, std::size_t bytes_transferred) {
    if (error) {
        std::cerr << "error in UdpBundleSource::HandleUdpSendZmqMessage: " << error.message() << std::endl;
        DoUdpShutdown();
    }
    else {
        const unsigned int readIndex = m_bytesToAckByUdpSendCallbackCb.GetIndexForRead();
        if (readIndex == UINT32_MAX) { //empty
            std::cerr << "error in UdpBundleSource::HandleUdpSendZmqMessage: AckCallback called with empty queue" << std::endl;
        }
        else if (m_bytesToAckByUdpSendCallbackCbVec[readIndex] == bytes_transferred) {
            ++m_totalUdpPacketsAckedByUdpSendCallback;
            m_totalBytesAckedByUdpSendCallback += m_bytesToAckByUdpSendCallbackCbVec[readIndex];
            m_bytesToAckByUdpSendCallbackCb.CommitRead();
            if (m_totalUdpPacketsAckedByUdpSendCallback <= m_totalUdpPacketsAckedByRate) { //rate segments ahead
                if (m_onSuccessfulAckCallback) {
                    m_onSuccessfulAckCallback();
                }
                if (m_useLocalConditionVariableAckReceived) {
                    m_localConditionVariableAckReceived.notify_one();
                }
            }
        }
        else {
            std::cerr << "error in UdpBundleSource::HandleUdpSendZmqMessage: wrong bytes acked: expected " << m_bytesToAckByUdpSendCallbackCbVec[readIndex] << " but got " << bytes_transferred << std::endl;
        }
    }
}


void UdpBundleSource::RestartNewDataSignaler() {
    m_newDataSignalerTimer.expires_from_now(boost::posix_time::pos_infin);
    m_newDataSignalerTimer.async_wait(boost::bind(&UdpBundleSource::OnNewData_TimerCancelled, this, boost::asio::placeholders::error));
}

void UdpBundleSource::SignalNewDataForwarded() {
    //if (m_rateTimer.expires_from_now().is_positive()) {
    //if the rate timer is running then it will automatically pick up the new data once it expires
    if (!m_rateTimerIsRunning) {
        m_newDataSignalerTimer.cancel(); //calls OnNewData_TimerCancelled within io_service thread
    }
    
}

void UdpBundleSource::OnNewData_TimerCancelled(const boost::system::error_code& e) {
    if (e == boost::asio::error::operation_aborted) {
        // Timer was cancelled as expected.  This method keeps calls within io_service thread.
        if (m_readyToForward) { //only allow signaling when udp is running so the io_service doesn't get hung when destructor is called
            RestartNewDataSignaler();
        }
        else {
            m_newDataSignalerTimerIsRunning = false;
        }
        TryRestartRateTimer();
    }
    else {
        std::cerr << "Critical error in UdpBundleSource::OnNewData_TimerCancelled: timer was not cancelled" << std::endl;
        m_newDataSignalerTimerIsRunning = false;
    }
}

//restarts the rate timer if there is a pending ack in the cb
void UdpBundleSource::TryRestartRateTimer() {
    if (!m_rateTimerIsRunning && (m_groupingOfBytesToAckByRateVec.size() == 0)) {
        uint64_t delayMicroSec = 0;
        for (unsigned int readIndex = m_bytesToAckByRateCb.GetIndexForRead(); readIndex != UINT32_MAX; readIndex = m_bytesToAckByRateCb.GetIndexForRead()) { //notempty
            const double numBitsDouble = static_cast<double>(m_bytesToAckByRateCbVec[readIndex]) * 8.0;
            const double delayMicroSecDouble = (1.0 / m_rateBitsPerSec) * numBitsDouble * 1e6;
            delayMicroSec += static_cast<uint64_t>(delayMicroSecDouble);
// JCF -- swapped
//            m_bytesToAckByRateCb.CommitRead();
//            m_groupingOfBytesToAckByRateVec.push_back(m_bytesToAckByRateCbVec[readIndex]);
            m_groupingOfBytesToAckByRateVec.push_back(m_bytesToAckByRateCbVec[readIndex]);
            m_bytesToAckByRateCb.CommitRead();
            if (delayMicroSec >= 10000) { //try to avoid sleeping for any time smaller than 10 milliseconds
                break;
            }
        }
        if (m_groupingOfBytesToAckByRateVec.size()) {
            //std::cout << "d " << delayMicroSec << " sz " << m_groupingOfBytesToAckByRateVec.size() << std::endl;
            m_rateTimer.expires_from_now(boost::posix_time::microseconds(delayMicroSec));
            m_rateTimer.async_wait(boost::bind(&UdpBundleSource::OnRate_TimerExpired, this, boost::asio::placeholders::error));
            m_rateTimerIsRunning = true;
        }
    }
}

void UdpBundleSource::OnRate_TimerExpired(const boost::system::error_code& e) {
    m_rateTimerIsRunning = false;
    if (e != boost::asio::error::operation_aborted) {
        // Timer was not cancelled, take necessary action.
        if(m_groupingOfBytesToAckByRateVec.size() > 0) {
            m_totalUdpPacketsAckedByRate += m_groupingOfBytesToAckByRateVec.size();
//            std::cout << "m_totalUdpPacketsAckedByRate: " << m_totalUdpPacketsAckedByRate << std::endl;
            for (std::size_t i = 0; i < m_groupingOfBytesToAckByRateVec.size(); ++i) {
                m_totalBytesAckedByRate += m_groupingOfBytesToAckByRateVec[i];
            }
            m_groupingOfBytesToAckByRateVec.clear();
        
            if (m_totalUdpPacketsAckedByRate <= m_totalUdpPacketsAckedByUdpSendCallback) { //tcp send callback segments ahead
                if (m_onSuccessfulAckCallback) {
                    m_onSuccessfulAckCallback();
                }
                if (m_useLocalConditionVariableAckReceived) {
                    m_localConditionVariableAckReceived.notify_one();
                }
            }
            TryRestartRateTimer(); //must be called after commit read
        }
        else {
            std::cerr << "error in StcpBundleSource::OnRate_TimerExpired: m_groupingOfBytesToAckByRateVec is size 0" << std::endl;
        }
    }
    else {
        //std::cout << "timer cancelled\n";
    }
}



void UdpBundleSource::DoUdpShutdown() {
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
    m_newDataSignalerTimer.cancel(); //do this after readyToForward is false (as cancel will just restart it otherwise)
}

bool UdpBundleSource::ReadyToForward() {
    return m_readyToForward;
}

void UdpBundleSource::SetOnSuccessfulAckCallback(const OnSuccessfulAckCallback_t & callback) {
    m_onSuccessfulAckCallback = callback;
}

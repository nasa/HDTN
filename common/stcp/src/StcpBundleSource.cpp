#include <string>
#include <iostream>
#include "StcpBundleSource.h"
#include <boost/lexical_cast.hpp>
#include <boost/make_shared.hpp>
#include <boost/endian/conversion.hpp>
#include <boost/make_unique.hpp>

StcpBundleSource::StcpBundleSource(const uint16_t desiredKeeAliveIntervlSeconds, const uint64_t rateBps, const unsigned int maxUnacked) :
m_work(m_ioService), //prevent stopping of ioservice until destructor
m_resolver(m_ioService),
m_needToSendKeepAliveMessageTimer(m_ioService),
m_rateTimer(m_ioService),
m_newDataSignalerTimer(m_ioService),
M_KEEP_ALIVE_INTERVAL_SECONDS(desiredKeeAliveIntervlSeconds),
m_rateBitsPerSec(rateBps),
MAX_UNACKED(maxUnacked),
m_bytesToAckByRateCb(MAX_UNACKED),
m_bytesToAckByRateCbVec(MAX_UNACKED),
m_bytesToAckByTcpSendCallbackCb(MAX_UNACKED),
m_bytesToAckByTcpSendCallbackCbVec(MAX_UNACKED),
m_readyToForward(false),
m_dataServedAsKeepAlive(true),
m_rateTimerIsRunning(false),
m_newDataSignalerTimerIsRunning(false),
m_useLocalConditionVariableAckReceived(false), //for destructor only

m_totalDataSegmentsAckedByTcpSendCallback(0),
m_totalBytesAckedByTcpSendCallback(0),
m_totalDataSegmentsAckedByRate(0),
m_totalBytesAckedByRate(0),
m_totalDataSegmentsSent(0),
m_totalBundleBytesSent(0),
m_totalStcpBytesSent(0)
{
    m_handleTcpSendCallback = boost::bind(&StcpBundleSource::HandleTcpSend, this, boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred);
    m_handleTcpSendKeepAliveCallback = boost::bind(&StcpBundleSource::HandleTcpSendKeepAlive, this, boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred);

    m_ioServiceThreadPtr = boost::make_unique<boost::thread>(boost::bind(&boost::asio::io_service::run, &m_ioService));

    RestartNewDataSignaler();
}

StcpBundleSource::~StcpBundleSource() {

    //prevent StcpBundleSource from exiting before all bundles sent and acked
    boost::mutex localMutex;
    boost::mutex::scoped_lock lock(localMutex);
    m_useLocalConditionVariableAckReceived = true;
    std::size_t previousUnacked = std::numeric_limits<std::size_t>::max();
    for (unsigned int attempt = 0; attempt < 10; ++attempt) {
        const std::size_t numUnacked = GetTotalDataSegmentsUnacked();
        if (numUnacked) {
            std::cout << "notice: StcpBundleSource destructor waiting on " << numUnacked << " unacked bundles" << std::endl;
            
            std::cout << "   acked by rate: " << m_totalDataSegmentsAckedByRate << std::endl;
            std::cout << "   acked by cb: " << m_totalDataSegmentsAckedByTcpSendCallback << std::endl;
            std::cout << "   total sent: " << m_totalDataSegmentsSent << std::endl;
            
            if (previousUnacked > numUnacked) {
                previousUnacked = numUnacked;
                attempt = 0;
            }
            m_localConditionVariableAckReceived.timed_wait(lock, boost::posix_time::milliseconds(250)); // call lock.unlock() and blocks the current thread
            //thread is now unblocked, and the lock is reacquired by invoking lock.lock()
            continue;
        }
        break;
    }
    
    DoStcpShutdown();

    //The DoStcpShutdown should have taken care of this, but just to be sure, we have a single threaded destructor call.
    std::cout << "Checking that newDataSignalerTimer is stopped before the ioService.stop() call.." << std::endl;
    while (m_newDataSignalerTimerIsRunning) {
        std::cout << "newDataSignalerTimer is not stopped yet..." << std::endl;
        m_newDataSignalerTimer.cancel(); //do this after readyToForward is false (DoUdpShutdown did this) (as cancel will just restart it otherwise)
        boost::this_thread::sleep(boost::posix_time::milliseconds(100));
    }
    std::cout << "newDataSignalerTimer is stopped." << std::endl;

    m_tcpAsyncSenderPtr.reset(); //stop this first
    //This function does not block, but instead simply signals the io_service to stop
    //All invocations of its run() or run_one() member functions should return as soon as possible.
    //Subsequent calls to run(), run_one(), poll() or poll_one() will return immediately until reset() is called.
    m_ioService.stop(); //ioservice requires stopping before join because of the m_work object

    if(m_ioServiceThreadPtr) {
        m_ioServiceThreadPtr->join();
        m_ioServiceThreadPtr.reset(); //delete it
    }

    //print stats
    std::cout << "m_totalDataSegmentsSent " << m_totalDataSegmentsSent << std::endl;
    std::cout << "m_totalDataSegmentsAckedByTcpSendCallback " << m_totalDataSegmentsAckedByTcpSendCallback << std::endl;
    std::cout << "m_totalDataSegmentsAckedByRate " << m_totalDataSegmentsAckedByRate << std::endl;
    std::cout << "m_totalBundleBytesSent " << m_totalBundleBytesSent << std::endl;
    std::cout << "m_totalStcpBytesSent " << m_totalStcpBytesSent << std::endl;
    std::cout << "m_totalBytesAckedByTcpSendCallback " << m_totalBytesAckedByTcpSendCallback << std::endl;
    std::cout << "m_totalBytesAckedByRate " << m_totalBytesAckedByRate << std::endl;
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

void StcpBundleSource::GenerateDataUnitHeaderOnly(std::vector<uint8_t> & dataUnit, uint32_t sizeContents) {
    uint32_t sizeContentsBigEndian = sizeContents;
    boost::endian::native_to_big_inplace(sizeContentsBigEndian);


    dataUnit.resize(sizeof(uint32_t));

    memcpy(&dataUnit[0], &sizeContentsBigEndian, sizeof(uint32_t));
}

void StcpBundleSource::UpdateRate(uint64_t rateBitsPerSec) {
    m_rateBitsPerSec = rateBitsPerSec;
}



bool StcpBundleSource::Forward(zmq::message_t & dataZmq) {
    if (!m_readyToForward) {
        std::cerr << "link not ready to forward yet" << std::endl;
        return false;
    }

    const unsigned int writeIndexRate = m_bytesToAckByRateCb.GetIndexForWrite(); //don't put this in tcp async write callback
    if (writeIndexRate == UINT32_MAX) { //push check
        std::cerr << "Error in StcpBundleSource::Forward.. too many unacked packets by rate" << std::endl;
        return false;
    }

    const unsigned int writeIndexTcpSendCallback = m_bytesToAckByTcpSendCallbackCb.GetIndexForWrite(); //don't put this in tcp async write callback
    if (writeIndexTcpSendCallback == UINT32_MAX) { //push check
        std::cerr << "Error in StcpBundleSource::Forward.. too many unacked packets by tcp send callback" << std::endl;
        return false;
    }


    ++m_totalDataSegmentsSent;
    m_totalBundleBytesSent += dataZmq.size();

    const uint32_t dataUnitSize = static_cast<uint32_t>(dataZmq.size() + sizeof(uint32_t));
    m_totalStcpBytesSent += dataUnitSize;

    m_bytesToAckByRateCbVec[writeIndexRate] = dataUnitSize;
    m_bytesToAckByRateCb.CommitWrite(); //pushed

    m_bytesToAckByTcpSendCallbackCbVec[writeIndexTcpSendCallback] = dataUnitSize;
    m_bytesToAckByTcpSendCallbackCb.CommitWrite(); //pushed

    SignalNewDataForwarded();

    m_dataServedAsKeepAlive = true;

    std::unique_ptr<std::vector<uint8_t> > dataUnitHeaderPtr = boost::make_unique<std::vector<uint8_t> >();

    StcpBundleSource::GenerateDataUnitHeaderOnly(*dataUnitHeaderPtr, static_cast<uint32_t>(dataZmq.size()));
    std::unique_ptr<TcpAsyncSenderElement> el;
    TcpAsyncSenderElement::Create(el, std::move(dataUnitHeaderPtr), boost::make_unique<zmq::message_t>(std::move(dataZmq)), &m_handleTcpSendCallback);
    m_tcpAsyncSenderPtr->AsyncSend_ThreadSafe(std::move(el));

    return true;
}

bool StcpBundleSource::Forward(std::vector<uint8_t> & dataVec) {
    if (!m_readyToForward) {
        std::cerr << "link not ready to forward yet" << std::endl;
        return false;
    }

    const unsigned int writeIndexRate = m_bytesToAckByRateCb.GetIndexForWrite(); //don't put this in tcp async write callback
    if (writeIndexRate == UINT32_MAX) { //push check
        std::cerr << "Error in StcpBundleSource::Forward.. too many unacked packets by rate" << std::endl;
        return false;
    }

    const unsigned int writeIndexTcpSendCallback = m_bytesToAckByTcpSendCallbackCb.GetIndexForWrite(); //don't put this in tcp async write callback
    if (writeIndexTcpSendCallback == UINT32_MAX) { //push check
        std::cerr << "Error in StcpBundleSource::Forward.. too many unacked packets by tcp send callback" << std::endl;
        return false;
    }



    ++m_totalDataSegmentsSent;
    m_totalBundleBytesSent += dataVec.size();

    const uint32_t dataUnitSize = static_cast<uint32_t>(dataVec.size() + sizeof(uint32_t));
    m_totalStcpBytesSent += dataUnitSize;

    m_bytesToAckByRateCbVec[writeIndexRate] = dataUnitSize;
    m_bytesToAckByRateCb.CommitWrite(); //pushed

    m_bytesToAckByTcpSendCallbackCbVec[writeIndexTcpSendCallback] = dataUnitSize;
    m_bytesToAckByTcpSendCallbackCb.CommitWrite(); //pushed

    SignalNewDataForwarded();

    m_dataServedAsKeepAlive = true;

    std::unique_ptr<std::vector<uint8_t> > dataUnitHeaderPtr = boost::make_unique<std::vector<uint8_t> >();

    StcpBundleSource::GenerateDataUnitHeaderOnly(*dataUnitHeaderPtr, static_cast<uint32_t>(dataVec.size()));
    std::unique_ptr<TcpAsyncSenderElement> el;
    TcpAsyncSenderElement::Create(el, std::move(dataUnitHeaderPtr), boost::make_unique<std::vector<uint8_t> >(std::move(dataVec)), &m_handleTcpSendCallback);
    m_tcpAsyncSenderPtr->AsyncSend_ThreadSafe(std::move(el));

    return true;
}


bool StcpBundleSource::Forward(const uint8_t* bundleData, const std::size_t size) {
    std::vector<uint8_t> vec(bundleData, bundleData + size);
    return Forward(vec);
}


std::size_t StcpBundleSource::GetTotalDataSegmentsAcked() {
    const std::size_t totalAckedByTcpSend = m_totalDataSegmentsAckedByTcpSendCallback;
    const std::size_t totalAckedByRate = m_totalDataSegmentsAckedByRate;
    return (totalAckedByTcpSend < totalAckedByRate) ? totalAckedByTcpSend : totalAckedByRate;
}

std::size_t StcpBundleSource::GetTotalDataSegmentsSent() {
    return m_totalDataSegmentsSent;
}

std::size_t StcpBundleSource::GetTotalDataSegmentsUnacked() {
    return GetTotalDataSegmentsSent() - GetTotalDataSegmentsAcked();
}

std::size_t StcpBundleSource::GetTotalBundleBytesAcked() {
    const std::size_t totalAckedByTcpSend = m_totalBytesAckedByTcpSendCallback;
    const std::size_t totalAckedByRate = m_totalBytesAckedByRate;
    return (totalAckedByTcpSend < totalAckedByRate) ? totalAckedByTcpSend : totalAckedByRate;
}

std::size_t StcpBundleSource::GetTotalBundleBytesSent() {
    return m_totalBundleBytesSent;
}

std::size_t StcpBundleSource::GetTotalBundleBytesUnacked() {
    return GetTotalBundleBytesSent() - GetTotalBundleBytesAcked();
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
        m_tcpAsyncSenderPtr = boost::make_unique<TcpAsyncSender>(100, m_tcpSocketPtr);

        StartTcpReceive();
    }
}



void StcpBundleSource::HandleTcpSend(const boost::system::error_code& error, std::size_t bytes_transferred) {
    if (error) {
        std::cerr << "error in StcpBundleSource::HandleTcpSend: " << error.message() << std::endl;
        DoStcpShutdown();
    }
    else {
        const unsigned int readIndex = m_bytesToAckByTcpSendCallbackCb.GetIndexForRead();
        if (readIndex == UINT32_MAX) { //empty
            std::cerr << "error: AckCallback called with empty queue" << std::endl;
        }
        else if (m_bytesToAckByTcpSendCallbackCbVec[readIndex] == bytes_transferred) {
            ++m_totalDataSegmentsAckedByTcpSendCallback;
            m_totalBytesAckedByTcpSendCallback += m_bytesToAckByTcpSendCallbackCbVec[readIndex];
            m_bytesToAckByTcpSendCallbackCb.CommitRead();
            if (m_totalDataSegmentsAckedByTcpSendCallback <= m_totalDataSegmentsAckedByRate) { //rate segments ahead
                if (m_onSuccessfulAckCallback) {
                    m_onSuccessfulAckCallback();
                }
                if (m_useLocalConditionVariableAckReceived) {
                    m_localConditionVariableAckReceived.notify_one();
                }
            }
        }
        else {
            std::cerr << "error in StcpBundleSource::HandleTcpSend: wrong bytes acked: expected " << m_bytesToAckByTcpSendCallbackCbVec[readIndex] << " but got " << bytes_transferred << std::endl;
        }
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

void StcpBundleSource::RestartNewDataSignaler() {
    m_newDataSignalerTimer.expires_from_now(boost::posix_time::pos_infin);
    m_newDataSignalerTimer.async_wait(boost::bind(&StcpBundleSource::OnNewData_TimerCancelled, this, boost::asio::placeholders::error));
}

void StcpBundleSource::SignalNewDataForwarded() {
    //if (m_rateTimer.expires_from_now().is_positive()) {
    //if the rate timer is running then it will automatically pick up the new data once it expires
    if (!m_rateTimerIsRunning) {
        m_newDataSignalerTimer.cancel(); //calls OnNewData_TimerCancelled within io_service thread
    }
    
}

void StcpBundleSource::OnNewData_TimerCancelled(const boost::system::error_code& e) {
    if (e == boost::asio::error::operation_aborted) {
        // Timer was cancelled as expected.  This method keeps calls within io_service thread.
        if (m_readyToForward) { //only allow signaling when tcp is running so the io_service doesn't get hung when destructor is called
            RestartNewDataSignaler();
        }
        else {
            m_newDataSignalerTimerIsRunning = false;
        }
        TryRestartRateTimer();
    }
    else {
        std::cerr << "Critical error in StcpBundleSource::OnNewData_TimerCancelled: timer was not cancelled" << std::endl;
        m_newDataSignalerTimerIsRunning = false;
    }
}

//restarts the rate timer if there is a pending ack in the cb
void StcpBundleSource::TryRestartRateTimer() {
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
            m_rateTimer.async_wait(boost::bind(&StcpBundleSource::OnRate_TimerExpired, this, boost::asio::placeholders::error));
            m_rateTimerIsRunning = true;
        }
    }
}

void StcpBundleSource::OnRate_TimerExpired(const boost::system::error_code& e) {
    m_rateTimerIsRunning = false;
    if (e != boost::asio::error::operation_aborted) {
        // Timer was not cancelled, take necessary action.
        if(m_groupingOfBytesToAckByRateVec.size() > 0) {
            m_totalDataSegmentsAckedByRate += m_groupingOfBytesToAckByRateVec.size();
            for (std::size_t i = 0; i < m_groupingOfBytesToAckByRateVec.size(); ++i) {
                m_totalBytesAckedByRate += m_groupingOfBytesToAckByRateVec[i];
            }
            m_groupingOfBytesToAckByRateVec.clear();
        
            if (m_totalDataSegmentsAckedByRate <= m_totalDataSegmentsAckedByTcpSendCallback) { //tcp send callback segments ahead
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



void StcpBundleSource::OnNeedToSendKeepAliveMessage_TimerExpired(const boost::system::error_code& e) {
    if (e != boost::asio::error::operation_aborted) {
        // Timer was not cancelled, take necessary action.
        if (m_tcpSocketPtr) {
            m_needToSendKeepAliveMessageTimer.expires_from_now(boost::posix_time::seconds(M_KEEP_ALIVE_INTERVAL_SECONDS));
            m_needToSendKeepAliveMessageTimer.async_wait(boost::bind(&StcpBundleSource::OnNeedToSendKeepAliveMessage_TimerExpired, this, boost::asio::placeholders::error));
            //SEND KEEPALIVE PACKET
            if (!m_dataServedAsKeepAlive) {
                static const uint32_t keepAliveData = 0; //0 is the keep alive signal 

                std::unique_ptr<TcpAsyncSenderElement> el;
                TcpAsyncSenderElement::Create(el, (const uint8_t *) &keepAliveData, sizeof(keepAliveData), &m_handleTcpSendKeepAliveCallback);
                m_tcpAsyncSenderPtr->AsyncSend_ThreadSafe(std::move(el));
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
            std::cout << "shutting down StcpBundleSource TCP socket.." << std::endl;
            m_tcpSocketPtr->shutdown(boost::asio::socket_base::shutdown_type::shutdown_both);
        }
        catch (const boost::system::system_error & e) {
            std::cerr << "error in StcpBundleSource::DoStcpShutdown: " << e.what() << std::endl;
        }
        try {
            std::cout << "closing StcpBundleSource TCP socket socket.." << std::endl;
            m_tcpSocketPtr->close();
        }
        catch (const boost::system::system_error & e) {
            std::cerr << "error in StcpBundleSource::DoStcpShutdown: " << e.what() << std::endl;
        }
        //don't delete the tcp socket because the Forward function is multi-threaded without a mutex to
        //increase speed, so prevent a race condition that would cause a null pointer exception
        //std::cout << "deleting tcp socket" << std::endl;
        //m_tcpSocketPtr = boost::shared_ptr<boost::asio::ip::tcp::socket>();
    }
    m_needToSendKeepAliveMessageTimer.cancel();
    m_newDataSignalerTimer.cancel(); //do this after readyToForward is false (as cancel will just restart it otherwise)
}

bool StcpBundleSource::ReadyToForward() {
    return m_readyToForward;
}

void StcpBundleSource::SetOnSuccessfulAckCallback(const OnSuccessfulAckCallback_t & callback) {
    m_onSuccessfulAckCallback = callback;
}

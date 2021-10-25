#ifndef TCPCL_INDUCT_H
#define TCPCL_INDUCT_H 1

#include <string>
#include "Induct.h"
#include "TcpclBundleSink.h"
#include <list>
#include <boost/make_unique.hpp>

class TcpclInduct : public Induct {
public:
    TcpclInduct(const InductProcessBundleCallback_t & inductProcessBundleCallback, const induct_element_config_t & inductConfig,
        const uint64_t myNodeId, const uint64_t maxBundleSizeBytes, const OnNewOpportunisticLinkCallback_t & onNewOpportunisticLinkCallback,
        const OnDeletedOpportunisticLinkCallback_t & onDeletedOpportunisticLinkCallback);
    virtual ~TcpclInduct();
    bool ForwardOnOpportunisticLink(const uint64_t remoteNodeId, std::vector<uint8_t> & dataVec, const uint32_t timeoutSeconds);
    bool ForwardOnOpportunisticLink(const uint64_t remoteNodeId, zmq::message_t & dataZmq, const uint32_t timeoutSeconds);
    bool ForwardOnOpportunisticLink(const uint64_t remoteNodeId, const uint8_t* bundleData, const std::size_t size, const uint32_t timeoutSeconds);
private:
    struct OpportunisticBundleQueue {
        OpportunisticBundleQueue() {

        }
        std::size_t GetQueueSize() {
            return m_dataToSendQueue.size();
        }
        void PushMove_ThreadSafe(zmq::message_t & msg) {
            boost::mutex::scoped_lock lock(m_mutex);
            m_dataToSendQueue.emplace(boost::make_unique<zmq::message_t>(std::move(msg)), std::vector<uint8_t>());
        }
        void PushMove_ThreadSafe(std::vector<uint8_t> & msg) {
            boost::mutex::scoped_lock lock(m_mutex);
            m_dataToSendQueue.emplace(std::unique_ptr<zmq::message_t>(), std::move(msg));
        }
        void PushMove_ThreadSafe(std::pair<std::unique_ptr<zmq::message_t>, std::vector<uint8_t> > & msgPair) {
            boost::mutex::scoped_lock lock(m_mutex);
            m_dataToSendQueue.push(std::move(msgPair));
        }
        bool TryPop_ThreadSafe(std::pair<std::unique_ptr<zmq::message_t>, std::vector<uint8_t> > & msgPair) {
            boost::mutex::scoped_lock lock(m_mutex);
            if (m_dataToSendQueue.empty()) {
                return false;
            }
            msgPair = std::move(m_dataToSendQueue.front());
            m_dataToSendQueue.pop();
                
            return true;
        }
        void WaitUntilNotifiedOr250MsTimeout() {
            boost::mutex::scoped_lock lock(m_mutex);
            m_conditionVariable.timed_wait(lock, boost::posix_time::milliseconds(250)); // call lock.unlock() and blocks the current thread
        }
        void NotifyAll() {
            m_conditionVariable.notify_all();
        }
        boost::mutex m_mutex;
        boost::condition_variable m_conditionVariable;
        std::queue<std::pair<std::unique_ptr<zmq::message_t>, std::vector<uint8_t> > > m_dataToSendQueue;
    };


    TcpclInduct();
    void StartTcpAccept();
    void HandleTcpAccept(boost::shared_ptr<boost::asio::ip::tcp::socket> & newTcpSocketPtr, const boost::system::error_code& error);
    void ConnectionReadyToBeDeletedNotificationReceived();
    void RemoveInactiveTcpConnections();
    void DisableRemoveInactiveTcpConnections();
    bool ForwardOnOpportunisticLink(const uint64_t remoteNodeId, zmq::message_t * zmqMsgPtr, std::vector<uint8_t> * vec8Ptr, const uint32_t timeoutSeconds);
    bool BundleSinkTryGetData_FromIoServiceThread(OpportunisticBundleQueue & opportunisticBundleQueue, std::pair<std::unique_ptr<zmq::message_t>, std::vector<uint8_t> > & bundleDataPair);
    void OnContactHeaderCallback_FromIoServiceThread(TcpclBundleSink * thisTcpclBundleSinkPtr);
    void BundleSinkNotifyOpportunisticDataAcked_FromIoServiceThread(OpportunisticBundleQueue & opportunisticBundleQueue);
    bool NotifyBundleReadyToSend_FromIoServiceThread(const uint64_t remoteNodeId);

    boost::asio::io_service m_ioService;
    boost::asio::ip::tcp::acceptor m_tcpAcceptor;
    std::unique_ptr<boost::asio::io_service::work> m_workPtr;
    std::unique_ptr<boost::thread> m_ioServiceThreadPtr;
    std::list<TcpclBundleSink> m_listTcpclBundleSinks;
    const uint64_t M_MY_NODE_ID;
    volatile bool m_allowRemoveInactiveTcpConnections;
    const uint64_t M_MAX_BUNDLE_SIZE_BYTES;

    

    std::map<uint64_t, OpportunisticBundleQueue> m_mapNodeIdToOpportunisticBundleQueue;
    boost::mutex m_mapNodeIdToOpportunisticBundleQueueMutex;
    const OnNewOpportunisticLinkCallback_t m_onNewOpportunisticLinkCallback;
    const OnDeletedOpportunisticLinkCallback_t m_onDeletedOpportunisticLinkCallback;
};


#endif // TCPCL_INDUCT_H


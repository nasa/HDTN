#include "Induct.h"
#include "Logger.h"
#include <boost/make_unique.hpp>
#include <memory>
#include <boost/lexical_cast.hpp>

//INDUCT
Induct::Induct(const InductProcessBundleCallback_t & inductProcessBundleCallback, const induct_element_config_t & inductConfig) :
    m_inductProcessBundleCallback(inductProcessBundleCallback),
    m_inductConfig(inductConfig)
{}
Induct::~Induct() {}

Induct::OpportunisticBundleQueue::OpportunisticBundleQueue() {}
Induct::OpportunisticBundleQueue::~OpportunisticBundleQueue() {
    LOG_INFO(hdtn::Logger::SubProcess::none) << "opportunistic link with m_remoteNodeId " << m_remoteNodeId << " terminated with " << m_dataToSendQueue.size() << " bundles queued";
}
std::size_t Induct::OpportunisticBundleQueue::GetQueueSize() {
    return m_dataToSendQueue.size();
}
void Induct::OpportunisticBundleQueue::PushMove_ThreadSafe(zmq::message_t & msg) {
    boost::mutex::scoped_lock lock(m_mutex);
    m_dataToSendQueue.emplace(boost::make_unique<zmq::message_t>(std::move(msg)), std::vector<uint8_t>());
}
void Induct::OpportunisticBundleQueue::PushMove_ThreadSafe(std::vector<uint8_t> & msg) {
    boost::mutex::scoped_lock lock(m_mutex);
    m_dataToSendQueue.emplace(std::unique_ptr<zmq::message_t>(), std::move(msg));
}
void Induct::OpportunisticBundleQueue::PushMove_ThreadSafe(std::pair<std::unique_ptr<zmq::message_t>, std::vector<uint8_t> > & msgPair) {
    boost::mutex::scoped_lock lock(m_mutex);
    m_dataToSendQueue.push(std::move(msgPair));
}
bool Induct::OpportunisticBundleQueue::TryPop_ThreadSafe(std::pair<std::unique_ptr<zmq::message_t>, std::vector<uint8_t> > & msgPair) {
    {
        boost::mutex::scoped_lock lock(m_mutex);
        if (m_dataToSendQueue.empty()) {
            return false;
        }
        msgPair = std::move(m_dataToSendQueue.front());
        m_dataToSendQueue.pop();
    }
    m_conditionVariable.notify_all();
    return true;
}
void Induct::OpportunisticBundleQueue::WaitUntilNotifiedOr250MsTimeout() {
    boost::mutex::scoped_lock lock(m_mutex);
    m_conditionVariable.timed_wait(lock, boost::posix_time::milliseconds(250)); // call lock.unlock() and blocks the current thread
}
void Induct::OpportunisticBundleQueue::NotifyAll() {
    m_conditionVariable.notify_all();
}
    
bool Induct::ForwardOnOpportunisticLink(const uint64_t remoteNodeId, std::vector<uint8_t> & dataVec, const uint32_t timeoutSeconds) {
    return ForwardOnOpportunisticLink(remoteNodeId, NULL, &dataVec, timeoutSeconds);
}
bool Induct::ForwardOnOpportunisticLink(const uint64_t remoteNodeId, zmq::message_t & dataZmq, const uint32_t timeoutSeconds) {
    return ForwardOnOpportunisticLink(remoteNodeId, &dataZmq, NULL, timeoutSeconds);
}
bool Induct::ForwardOnOpportunisticLink(const uint64_t remoteNodeId, const uint8_t* bundleData, const std::size_t size, const uint32_t timeoutSeconds) {
    std::vector<uint8_t> dataVec(bundleData, bundleData + size);
    return ForwardOnOpportunisticLink(remoteNodeId, NULL, &dataVec, timeoutSeconds);
}
bool Induct::ForwardOnOpportunisticLink(const uint64_t remoteNodeId, zmq::message_t * zmqMsgPtr, std::vector<uint8_t> * vec8Ptr, const uint32_t timeoutSeconds) {
    m_mapNodeIdToOpportunisticBundleQueueMutex.lock();
    std::map<uint64_t, OpportunisticBundleQueue>::iterator obqIt = m_mapNodeIdToOpportunisticBundleQueue.find(remoteNodeId);
    m_mapNodeIdToOpportunisticBundleQueueMutex.unlock();
    if (obqIt == m_mapNodeIdToOpportunisticBundleQueue.end()) {
        LOG_ERROR(subprocess) << "error in Induct::ForwardOnOpportunisticLink: opportunistic link with remoteNodeId " << remoteNodeId << " does not exist";
        return false;
    }
    OpportunisticBundleQueue & opportunisticBundleQueue = obqIt->second;
    boost::posix_time::ptime timeoutExpiry((timeoutSeconds != 0) ?
        boost::posix_time::special_values::not_a_date_time :
        boost::posix_time::special_values::neg_infin); //allow zero time to fail immediately if full
    //while (opportunisticBundleQueue.GetQueueSize() > 5) { //can't use queue size here, need to go on bundle acks (which limits the max queue size anyway)
    //const unsigned int maxBundlesInPipeline = opportunisticBundleQueue.m_bidirectionalLinkPtr->Virtual_GetMaxTxBundlesInPipeline();
    //while ((opportunisticBundleQueue.m_bidirectionalLinkPtr->Virtual_GetTotalBundlesUnacked() >= maxBundlesInPipeline)
    //    || (opportunisticBundleQueue.GetQueueSize() >= maxBundlesInPipeline))
    while (opportunisticBundleQueue.GetQueueSize() >= opportunisticBundleQueue.m_maxTxBundlesInPipeline) {
        if (timeoutExpiry == boost::posix_time::special_values::not_a_date_time) {
            timeoutExpiry = boost::posix_time::microsec_clock::universal_time() + boost::posix_time::seconds(timeoutSeconds);
        }
        else if (timeoutExpiry < boost::posix_time::microsec_clock::universal_time()) {
            std::string msg = "notice in Induct::ForwardOnOpportunisticLink: timed out after " +
                boost::lexical_cast<std::string>(timeoutSeconds) +
                " seconds because it has too many pending opportunistic bundles the queue for remoteNodeId " +
                boost::lexical_cast<std::string>(remoteNodeId);
            LOG_INFO(hdtn::Logger::SubProcess::none) << msg;
            return false;

        }
        Virtual_PostNotifyBundleReadyToSend_FromIoServiceThread(remoteNodeId);
        opportunisticBundleQueue.WaitUntilNotifiedOr250MsTimeout();
        //thread is now unblocked, and the lock is reacquired by invoking lock.lock()
    }
    if (zmqMsgPtr) {
        opportunisticBundleQueue.PushMove_ThreadSafe(*zmqMsgPtr);
    }
    else {
        opportunisticBundleQueue.PushMove_ThreadSafe(*vec8Ptr);
    }
    Virtual_PostNotifyBundleReadyToSend_FromIoServiceThread(remoteNodeId);
    return true;
}

void Induct::Virtual_PostNotifyBundleReadyToSend_FromIoServiceThread(const uint64_t remoteNodeId) {}

bool Induct::BundleSinkTryGetData_FromIoServiceThread(OpportunisticBundleQueue & opportunisticBundleQueue, std::pair<std::unique_ptr<zmq::message_t>, std::vector<uint8_t> > & bundleDataPair) {
    return opportunisticBundleQueue.TryPop_ThreadSafe(bundleDataPair);
}
void Induct::BundleSinkNotifyOpportunisticDataAcked_FromIoServiceThread(OpportunisticBundleQueue & opportunisticBundleQueue) {
    Virtual_PostNotifyBundleReadyToSend_FromIoServiceThread(opportunisticBundleQueue.m_remoteNodeId);
    opportunisticBundleQueue.NotifyAll();
}

#ifndef OUTDUCT_MANAGER_H
#define OUTDUCT_MANAGER_H 1

#include "Outduct.h"
#include <map>
#include <boost/shared_ptr.hpp>
#include <vector>
#include <zmq.hpp>
#include <boost/thread.hpp>

class OutductManager {
public:
    

    OutductManager();
    ~OutductManager();
    bool LoadOutductsFromConfig(const OutductsConfig & outductsConfig);
    void Clear();
    bool AllReadyToForward() const;
    void StopAllOutducts();
    Outduct * GetOutductByFlowId(const uint64_t flowId);
    Outduct * GetOutductByOutductUuid(const uint64_t uuid);

    bool Forward(uint64_t flowId, const uint8_t* bundleData, const std::size_t size);
    bool Forward(uint64_t flowId, zmq::message_t & movableDataZmq);
    bool Forward(uint64_t flowId, std::vector<uint8_t> & movableDataVec);

    bool Forward_Blocking(uint64_t flowId, const uint8_t* bundleData, const std::size_t size, const uint32_t timeoutSeconds);
    bool Forward_Blocking(uint64_t flowId, zmq::message_t & movableDataZmq, const uint32_t timeoutSeconds);
    bool Forward_Blocking(uint64_t flowId, std::vector<uint8_t> & movableDataVec, const uint32_t timeoutSeconds);
private:
    void OnSuccessfulBundleAck(uint64_t uuidIndex);

    struct thread_communication_t {
        boost::condition_variable m_cv;
        boost::mutex m_mutex;
        boost::mutex::scoped_lock m_lock;

        thread_communication_t() : m_lock(m_mutex) {}
        void Notify() {
            m_cv.notify_one();
        }
        void Wait250msOrUntilNotified() {
            m_cv.timed_wait(m_lock, boost::posix_time::milliseconds(250));
        }
    };

    std::map<uint64_t, boost::shared_ptr<Outduct> > m_flowIdToOutductMap;
    std::vector<boost::shared_ptr<Outduct> > m_outductsVec;
    std::vector<std::unique_ptr<thread_communication_t> > m_threadCommunicationVec;
    uint64_t m_numEventsTooManyUnackedBundles;
};

#endif // OUTDUCT_MANAGER_H


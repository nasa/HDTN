#ifndef _HDTN_EGRESS_ASYNC_H
#define _HDTN_EGRESS_ASYNC_H


#include <iostream>
#include <string>

#include "message.hpp"
#include "zmq.hpp"
#include <boost/thread.hpp>
#include <boost/asio.hpp>
#include <map>
#include <queue>
#include "HdtnConfig.h"
#include "OutductManager.h"
#include "CircularIndexBufferSingleProducerSingleConsumerConfigurable.h"
#include "Logger.h"

#define HEGR_NAME_SZ (32)
#define HEGR_ENTRY_COUNT (1 << 20)
#define HEGR_ENTRY_SZ (256)
#define HEGR_FLAG_ACTIVE (0x0001)
#define HEGR_FLAG_UP (0x0002)
#define HEGR_HARD_IFG (0x0004)
#define HEGR_FLAG_UDP (0x0010)
#define HEGR_FLAG_STCPv1 (0x0020)
#define HEGR_FLAG_LTP (0x0040)
#define HEGR_FLAG_TCPCLv3 (0x0080)

namespace hdtn {


class HegrManagerAsync {
public:
    HegrManagerAsync();
    ~HegrManagerAsync();
    void Stop();
    void Init(const HdtnConfig & hdtnConfig, zmq::context_t * hdtnOneProcessZmqInprocContextPtr = NULL);

    uint64_t m_bundleCount;
    uint64_t m_bundleData;
    uint64_t m_messageCount;

    std::unique_ptr<zmq::context_t> m_zmqCtx_ingressEgressPtr;
    std::unique_ptr<zmq::socket_t> m_zmqPullSock_boundIngressToConnectingEgressPtr;
    std::unique_ptr<zmq::socket_t> m_zmqPushSock_connectingEgressToBoundIngressPtr;
    std::unique_ptr<zmq::context_t> m_zmqCtx_storageEgressPtr;
    std::unique_ptr<zmq::socket_t> m_zmqPullSock_connectingStorageToBoundEgressPtr;
    std::unique_ptr<zmq::socket_t> m_zmqPushSock_boundEgressToConnectingStoragePtr;

private:
    void ReadZmqThreadFunc();
    void OnSuccessfulBundleAck(uint64_t outductUuidIndex);
    void ProcessZmqMessagesThreadFunc(
        CircularIndexBufferSingleProducerSingleConsumerConfigurable & cb,
        std::vector<hdtn::ToEgressHdr> & toEgressHeaderMessages,
        std::vector<zmq::message_t> & payloadMessages);
    OutductManager m_outductManager;
    HdtnConfig m_hdtnConfig;
    
    boost::condition_variable m_conditionVariableProcessZmqMessages;


    std::unique_ptr<boost::thread> m_threadZmqReaderPtr;
    volatile bool m_running;
};

}  // namespace hdtn

#endif

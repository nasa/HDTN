#ifndef _HDTN_EGRESS_ASYNC_H
#define _HDTN_EGRESS_ASYNC_H


#include <iostream>
#include <string>

#include "message.hpp"
#include "paths.hpp"
#include "zmq.hpp"
#include <boost/thread.hpp>
#include <boost/asio.hpp>
#include <map>
#include <queue>
#include "TcpclBundleSource.h"
#include "StcpBundleSource.h"
#include "UdpBundleSource.h"

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

class HegrEntryAsync {
public:
    // JCF, seems to be missing virtual destructor, added below
    virtual ~HegrEntryAsync();

    HegrEntryAsync();

    /**
    Initializes the egress port
    */
    virtual void Init(uint64_t flags);

    /**
    Sets the active label for this instance
    */
    virtual void Label(uint64_t label);

    /**
    Sets the active name for this instance. char* n can be of length HEGR_NAME_SZ
    - 1.
    */
    virtual void Name(char *n);

    /**
    Sets a target data rate for an egress port - most often used in conjunction
    with HARD_IFG

    This is really only useful when one wants the egress to perform its own rate
    control - elements internal to the cluster perform their own rate limiting
    through an object specific to that.
    */
    virtual void Rate(uint64_t rate);

    /**
    Pure virtual method - handles forwarding a batch of messages to a specific
    receiver

    @param msg list of buffers to send
    @param sz list of the size of each buffer
    @param count number of buffers to send
    @return number of messages forwarded
    */
    virtual int Forward(zmq::message_t & zmqMessage) = 0;

    /**
    Runs housekeeping tasks for a specified egress port
    */
    virtual void Update(uint64_t delta);

    /**
    Administratively enables this link

    @return zero on success, or nonzero on failure
    */
    virtual int Enable();

    /**
    Administratively disables this link

    @return zero on success, or nonzero on failure
    */
    virtual int Disable();

    /**
    Checks to see if the port is currently available for use

    @return true if the port is available (ACTIVE & UP), and false otherwise
    */
    virtual bool Available();

    void Shutdown();

    virtual std::size_t GetTotalBundlesAcked() = 0;
    virtual std::size_t GetTotalBundlesSent() = 0;

protected:
    uint64_t m_label;
    uint64_t m_flags;
    //sockaddr_in m_ipv4;
};


class HegrUdpEntryAsync : public HegrEntryAsync {
public:
    HegrUdpEntryAsync();

    /**
    Initializes a new UDP forwarding entry
    */
    virtual void Init(uint64_t flags);

    /**
    Specifies an upper data rate for this link.

    If HARD_IFG is set, setting a target data rate will disable the use of
    sendmmsg() and related methods.  This will often yield reduced performance.

    @param rate Rate at which traffic may flow through this link
    */
    void Rate(uint64_t rate);

    /**
    Forwards a collection of UDP packets through this path and to a receiver

    @param msg List of messages to forward
    @param sz Size of each individual message
    @param count Total number of messages
    @return number of bytes forwarded on success, or an error code on failure
    */
    virtual int Forward(zmq::message_t & zmqMessage);

    /**
    Essentially a no-op for this entry type
    */
    void Update(uint64_t delta);

    /**
    Essentially a no-op for this entry type

    @return zero on success, and nonzero on failure
    */
    int Enable();

    /**
    Essentially a no-op for this entry type

    @return zero on success, and nonzero on failure
    */
    int Disable();

    void Shutdown();

    void Connect(const std::string & hostname, const std::string & port);
    UdpBundleSource * GetUdpBundleSourcePtr();

    virtual std::size_t GetTotalBundlesAcked();
    virtual std::size_t GetTotalBundlesSent();
private:
    std::unique_ptr<UdpBundleSource> m_udpBundleSourcePtr;
};


//tcpcl
class HegrTcpclEntryAsync : public HegrEntryAsync {
private:

public:
    HegrTcpclEntryAsync();
    //HegrTcpclEntryAsync(const boost::asio::ip::udp::endpoint & udpDestinationEndpoint, boost::asio::ip::udp::socket * const udpSocketPtr);
    virtual ~HegrTcpclEntryAsync();

    /**
    Initializes a new UDP forwarding entry
    */
    virtual void Init(uint64_t flags);

    /**
    Specifies an upper data rate for this link.

    If HARD_IFG is set, setting a target data rate will disable the use of
    sendmmsg() and related methods.  This will often yield reduced performance.

    @param rate Rate at which traffic may flow through this link
    */
    void Rate(uint64_t rate);

    /**
    Forwards a collection of UDP packets through this path and to a receiver

    @param msg List of messages to forward
    @param sz Size of each individual message
    @param count Total number of messages
    @return number of bytes forwarded on success, or an error code on failure
    */
    virtual int Forward(zmq::message_t & zmqMessage);

    /**
    Essentially a no-op for this entry type
    */
    void Update(uint64_t delta);

    /**
    Essentially a no-op for this entry type

    @return zero on success, and nonzero on failure
    */
    int Enable();

    /**
    Essentially a no-op for this entry type

    @return zero on success, and nonzero on failure
    */
    int Disable();

    void Shutdown();


    void Connect(const std::string & hostname, const std::string & port);
    TcpclBundleSource * GetTcpclBundleSourcePtr();

    virtual std::size_t GetTotalBundlesAcked();
    virtual std::size_t GetTotalBundlesSent();
private:
    boost::shared_ptr<TcpclBundleSource> m_tcpclBundleSourcePtr;

};

//stcp
class HegrStcpEntryAsync : public HegrEntryAsync {
private:

public:
    HegrStcpEntryAsync();
    //HegrTcpclEntryAsync(const boost::asio::ip::udp::endpoint & udpDestinationEndpoint, boost::asio::ip::udp::socket * const udpSocketPtr);
    virtual ~HegrStcpEntryAsync();

    /**
    Initializes a new UDP forwarding entry
    */
    virtual void Init(uint64_t flags);

    /**
    Specifies an upper data rate for this link.

    If HARD_IFG is set, setting a target data rate will disable the use of
    sendmmsg() and related methods.  This will often yield reduced performance.

    @param rate Rate at which traffic may flow through this link
    */
    void Rate(uint64_t rate);

    /**
    Forwards a collection of UDP packets through this path and to a receiver

    @param msg List of messages to forward
    @param sz Size of each individual message
    @param count Total number of messages
    @return number of bytes forwarded on success, or an error code on failure
    */
    virtual int Forward(zmq::message_t & zmqMessage);

    /**
    Essentially a no-op for this entry type
    */
    void Update(uint64_t delta);

    /**
    Essentially a no-op for this entry type

    @return zero on success, and nonzero on failure
    */
    int Enable();

    /**
    Essentially a no-op for this entry type

    @return zero on success, and nonzero on failure
    */
    int Disable();

    void Shutdown();


    void Connect(const std::string & hostname, const std::string & port);

    StcpBundleSource * GetStcpBundleSourcePtr();

    virtual std::size_t GetTotalBundlesAcked();
    virtual std::size_t GetTotalBundlesSent();
private:
    boost::shared_ptr<StcpBundleSource> m_stcpBundleSourcePtr;

};

class HegrManagerAsync {
public:
    HegrManagerAsync();
    ~HegrManagerAsync();
    void Stop();

    /**

    */
    void Init();

    /**

    */
    int Forward(int fec, zmq::message_t & zmqMessage);

    /**

    */
    int Add(int fec, uint64_t flags, const char *dst, int port, uint64_t rateBitsPerSec = 0);

    /**

    */
    int Remove(int fec);

    /**

    */
    void Up(int fec);

    /**

    */
    void Down(int fec);

    uint64_t m_bundleCount;
    uint64_t m_bundleData;
    uint64_t m_messageCount;

    bool m_testStorage = false;
    boost::shared_ptr<zmq::context_t> m_zmqCtx_ingressEgressPtr;
    boost::shared_ptr<zmq::socket_t> m_zmqPullSock_boundIngressToConnectingEgressPtr;
    boost::shared_ptr<zmq::socket_t> m_zmqPushSock_connectingEgressToBoundIngressPtr;
    boost::shared_ptr<zmq::context_t> m_zmqCtx_storageEgressPtr;
    boost::shared_ptr<zmq::socket_t> m_zmqPullSock_connectingStorageToBoundEgressPtr;
    boost::shared_ptr<zmq::socket_t> m_zmqPushSock_boundEgressToConnectingStoragePtr;

private:
    void ReadZmqThreadFunc();
    void OnSuccessfulBundleAck();
    void ProcessZmqMessagesThreadFunc(
        CircularIndexBufferSingleProducerSingleConsumerConfigurable & cb,
        std::vector<hdtn::BlockHdr> & headerMessages,
        std::vector<bool> & isFromStorage,
        std::vector<zmq::message_t> & payloadMessages);
    std::map<unsigned int, boost::shared_ptr<HegrEntryAsync> > m_entryMap;
    //HegrEntryAsync *Entry(int offset);
    //void *m_entries;
    boost::asio::io_service m_ioService;
    boost::asio::ip::udp::socket m_udpSocket;
    boost::asio::io_service::work m_work; //keep ioservice::run from exiting when no work to do
    boost::condition_variable m_conditionVariableProcessZmqMessages;


    boost::shared_ptr<boost::thread> m_threadZmqReaderPtr;
    boost::shared_ptr<boost::thread> m_ioServiceThreadPtr;
    volatile bool m_running;
};

}  // namespace hdtn

#endif

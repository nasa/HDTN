#ifndef TCPCL_H
#define TCPCL_H 1

#include <string>
#include <boost/shared_ptr.hpp>
#include <boost/integer.hpp>
#include <boost/function.hpp>
#include <vector>

#define TCPCL_VERSION 3

enum class TCPCL_MAIN_RX_STATE
{
    READ_CONTACT_HEADER = 0,
    READ_MESSAGE_TYPE_BYTE,
    READ_DATA_SEGMENT,
    READ_ACK_SEGMENT,
    READ_LENGTH_SEGMENT,
    READ_SHUTDOWN_SEGMENT_REASON_CODE,
    READ_SHUTDOWN_SEGMENT_RECONNECTION_DELAY_SDNV
};

enum class TCPCL_CONTACT_HEADER_RX_STATE
{
    READ_SYNC_1 = 0,
    READ_SYNC_2,
    READ_SYNC_3,
    READ_SYNC_4,
    READ_VERSION,
    READ_FLAGS,
    READ_KEEPALIVE_INTERVAL_BYTE1,
    READ_KEEPALIVE_INTERVAL_BYTE2,
    READ_LOCAL_EID_LENGTH_SDNV,
    READ_LOCAL_EID_STRING
};

enum class TCPCL_DATA_SEGMENT_RX_STATE
{
    READ_CONTENT_LENGTH_SDNV = 0,
    READ_CONTENTS
};

enum class MESSAGE_TYPE_BYTE_CODES
{
    RESERVED = 0x0,
    DATA_SEGMENT = 0x1,
    ACK_SEGMENT = 0x2,
    REFUSE_BUNDLE = 0x3,
    KEEPALIVE = 0x4,
    SHUTDOWN = 0x5,
    LENGTH = 0x6
};

enum class SHUTDOWN_REASON_CODES
{
    IDLE_TIMEOUT = 0x0,
    VERSION_MISMATCH = 0x1,
    BUSY = 0x2,
    UNASSIGNED
};

enum class BUNDLE_REFUSAL_CODES
{
    REFUSAL_REASON_UNKNOWN = 0x0, //REASON_UNKNOW taken in windows
    RECEIVER_HAS_COMPLETE_BUNDLE = 0x1,
    RECEIVER_RESOURCES_EXHAUSTED = 0x2,
    RECEIVER_PROBLEM__PLEASE_RETRANSMIT = 0x3,
    UNASSIGNED
};

enum class CONTACT_HEADER_FLAGS
{
    REQUEST_ACK_OF_BUNDLE_SEGMENTS = (1U << 0),
    REQUEST_ENABLING_OF_REACTIVE_FRAGMENTATION = (1U << 1),
    SUPPORT_BUNDLE_REFUSAL = (1U << 2),
    REQUEST_SENDING_OF_LENGTH_MESSAGES = (1U << 3)
};


class Tcpcl {


public:
    typedef boost::function<void(std::vector<uint8_t> & dataSegmentDataVec, bool isStartFlag, bool isEndFlag)> DataSegmentContentsReadCallback_t;
    typedef boost::function<void(CONTACT_HEADER_FLAGS flags, uint16_t keepAliveIntervalSeconds, const std::string & localEid)> ContactHeaderReadCallback_t;
    typedef boost::function<void(uint64_t totalBytesAcknowledged)> AckSegmentReadCallback_t;
    typedef boost::function<void(BUNDLE_REFUSAL_CODES refusalCode)> BundleRefusalCallback_t;
    typedef boost::function<void(uint64_t nextBundleLength)> NextBundleLengthCallback_t;
    typedef boost::function<void()> KeepAliveCallback_t;
    typedef boost::function<void(bool hasReasonCode, SHUTDOWN_REASON_CODES shutdownReasonCode,
        bool hasReconnectionDelay, uint64_t reconnectionDelaySeconds)> ShutdownMessageCallback_t;

    Tcpcl();
    ~Tcpcl();
    void SetDataSegmentContentsReadCallback(DataSegmentContentsReadCallback_t callback);
    void SetContactHeaderReadCallback(ContactHeaderReadCallback_t callback);
    void SetAckSegmentReadCallback(AckSegmentReadCallback_t callback);
    void SetBundleRefusalCallback(BundleRefusalCallback_t callback);
    void SetNextBundleLengthCallback(NextBundleLengthCallback_t callback);
    void SetKeepAliveCallback(KeepAliveCallback_t callback);
    void SetShutdownMessageCallback(ShutdownMessageCallback_t callback);
    void SetMaxReceiveBundleSizeBytes(const uint64_t maxRxBundleSizeBytes);

    void InitRx();
    void HandleReceivedChars(const uint8_t * rxVals, std::size_t numChars);
    void HandleReceivedChar(const uint8_t rxVal);
    static void GenerateContactHeader(std::vector<uint8_t> & hdr, CONTACT_HEADER_FLAGS flags, uint16_t keepAliveIntervalSeconds, const std::string & localEid);
    static void GenerateDataSegment(std::vector<uint8_t> & dataSegment, bool isStartSegment, bool isEndSegment, const uint8_t * contents, uint64_t sizeContents);
    static void GenerateDataSegmentHeaderOnly(std::vector<uint8_t> & dataSegmentHeaderDataVec, bool isStartSegment, bool isEndSegment, uint64_t sizeContents);
    static void GenerateAckSegment(std::vector<uint8_t> & ackSegment, uint64_t totalBytesAcknowledged);
    static void GenerateBundleRefusal(std::vector<uint8_t> & refusalMessage, BUNDLE_REFUSAL_CODES refusalCode);
    static void GenerateBundleLength(std::vector<uint8_t> & bundleLengthMessage, uint64_t nextBundleLength);
    static void GenerateKeepAliveMessage(std::vector<uint8_t> & keepAliveMessage);
    static void GenerateShutdownMessage(std::vector<uint8_t> & shutdownMessage,
        bool includeReasonCode, SHUTDOWN_REASON_CODES shutdownReasonCode,
        bool includeReconnectionDelay, uint64_t reconnectionDelaySeconds);
public:
    uint64_t M_MAX_RX_BUNDLE_SIZE_BYTES;
    std::vector<uint8_t> m_sdnvTempVec;
    TCPCL_MAIN_RX_STATE m_mainRxState;
    TCPCL_CONTACT_HEADER_RX_STATE m_contactHeaderRxState;
    TCPCL_DATA_SEGMENT_RX_STATE m_dataSegmentRxState;

    //contact header
    CONTACT_HEADER_FLAGS m_contactHeaderFlags;
    uint16_t m_keepAliveInterval;
    uint64_t m_localEidLength;
    std::string m_localEidStr;
    MESSAGE_TYPE_BYTE_CODES m_messageTypeByte;


    uint8_t m_messageTypeFlags;

    //data segment
    bool m_dataSegmentStartFlag;
    bool m_dataSegmentEndFlag;
    uint64_t m_dataSegmentLength;
    std::vector<uint8_t> m_dataSegmentDataVec;

    //ack segment
    uint64_t m_ackSegmentLength;

    //refuse bundle
    uint8_t m_bundleRefusalCode;

    //next bundle length
    uint64_t m_nextBundleLength;

    //shutdown
    bool m_shutdownHasReasonFlag;
    bool m_shutdownHasReconnectionDelayFlag;
    uint64_t m_shutdownReconnectionDelay;
    SHUTDOWN_REASON_CODES m_shutdownReasonCode;

    //callback functions
    ContactHeaderReadCallback_t m_contactHeaderReadCallback;
    DataSegmentContentsReadCallback_t m_dataSegmentContentsReadCallback;
    AckSegmentReadCallback_t m_ackSegmentReadCallback;
    BundleRefusalCallback_t m_bundleRefusalCallback;
    NextBundleLengthCallback_t m_nextBundleLengthCallback;
    KeepAliveCallback_t m_keepAliveCallback;
    ShutdownMessageCallback_t m_shutdownMessageCallback;
};

#endif // TCPCL_H


#ifndef LTP_H
#define LTP_H 1

#include <string>
#include <boost/shared_ptr.hpp>
#include <boost/integer.hpp>
#include <boost/function.hpp>
#include <vector>

enum class LTP_MAIN_RX_STATE
{
	READ_HEADER = 0,
	READ_DATA_SEGMENT_CONTENT,
    READ_REPORT_SEGMENT_CONTENT,
    READ_REPORT_ACKNOWLEDGEMENT_SEGMENT_CONTENT,
    READ_CANCEL_SEGMENT_CONTENT_BYTE,
	READ_TRAILER
};

enum class LTP_HEADER_RX_STATE
{
	READ_CONTROL_BYTE = 0,
	READ_SESSION_ORIGINATOR_ENGINE_ID_SDNV,
    READ_SESSION_NUMBER_SDNV,
	READ_NUM_EXTENSIONS_BYTE,
	READ_ONE_HEADER_EXTENSION_TAG_BYTE,
    READ_ONE_HEADER_EXTENSION_LENGTH_SDNV,
    READ_ONE_HEADER_EXTENSION_VALUE
};

enum class LTP_TRAILER_RX_STATE
{
    READ_ONE_TRAILER_EXTENSION_TAG_BYTE = 0,
    READ_ONE_TRAILER_EXTENSION_LENGTH_SDNV,
    READ_ONE_TRAILER_EXTENSION_VALUE
};

enum class LTP_DATA_SEGMENT_RX_STATE
{
	READ_CLIENT_SERVICE_ID_SDNV = 0,
    READ_OFFSET_SDNV,
    READ_LENGTH_SDNV,
    READ_CHECKPOINT_SERIAL_NUMBER_SDNV,
    READ_REPORT_SERIAL_NUMBER_SDNV,
	READ_CLIENT_SERVICE_DATA
};

enum class LTP_REPORT_SEGMENT_RX_STATE
{
    READ_REPORT_SERIAL_NUMBER_SDNV = 0,
    READ_CHECKPOINT_SERIAL_NUMBER_SDNV,
    READ_UPPER_BOUND_SDNV,
    READ_LOWER_BOUND_SDNV,
    READ_RECEPTION_CLAIM_COUNT_SDNV,
    READ_ONE_RECEPTION_CLAIM_OFFSET_SDNV,
    READ_ONE_RECEPTION_CLAIM_LENGTH_SDNV
};
enum class LTP_REPORT_ACKNOWLEDGEMENT_SEGMENT_RX_STATE
{
    READ_REPORT_SERIAL_NUMBER_SDNV = 0
};
/*
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
*/

class Ltp {


public:
    /*
	typedef boost::function<void(std::vector<uint8_t> & dataSegmentDataVec, bool isStartFlag, bool isEndFlag)> DataSegmentContentsReadCallback_t;
	typedef boost::function<void(CONTACT_HEADER_FLAGS flags, uint16_t keepAliveIntervalSeconds, const std::string & localEid)> ContactHeaderReadCallback_t;
	typedef boost::function<void(uint64_t totalBytesAcknowledged)> AckSegmentReadCallback_t;
	typedef boost::function<void(BUNDLE_REFUSAL_CODES refusalCode)> BundleRefusalCallback_t;
	typedef boost::function<void(uint64_t nextBundleLength)> NextBundleLengthCallback_t;
	typedef boost::function<void()> KeepAliveCallback_t;
	typedef boost::function<void(bool hasReasonCode, SHUTDOWN_REASON_CODES shutdownReasonCode,
								 bool hasReconnectionDelay, uint64_t reconnectionDelaySeconds)> ShutdownMessageCallback_t;
*/
    Ltp();
	~Ltp();
    /*
	void SetDataSegmentContentsReadCallback(DataSegmentContentsReadCallback_t callback);
	void SetContactHeaderReadCallback(ContactHeaderReadCallback_t callback);
	void SetAckSegmentReadCallback(AckSegmentReadCallback_t callback);
	void SetBundleRefusalCallback(BundleRefusalCallback_t callback);
	void SetNextBundleLengthCallback(NextBundleLengthCallback_t callback);
	void SetKeepAliveCallback(KeepAliveCallback_t callback);
	void SetShutdownMessageCallback(ShutdownMessageCallback_t callback);
    */

	void InitRx();
	void HandleReceivedChars(const uint8_t * rxVals, std::size_t numChars);
	void HandleReceivedChar(const uint8_t rxVal);
    /*
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
                                        */
public:
	std::vector<uint8_t> m_sdnvTempVec;
    LTP_MAIN_RX_STATE m_mainRxState;
    LTP_HEADER_RX_STATE m_headerRxState;
    LTP_TRAILER_RX_STATE m_trailerRxState;
    LTP_DATA_SEGMENT_RX_STATE m_dataSegmentRxState;
    LTP_REPORT_SEGMENT_RX_STATE m_reportSegmentRxState;
    //LTP_REPORT_ACKNOWLEDGEMENT_SEGMENT_RX_STATE m_reportAcknowledgementSegmentRxState; //one state only so not needed

    uint8_t m_segmentTypeFlags;
    uint64_t m_sessionOriginatorEngineId;
    uint64_t m_sessionNumber;
    uint8_t m_numHeaderExtensionTlvs;
    uint8_t m_numTrailerExtensionTlvs;

    uint64_t m_dataSegment_clientServiceId;
    uint64_t m_dataSegment_offset;
    uint64_t m_dataSegment_length;
    uint64_t m_dataSegment_checkpointSerialNumber;
    uint64_t m_dataSegment_reportSerialNumber;
    std::vector<uint8_t> m_dataSegment_clientServiceData;
    
    uint64_t m_reportSegment_reportSerialNumber;
    uint64_t m_reportSegment_checkpointSerialNumber;
    uint64_t m_reportSegment_upperBound;
    uint64_t m_reportSegment_lowerBound;
    uint64_t m_reportSegment_receptionClaimCount;
    struct reception_claim_t {
        uint64_t offset;
        uint64_t length;
    };
    std::vector<reception_claim_t> m_reportSegment_receptionClaims;

    uint64_t m_reportAcknowledgementSegment_reportSerialNumber;

    uint8_t m_cancelSegment_reasonCode;
        /*
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
    */
};

#endif // LTP_H


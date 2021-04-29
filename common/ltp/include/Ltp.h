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

enum class LTP_SEGMENT_TYPE_FLAGS
{
    REDDATA = 0x00,
    REDDATA_CHECKPOINT = 0x01,
    REDDATA_CHECKPOINT_ENDOFREDPART = 0x02,
    REDDATA_CHECKPOINT_ENDOFREDPART_ENDOFBLOCK = 0x03,
    GREENDATA = 0x04,
    GREENDATA_ENDOFBLOCK = 0x07,
    REPORT_SEGMENT = 0x08,
    REPORT_ACK_SEGMENT = 0x09,
    CANCEL_SEGMENT_FROM_BLOCK_SENDER = 12,
    CANCEL_ACK_SEGMENT_TO_BLOCK_SENDER = 13,
    CANCEL_SEGMENT_FROM_BLOCK_RECEIVER = 14,
    CANCEL_ACK_SEGMENT_TO_BLOCK_RECEIVER = 15,

};
enum class LTP_DATA_SEGMENT_TYPE_FLAGS //A SUBSET OF THE ABOVE FOR PARAMETER TO GENERATE DATA SEGMENTS
{
    REDDATA = 0x00,
    REDDATA_CHECKPOINT = 0x01,
    REDDATA_CHECKPOINT_ENDOFREDPART = 0x02,
    REDDATA_CHECKPOINT_ENDOFREDPART_ENDOFBLOCK = 0x03,
    GREENDATA = 0x04,
    GREENDATA_ENDOFBLOCK = 0x07
};
/*
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
    struct reception_claim_t {
        uint64_t offset;
        uint64_t length;

        reception_claim_t(); //a default constructor: X()
        ~reception_claim_t(); //a destructor: ~X()
        reception_claim_t(const reception_claim_t& o); //a copy constructor: X(const X&)
        reception_claim_t(reception_claim_t&& o); //a move constructor: X(X&&)
        reception_claim_t& operator=(const reception_claim_t& o); //a copy assignment: operator=(const X&)
        reception_claim_t& operator=(reception_claim_t&& o); //a move assignment: operator=(X&&)
        bool operator==(const reception_claim_t & o) const; //operator ==
        bool operator!=(const reception_claim_t & o) const; //operator !=
        uint64_t Serialize(uint8_t * serialization) const;
    };
    struct report_segment_t {
        uint64_t reportSerialNumber;
        uint64_t checkpointSerialNumber;
        uint64_t upperBound;
        uint64_t lowerBound;
        uint64_t receptionClaimCount;
        std::vector<reception_claim_t> receptionClaims;

        report_segment_t(); //a default constructor: X()
        ~report_segment_t(); //a destructor: ~X()
        report_segment_t(const report_segment_t& o); //a copy constructor: X(const X&)
        report_segment_t(report_segment_t&& o); //a move constructor: X(X&&)
        report_segment_t& operator=(const report_segment_t& o); //a copy assignment: operator=(const X&)
        report_segment_t& operator=(report_segment_t&& o); //a move assignment: operator=(X&&)
        bool operator==(const report_segment_t & o) const; //operator ==
        bool operator!=(const report_segment_t & o) const; //operator !=
        uint64_t Serialize(uint8_t * serialization) const;
        uint64_t GetMaximumDataRequiredForSerialization() const;
    };
    struct ltp_extension_t {
        uint8_t tag;
        //uint64_t length; //shall be stored in valueVec.size()
        std::vector<uint8_t> valueVec;

        ltp_extension_t(); //a default constructor: X()
        ~ltp_extension_t(); //a destructor: ~X()
        ltp_extension_t(const ltp_extension_t& o); //a copy constructor: X(const X&)
        ltp_extension_t(ltp_extension_t&& o); //a move constructor: X(X&&)
        ltp_extension_t& operator=(const ltp_extension_t& o); //a copy assignment: operator=(const X&)
        ltp_extension_t& operator=(ltp_extension_t&& o); //a move assignment: operator=(X&&)
        bool operator==(const ltp_extension_t & o) const; //operator ==
        bool operator!=(const ltp_extension_t & o) const; //operator !=
        void AppendSerialize(std::vector<uint8_t> & serialization) const;
        uint64_t Serialize(uint8_t * serialization) const;
    };
    struct ltp_extensions_t {
        std::vector<ltp_extension_t> extensionsVec;

        ltp_extensions_t(); //a default constructor: X()
        ~ltp_extensions_t(); //a destructor: ~X()
        ltp_extensions_t(const ltp_extensions_t& o); //a copy constructor: X(const X&)
        ltp_extensions_t(ltp_extensions_t&& o); //a move constructor: X(X&&)
        ltp_extensions_t& operator=(const ltp_extensions_t& o); //a copy assignment: operator=(const X&)
        ltp_extensions_t& operator=(ltp_extensions_t&& o); //a move assignment: operator=(X&&)
        bool operator==(const ltp_extensions_t & o) const; //operator ==
        bool operator!=(const ltp_extensions_t & o) const; //operator !=
        void AppendSerialize(std::vector<uint8_t> & serialization) const;
        uint64_t Serialize(uint8_t * serialization) const;
        uint64_t GetMaximumDataRequiredForSerialization() const;
    };
    //struct ltp_header_and_trailer_extensions_t {
    //    ltp_extensions_t headerExtensions;
    //    ltp_extensions_t trailerExtensions;
    //};
    struct data_segment_metadata_t {
        data_segment_metadata_t();
        data_segment_metadata_t(uint64_t paramClientServiceId, uint64_t paramOffset, uint64_t paramLength, uint64_t * paramCheckpointSerialNumber = NULL, uint64_t * paramReportSerialNumber = NULL);
        bool operator==(const data_segment_metadata_t & o) const; //operator ==
        bool operator!=(const data_segment_metadata_t & o) const; //operator !=
        uint64_t Serialize(uint8_t * serialization) const;
        uint64_t GetMaximumDataRequiredForSerialization() const;
        
        uint64_t clientServiceId;
        uint64_t offset;
        uint64_t length;
        uint64_t * checkpointSerialNumber;
        uint64_t * reportSerialNumber;
    };
    
    
	typedef boost::function<void(uint8_t segmentTypeFlags, uint64_t sessionOriginatorEngineId, uint64_t sessionNumber,
        std::vector<uint8_t> & clientServiceDataVec, const data_segment_metadata_t & dataSegmentMetadata,
        Ltp::ltp_extensions_t & headerExtensions, Ltp::ltp_extensions_t & trailerExtensions)> DataSegmentContentsReadCallback_t;
    typedef boost::function<void(uint64_t sessionOriginatorEngineId, uint64_t sessionNumber, const report_segment_t & reportSegment,
        Ltp::ltp_extensions_t & headerExtensions, Ltp::ltp_extensions_t & trailerExtensions)> ReportSegmentContentsReadCallback_t;
	
    Ltp();
	~Ltp();
    
	void SetDataSegmentContentsReadCallback(const DataSegmentContentsReadCallback_t & callback);
    void SetReportSegmentContentsReadCallback(const ReportSegmentContentsReadCallback_t & callback);


	void InitRx();
	bool HandleReceivedChars(const uint8_t * rxVals, std::size_t numChars, std::string & errorMessage);
	void HandleReceivedChar(const uint8_t rxVal, std::string & errorMessage);
    bool IsAtBeginningState() const; //unit testing convenience function
    
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
    static void GenerateReportAcknowledgementSegment(std::vector<uint8_t> & reportAckSegment, uint64_t sessionOriginatorEngineId, uint64_t sessionNumber, uint64_t reportSerialNumber);
    static void GenerateLtpHeaderPlusDataSegmentMetadata(std::vector<uint8_t> & ltpHeaderPlusDataSegmentMetadata, LTP_DATA_SEGMENT_TYPE_FLAGS dataSegmentTypeFlags, uint64_t sessionOriginatorEngineId,
        uint64_t sessionNumber, const data_segment_metadata_t & dataSegmentMetadata,
        ltp_extensions_t * headerExtensions = NULL, uint8_t numTrailerExtensions = 0);
    static void GenerateReportSegmentLtpPacket(std::vector<uint8_t> & ltpReportSegmentPacket, uint64_t sessionOriginatorEngineId,
        uint64_t sessionNumber, const report_segment_t & reportSegmentStruct,
        ltp_extensions_t * headerExtensions = NULL, ltp_extensions_t * trailerExtensions = NULL);
    //static void GenerateDataSegmentWithoutCheckpoint(std::vector<uint8_t> & dataSegment, LTP_DATA_SEGMENT_TYPE_FLAGS dataSegmentType, uint64_t sessionOriginatorEngineId,
    //    uint64_t sessionNumber, uint64_t clientServiceId, uint64_t offsetBytes, uint64_t lengthBytes);

private:
    void SetBeginningState();
    bool NextStateAfterHeaderExtensions(std::string & errorMessage);
    bool NextStateAfterTrailerExtensions(std::string & errorMessage);
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
    ltp_extensions_t m_headerExtensions;
    ltp_extensions_t m_trailerExtensions;
    uint64_t m_currentHeaderExtensionLength;
    uint64_t m_currentTrailerExtensionLength;

    data_segment_metadata_t m_dataSegmentMetadata;
    std::vector<uint8_t> m_dataSegment_clientServiceData;
    uint64_t m_dataSegment_checkpointSerialNumber;
    uint64_t m_dataSegment_reportSerialNumber;
    
    report_segment_t m_reportSegment;

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
*/
	//callback functions
	DataSegmentContentsReadCallback_t m_dataSegmentContentsReadCallback;
    ReportSegmentContentsReadCallback_t m_reportSegmentContentsReadCallback;
    
};

#endif // LTP_H


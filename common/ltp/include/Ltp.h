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

enum class CANCEL_SEGMENT_REASON_CODES
{
	USER_CANCELLED = 0x0, //Client service canceled session.
	UNREACHABLE = 0x1, //Unreachable client service.
	RLEXC = 0x2, //Retransmission limit exceeded.
    MISCOLORED = 0x3, //Received either a red-part data segment at block offset above any green - part data segment offset or a green - part data segment at block offset below any red - part data segment offset..
    SYSTEM_CANCELLED = 0x4, //A system error condition caused unexpected session termination.
    RXMTCYCEXC = 0x5, //Exceeded the Retransmission-Cycles limit.
	RESERVED
};



class Ltp {


public:
    struct reception_claim_t {
        uint64_t offset;
        uint64_t length;

        reception_claim_t(); //a default constructor: X()
        reception_claim_t(uint64_t paramOffset, uint64_t paramLength);
        ~reception_claim_t(); //a destructor: ~X()
        reception_claim_t(const reception_claim_t& o); //a copy constructor: X(const X&)
        reception_claim_t(reception_claim_t&& o); //a move constructor: X(X&&)
        reception_claim_t& operator=(const reception_claim_t& o); //a copy assignment: operator=(const X&)
        reception_claim_t& operator=(reception_claim_t&& o); //a move assignment: operator=(X&&)
        bool operator==(const reception_claim_t & o) const; //operator ==
        bool operator!=(const reception_claim_t & o) const; //operator !=
        friend std::ostream& operator<<(std::ostream& os, const reception_claim_t& o);
        uint64_t Serialize(uint8_t * serialization) const;
    };
    struct report_segment_t {
        uint64_t reportSerialNumber;
        uint64_t checkpointSerialNumber;
        uint64_t upperBound;
        uint64_t lowerBound;
        std::vector<reception_claim_t> receptionClaims;

        report_segment_t(); //a default constructor: X()
        report_segment_t(uint64_t paramReportSerialNumber, uint64_t paramCheckpointSerialNumber, uint64_t paramUpperBound, uint64_t paramLowerBound, const std::vector<reception_claim_t> & paramReceptionClaims);
        report_segment_t(uint64_t paramReportSerialNumber, uint64_t paramCheckpointSerialNumber, uint64_t paramUpperBound, uint64_t paramLowerBound, std::vector<reception_claim_t> && paramReceptionClaims);
        ~report_segment_t(); //a destructor: ~X()
        report_segment_t(const report_segment_t& o); //a copy constructor: X(const X&)
        report_segment_t(report_segment_t&& o); //a move constructor: X(X&&)
        report_segment_t& operator=(const report_segment_t& o); //a copy assignment: operator=(const X&)
        report_segment_t& operator=(report_segment_t&& o); //a move assignment: operator=(X&&)
        bool operator==(const report_segment_t & o) const; //operator ==
        bool operator!=(const report_segment_t & o) const; //operator !=
        friend std::ostream& operator<<(std::ostream& os, const report_segment_t& dt);
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
    typedef boost::function<void(uint64_t sessionOriginatorEngineId, uint64_t sessionNumber, uint64_t reportSerialNumberBeingAcknowledged,
        Ltp::ltp_extensions_t & headerExtensions, Ltp::ltp_extensions_t & trailerExtensions)> ReportAcknowledgementSegmentContentsReadCallback_t;
    typedef boost::function<void(uint64_t sessionOriginatorEngineId, uint64_t sessionNumber, CANCEL_SEGMENT_REASON_CODES reasonCode, bool isFromSender,
        Ltp::ltp_extensions_t & headerExtensions, Ltp::ltp_extensions_t & trailerExtensions)> CancelSegmentContentsReadCallback_t;
    typedef boost::function<void(uint64_t sessionOriginatorEngineId, uint64_t sessionNumber, bool isToSender,
        Ltp::ltp_extensions_t & headerExtensions, Ltp::ltp_extensions_t & trailerExtensions)> CancelAcknowledgementSegmentContentsReadCallback_t;
	
    Ltp();
	~Ltp();
    
	void SetDataSegmentContentsReadCallback(const DataSegmentContentsReadCallback_t & callback);
    void SetReportSegmentContentsReadCallback(const ReportSegmentContentsReadCallback_t & callback);
    void SetReportAcknowledgementSegmentContentsReadCallback(const ReportAcknowledgementSegmentContentsReadCallback_t & callback);
    void SetCancelSegmentContentsReadCallback(const CancelSegmentContentsReadCallback_t & callback);
    void SetCancelAcknowledgementSegmentContentsReadCallback(const CancelAcknowledgementSegmentContentsReadCallback_t & callback);


	void InitRx();
	bool HandleReceivedChars(const uint8_t * rxVals, std::size_t numChars, std::string & errorMessage);
	void HandleReceivedChar(const uint8_t rxVal, std::string & errorMessage);
    bool IsAtBeginningState() const; //unit testing convenience function
    
    
    static void GenerateReportAcknowledgementSegment(std::vector<uint8_t> & reportAckSegment, uint64_t sessionOriginatorEngineId, uint64_t sessionNumber, uint64_t reportSerialNumber);
    static void GenerateLtpHeaderPlusDataSegmentMetadata(std::vector<uint8_t> & ltpHeaderPlusDataSegmentMetadata, LTP_DATA_SEGMENT_TYPE_FLAGS dataSegmentTypeFlags, uint64_t sessionOriginatorEngineId,
        uint64_t sessionNumber, const data_segment_metadata_t & dataSegmentMetadata,
        ltp_extensions_t * headerExtensions = NULL, uint8_t numTrailerExtensions = 0);
    static void GenerateReportSegmentLtpPacket(std::vector<uint8_t> & ltpReportSegmentPacket, uint64_t sessionOriginatorEngineId,
        uint64_t sessionNumber, const report_segment_t & reportSegmentStruct,
        ltp_extensions_t * headerExtensions = NULL, ltp_extensions_t * trailerExtensions = NULL);
    static void GenerateReportAcknowledgementSegmentLtpPacket(std::vector<uint8_t> & ltpReportAcknowledgementSegmentPacket, uint64_t sessionOriginatorEngineId,
        uint64_t sessionNumber, uint64_t reportSerialNumberBeingAcknowledged,
        ltp_extensions_t * headerExtensions = NULL, ltp_extensions_t * trailerExtensions = NULL);
    static void GenerateCancelSegmentLtpPacket(std::vector<uint8_t> & ltpCancelSegmentPacket, uint64_t sessionOriginatorEngineId,
        uint64_t sessionNumber, CANCEL_SEGMENT_REASON_CODES reasonCode, bool isFromSender,
        ltp_extensions_t * headerExtensions = NULL, ltp_extensions_t * trailerExtensions = NULL);
    static void GenerateCancelAcknowledgementSegmentLtpPacket(std::vector<uint8_t> & ltpCancelAcknowledgementSegmentPacket, uint64_t sessionOriginatorEngineId,
        uint64_t sessionNumber, bool isToSender, ltp_extensions_t * headerExtensions = NULL, ltp_extensions_t * trailerExtensions = NULL);

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
    uint64_t m_reportSegment_receptionClaimCount;

    uint64_t m_reportAcknowledgementSegment_reportSerialNumber;

    uint8_t m_cancelSegment_reasonCode;
        
	//callback functions
	DataSegmentContentsReadCallback_t m_dataSegmentContentsReadCallback;
    ReportSegmentContentsReadCallback_t m_reportSegmentContentsReadCallback;
    ReportAcknowledgementSegmentContentsReadCallback_t m_reportAcknowledgementSegmentContentsReadCallback;
    CancelSegmentContentsReadCallback_t m_cancelSegmentContentsReadCallback;
    CancelAcknowledgementSegmentContentsReadCallback_t m_cancelAcknowledgementSegmentContentsReadCallback;
};

#endif // LTP_H


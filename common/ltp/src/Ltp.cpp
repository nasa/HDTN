#include "Ltp.h"
#include <boost/make_shared.hpp>
#include <boost/foreach.hpp>
#include <boost/endian/conversion.hpp>
#include <boost/lexical_cast.hpp>
#include <iostream>
#include "Sdnv.h"


Ltp::reception_claim_t::reception_claim_t() : offset(0), length(0) { } //a default constructor: X()
Ltp::reception_claim_t::~reception_claim_t() { } //a destructor: ~X()
Ltp::reception_claim_t::reception_claim_t(const reception_claim_t& o) : offset(o.offset), length(o.length) { } //a copy constructor: X(const X&)
Ltp::reception_claim_t::reception_claim_t(reception_claim_t&& o) : offset(o.offset), length(o.length) { } //a move constructor: X(X&&)
Ltp::reception_claim_t& Ltp::reception_claim_t::operator=(const reception_claim_t& o) { //a copy assignment: operator=(const X&)
    offset = o.offset;
    length = o.length;
    return *this;
}
Ltp::reception_claim_t& Ltp::reception_claim_t::operator=(reception_claim_t && o) { //a move assignment: operator=(X&&)
    offset = o.offset;
    length = o.length;
    return *this;
}
bool Ltp::reception_claim_t::operator==(const reception_claim_t & o) const {
    return (offset == o.offset) && (length == o.length);
}
bool Ltp::reception_claim_t::operator!=(const reception_claim_t & o) const {
    return (offset != o.offset) || (length != o.length);
}
uint64_t Ltp::reception_claim_t::Serialize(uint8_t * serialization) const {
    uint8_t * serializationBase = serialization;
    serialization += SdnvEncodeU64(serialization, offset);
    serialization += SdnvEncodeU64(serialization, length);
    return serialization - serializationBase;
}

Ltp::report_segment_t::report_segment_t() : reportSerialNumber(0), checkpointSerialNumber(0), upperBound(0), lowerBound(0), receptionClaimCount(0) { } //a default constructor: X()
Ltp::report_segment_t::~report_segment_t() { } //a destructor: ~X()
Ltp::report_segment_t::report_segment_t(const report_segment_t& o) : 
    reportSerialNumber(o.reportSerialNumber), checkpointSerialNumber(o.checkpointSerialNumber),
    upperBound(o.upperBound), lowerBound(o.lowerBound), receptionClaimCount(o.receptionClaimCount), receptionClaims(o.receptionClaims) { } //a copy constructor: X(const X&)
Ltp::report_segment_t::report_segment_t(report_segment_t&& o) : 
    reportSerialNumber(o.reportSerialNumber), checkpointSerialNumber(o.checkpointSerialNumber),
    upperBound(o.upperBound), lowerBound(o.lowerBound), receptionClaimCount(o.receptionClaimCount), receptionClaims(std::move(o.receptionClaims)) { } //a move constructor: X(X&&)
Ltp::report_segment_t& Ltp::report_segment_t::operator=(const report_segment_t& o) { //a copy assignment: operator=(const X&)
    reportSerialNumber = o.reportSerialNumber;
    checkpointSerialNumber = o.checkpointSerialNumber;
    upperBound = o.upperBound;
    lowerBound = o.lowerBound;
    receptionClaimCount = o.receptionClaimCount;
    receptionClaims = o.receptionClaims;
    return *this;
}
Ltp::report_segment_t& Ltp::report_segment_t::operator=(report_segment_t && o) { //a move assignment: operator=(X&&)
    reportSerialNumber = o.reportSerialNumber;
    checkpointSerialNumber = o.checkpointSerialNumber;
    upperBound = o.upperBound;
    lowerBound = o.lowerBound;
    receptionClaimCount = o.receptionClaimCount;
    receptionClaims = std::move(o.receptionClaims);
    return *this;
}
bool Ltp::report_segment_t::operator==(const report_segment_t & o) const {
    return (reportSerialNumber == o.reportSerialNumber) && (checkpointSerialNumber == o.checkpointSerialNumber)
        && (upperBound == o.upperBound) && (lowerBound == o.lowerBound)
        && (receptionClaimCount == o.receptionClaimCount) && (receptionClaims == o.receptionClaims);
}
bool Ltp::report_segment_t::operator!=(const report_segment_t & o) const {
    return (reportSerialNumber != o.reportSerialNumber) || (checkpointSerialNumber != o.checkpointSerialNumber)
        || (upperBound != o.upperBound) || (lowerBound != o.lowerBound)
        || (receptionClaimCount != o.receptionClaimCount) || (receptionClaims != o.receptionClaims);
}
uint64_t Ltp::report_segment_t::Serialize(uint8_t * serialization) const {
    uint8_t * serializationBase = serialization;
    serialization += SdnvEncodeU64(serialization, reportSerialNumber);
    serialization += SdnvEncodeU64(serialization, checkpointSerialNumber);
    serialization += SdnvEncodeU64(serialization, upperBound);
    serialization += SdnvEncodeU64(serialization, lowerBound);
    serialization += SdnvEncodeU64(serialization, receptionClaimCount);
    
    for (std::vector<reception_claim_t>::const_iterator it = receptionClaims.cbegin(); it != receptionClaims.cend(); ++it) {
        serialization += it->Serialize(serialization);
    }
    return serialization - serializationBase;
}
uint64_t Ltp::report_segment_t::GetMaximumDataRequiredForSerialization() const {
    return (5 * 10) + (receptionClaims.size() * (2 * 10)); //5 sdnvs * 10 bytes sdnv max + reception claims * 2sdnvs per claim
}

Ltp::ltp_extension_t::ltp_extension_t() : tag(0) { } //a default constructor: X()
Ltp::ltp_extension_t::~ltp_extension_t() { } //a destructor: ~X()
Ltp::ltp_extension_t::ltp_extension_t(const ltp_extension_t& o) : tag(o.tag), valueVec(o.valueVec) { } //a copy constructor: X(const X&)
Ltp::ltp_extension_t::ltp_extension_t(ltp_extension_t&& o) : tag(o.tag), valueVec(std::move(o.valueVec)) { } //a move constructor: X(X&&)
Ltp::ltp_extension_t& Ltp::ltp_extension_t::operator=(const ltp_extension_t& o) { //a copy assignment: operator=(const X&)
    tag = o.tag;
    valueVec = o.valueVec;
    return *this;
}
Ltp::ltp_extension_t& Ltp::ltp_extension_t::operator=(ltp_extension_t && o) { //a move assignment: operator=(X&&)
    tag = o.tag;
    valueVec = std::move(o.valueVec);
    return *this;
}
bool Ltp::ltp_extension_t::operator==(const ltp_extension_t & o) const {
    return (tag == o.tag) && (valueVec == o.valueVec);
}
bool Ltp::ltp_extension_t::operator!=(const ltp_extension_t & o) const {
    return (tag != o.tag) || (valueVec != o.valueVec);
}
void Ltp::ltp_extension_t::AppendSerialize(std::vector<uint8_t> & serialization) const {
    serialization.push_back(tag);

    //sdnv encode length (valueVec.size())
    const uint64_t originalSize = serialization.size();
    serialization.resize(originalSize + 20);
    const unsigned int outputSizeBytesLengthSdnv = SdnvEncodeU64(&serialization[originalSize], valueVec.size());
    serialization.resize(originalSize + outputSizeBytesLengthSdnv);

    //data copy valueVec
    serialization.insert(serialization.end(), valueVec.begin(), valueVec.end()); //concatenate
}
uint64_t Ltp::ltp_extension_t::Serialize(uint8_t * serialization) const {
    *serialization++ = tag;

    //sdnv encode length (valueVec.size())
    const uint64_t sdnvSizeBytes = SdnvEncodeU64(serialization, valueVec.size());
    serialization += sdnvSizeBytes;

    //data copy valueVec
    memcpy(serialization, valueVec.data(), valueVec.size()); //concatenate

    return 1 + sdnvSizeBytes + valueVec.size();
}

Ltp::ltp_extensions_t::ltp_extensions_t() { } //a default constructor: X()
Ltp::ltp_extensions_t::~ltp_extensions_t() { } //a destructor: ~X()
Ltp::ltp_extensions_t::ltp_extensions_t(const ltp_extensions_t& o) : extensionsVec(o.extensionsVec) { } //a copy constructor: X(const X&)
Ltp::ltp_extensions_t::ltp_extensions_t(ltp_extensions_t&& o) : extensionsVec(std::move(o.extensionsVec)) { } //a move constructor: X(X&&)
Ltp::ltp_extensions_t& Ltp::ltp_extensions_t::operator=(const ltp_extensions_t& o) { //a copy assignment: operator=(const X&)
    extensionsVec = o.extensionsVec;
    return *this;
}
Ltp::ltp_extensions_t& Ltp::ltp_extensions_t::operator=(ltp_extensions_t && o) { //a move assignment: operator=(X&&)
    extensionsVec = std::move(o.extensionsVec);
    return *this;
}
bool Ltp::ltp_extensions_t::operator==(const ltp_extensions_t & o) const {
    return (extensionsVec == o.extensionsVec);
}
bool Ltp::ltp_extensions_t::operator!=(const ltp_extensions_t & o) const {
    return (extensionsVec != o.extensionsVec);
}
void Ltp::ltp_extensions_t::AppendSerialize(std::vector<uint8_t> & serialization) const {
    for (std::vector<ltp_extension_t>::const_iterator it = extensionsVec.cbegin(); it != extensionsVec.cend(); ++it) {
        it->AppendSerialize(serialization);
    }
}
uint64_t Ltp::ltp_extensions_t::Serialize(uint8_t * serialization) const {
    uint8_t * serializationBase = serialization;
    for (std::vector<ltp_extension_t>::const_iterator it = extensionsVec.cbegin(); it != extensionsVec.cend(); ++it) {
        serialization += it->Serialize(serialization);
    }
    return serialization - serializationBase;
}
uint64_t Ltp::ltp_extensions_t::GetMaximumDataRequiredForSerialization() const {
    uint64_t maximumBytesRequired = extensionsVec.size() * 11; //tag plus 10 bytes sdnv max
    for (std::vector<ltp_extension_t>::const_iterator it = extensionsVec.cbegin(); it != extensionsVec.cend(); ++it) {
        maximumBytesRequired += it->valueVec.size();
    }
    return maximumBytesRequired;
}

Ltp::data_segment_metadata_t::data_segment_metadata_t() : clientServiceId(0), offset(0), length(0), checkpointSerialNumber(NULL), reportSerialNumber(NULL) {}
Ltp::data_segment_metadata_t::data_segment_metadata_t(uint64_t paramClientServiceId, uint64_t paramOffset, uint64_t paramLength, uint64_t * paramCheckpointSerialNumber, uint64_t * paramReportSerialNumber) :
    clientServiceId(paramClientServiceId),
    offset(paramOffset),
    length(paramLength),
    checkpointSerialNumber(paramCheckpointSerialNumber),
    reportSerialNumber(paramReportSerialNumber) {}
bool Ltp::data_segment_metadata_t::operator==(const data_segment_metadata_t & o) const {
    if (checkpointSerialNumber) {
        if (o.checkpointSerialNumber) {
            if (*checkpointSerialNumber != *o.checkpointSerialNumber) {
                return false;
            }
        }
        else { //o.checkpointSerialNumber == NULL
            return false;
        }
    }
    else { //checkpointSerialNumber == NULL
        if (o.checkpointSerialNumber != NULL) {
            return false;
        }
    }
    return (clientServiceId == o.clientServiceId) && (offset == o.offset) && (length == o.length);
}
bool Ltp::data_segment_metadata_t::operator!=(const data_segment_metadata_t & o) const {
    return !(*this == o);
}
uint64_t Ltp::data_segment_metadata_t::Serialize(uint8_t * serialization) const {
    uint8_t * serializationBase = serialization;
    serialization += SdnvEncodeU64(serialization, clientServiceId);
    serialization += SdnvEncodeU64(serialization, offset);
    serialization += SdnvEncodeU64(serialization, length); 
    if (checkpointSerialNumber && reportSerialNumber) {
        serialization += SdnvEncodeU64(serialization, *checkpointSerialNumber);
        serialization += SdnvEncodeU64(serialization, *reportSerialNumber);
    }
    return serialization - serializationBase;
}
uint64_t Ltp::data_segment_metadata_t::GetMaximumDataRequiredForSerialization() const {
    return (3 * 10) + ((static_cast<bool>(checkpointSerialNumber && reportSerialNumber)) * (static_cast<uint8_t>(2 * 10))); //5 sdnvs * 10 bytes sdnv max + reception claims * 2sdnvs per claim
}

Ltp::Ltp() {
    InitRx();
}
Ltp::~Ltp() {

}

void Ltp::SetDataSegmentContentsReadCallback(const DataSegmentContentsReadCallback_t & callback) {
    m_dataSegmentContentsReadCallback = callback;
}
void Ltp::SetReportSegmentContentsReadCallback(const ReportSegmentContentsReadCallback_t & callback) {
    m_reportSegmentContentsReadCallback = callback;
}
void Ltp::SetReportAcknowledgementSegmentContentsReadCallback(const ReportAcknowledgementSegmentContentsReadCallback_t & callback) {
    m_reportAcknowledgementSegmentContentsReadCallback = callback;
}

void Ltp::InitRx() {
    m_mainRxState = LTP_MAIN_RX_STATE::READ_HEADER;
    m_headerRxState = LTP_HEADER_RX_STATE::READ_CONTROL_BYTE;
    m_trailerRxState = LTP_TRAILER_RX_STATE::READ_ONE_TRAILER_EXTENSION_TAG_BYTE;
    m_dataSegmentRxState = LTP_DATA_SEGMENT_RX_STATE::READ_CLIENT_SERVICE_ID_SDNV;
    m_reportSegmentRxState = LTP_REPORT_SEGMENT_RX_STATE::READ_REPORT_SERIAL_NUMBER_SDNV;
    //m_reportAcknowledgementSegmentRxState = LTP_REPORT_ACKNOWLEDGEMENT_SEGMENT_RX_STATE::READ_REPORT_SERIAL_NUMBER_SDNV;
    m_sdnvTempVec.reserve(32); //critical for hardware accelerated decode (min size 10) to prevent out of bounds
    m_sdnvTempVec.clear();
}

bool Ltp::IsAtBeginningState() const { //unit testing convenience function
    return ((m_mainRxState == LTP_MAIN_RX_STATE::READ_HEADER) && (m_headerRxState == LTP_HEADER_RX_STATE::READ_CONTROL_BYTE));
}

void Ltp::SetBeginningState() {
    m_mainRxState = LTP_MAIN_RX_STATE::READ_HEADER;
    m_headerRxState = LTP_HEADER_RX_STATE::READ_CONTROL_BYTE;
}

void Ltp::HandleReceivedChar(const uint8_t rxVal, std::string & errorMessage) {
    HandleReceivedChars(&rxVal, 1, errorMessage);
}

bool Ltp::HandleReceivedChars(const uint8_t * rxVals, std::size_t numChars, std::string & errorMessage) {
    while (numChars) {
        --numChars;
        const uint8_t rxVal = *rxVals++;
        const LTP_MAIN_RX_STATE mainRxState = m_mainRxState; //const for optimization
        if (mainRxState == LTP_MAIN_RX_STATE::READ_HEADER) {
            const LTP_HEADER_RX_STATE headerRxState = m_headerRxState; //const for optimization
            if (headerRxState == LTP_HEADER_RX_STATE::READ_CONTROL_BYTE) {
                const uint8_t ltpVersion = rxVal >> 4;
                if (ltpVersion != 0) {
                    errorMessage = "error ltp version not 0.. got " + boost::lexical_cast<std::string>((int)ltpVersion);
                    return false;
                }
                else {
                    m_segmentTypeFlags = rxVal & 0x0f;
                    m_sdnvTempVec.clear();
                    m_headerRxState = LTP_HEADER_RX_STATE::READ_SESSION_ORIGINATOR_ENGINE_ID_SDNV;
                }
            }
            else if (headerRxState == LTP_HEADER_RX_STATE::READ_SESSION_ORIGINATOR_ENGINE_ID_SDNV) {
                m_sdnvTempVec.push_back(rxVal);
                if (m_sdnvTempVec.size() > 10) {
                    errorMessage = "error in LTP_HEADER_RX_STATE::READ_SESSION_ORIGINATOR_ENGINE_ID_SDNV, sdnv > 10 bytes";
                    return false;
                }
                else if ((rxVal & 0x80) == 0) { //if msbit is a 0 then stop
                    uint8_t sdnvSize;
                    m_sessionOriginatorEngineId = SdnvDecodeU64(m_sdnvTempVec.data(), &sdnvSize);
                    if (sdnvSize != m_sdnvTempVec.size()) {
                        errorMessage = "error in LTP_HEADER_RX_STATE::READ_SESSION_ORIGINATOR_ENGINE_ID_SDNV, sdnvSize != m_sdnvTempVec.size()";
                        return false;
                    }
                    else {
                        m_sdnvTempVec.clear();
                        m_headerRxState = LTP_HEADER_RX_STATE::READ_SESSION_NUMBER_SDNV;
                    }
                }
            }
            else if (headerRxState == LTP_HEADER_RX_STATE::READ_SESSION_NUMBER_SDNV) {
                m_sdnvTempVec.push_back(rxVal);
                if (m_sdnvTempVec.size() > 10) {
                    errorMessage = "error in LTP_HEADER_RX_STATE::READ_SESSION_NUMBER_SDNV, sdnv > 10 bytes";
                    return false;
                }
                else if ((rxVal & 0x80) == 0) { //if msbit is a 0 then stop
                    uint8_t sdnvSize;
                    m_sessionNumber = SdnvDecodeU64(m_sdnvTempVec.data(), &sdnvSize);
                    if (sdnvSize != m_sdnvTempVec.size()) {
                        errorMessage = "error in LTP_HEADER_RX_STATE::READ_SESSION_NUMBER_SDNV, sdnvSize != m_sdnvTempVec.size()";
                        return false;
                    }
                    else {
                        m_headerRxState = LTP_HEADER_RX_STATE::READ_NUM_EXTENSIONS_BYTE;
                    }
                }
            }
            else if (headerRxState == LTP_HEADER_RX_STATE::READ_NUM_EXTENSIONS_BYTE) { //msb
                m_numHeaderExtensionTlvs = rxVal >> 4;
                m_numTrailerExtensionTlvs = rxVal & 0x0f;
                m_headerExtensions.extensionsVec.clear();
                m_headerExtensions.extensionsVec.reserve(m_numHeaderExtensionTlvs);
                m_trailerExtensions.extensionsVec.clear();
                m_trailerExtensions.extensionsVec.reserve(m_numTrailerExtensionTlvs);
                if (m_numHeaderExtensionTlvs) {
                    m_headerRxState = LTP_HEADER_RX_STATE::READ_ONE_HEADER_EXTENSION_TAG_BYTE;
                }
                else {
                    if (!NextStateAfterHeaderExtensions(errorMessage)) return false;
                }
            }
            else if (headerRxState == LTP_HEADER_RX_STATE::READ_ONE_HEADER_EXTENSION_TAG_BYTE) {
#if (__cplusplus >= 201703L)
                ltp_extension_t & ext = m_headerExtensions.extensionsVec.emplace_back();
                ext.tag = rxVal;
#else
                m_headerExtensions.extensionsVec.emplace_back();
                m_headerExtensions.extensionsVec.back().tag = rxVal;
#endif
                m_sdnvTempVec.clear();
                m_headerRxState = LTP_HEADER_RX_STATE::READ_ONE_HEADER_EXTENSION_LENGTH_SDNV;
            }
            else if (headerRxState == LTP_HEADER_RX_STATE::READ_ONE_HEADER_EXTENSION_LENGTH_SDNV) {
                m_sdnvTempVec.push_back(rxVal);
                if (m_sdnvTempVec.size() > 10) {
                    errorMessage = "error in LTP_HEADER_RX_STATE::READ_ONE_HEADER_EXTENSION_LENGTH_SDNV, sdnv > 10 bytes";
                    return false;
                }
                else if ((rxVal & 0x80) == 0) { //if msbit is a 0 then stop
                    uint8_t sdnvSize;
                    m_currentHeaderExtensionLength = SdnvDecodeU64(m_sdnvTempVec.data(), &sdnvSize);
                    if (sdnvSize != m_sdnvTempVec.size()) {
                        errorMessage = "error in LTP_HEADER_RX_STATE::READ_ONE_HEADER_EXTENSION_LENGTH_SDNV, sdnvSize != m_sdnvTempVec.size()";
                        return false;
                    }
                    else if (m_currentHeaderExtensionLength == 0) {
                        if (m_headerExtensions.extensionsVec.size() == m_numHeaderExtensionTlvs) {
                            if(!NextStateAfterHeaderExtensions(errorMessage)) return false;
                        }
                        else {
                            m_headerRxState = LTP_HEADER_RX_STATE::READ_ONE_HEADER_EXTENSION_TAG_BYTE;
                        }
                    }
                    else {
                        m_headerExtensions.extensionsVec.back().valueVec.reserve(m_currentHeaderExtensionLength);
                        m_headerRxState = LTP_HEADER_RX_STATE::READ_ONE_HEADER_EXTENSION_VALUE;
                    }
                }
            }
            else if (headerRxState == LTP_HEADER_RX_STATE::READ_ONE_HEADER_EXTENSION_VALUE) {
                std::vector<uint8_t> & valueVec = m_headerExtensions.extensionsVec.back().valueVec;
                valueVec.push_back(rxVal);
                if (valueVec.size() == m_currentHeaderExtensionLength) {
                    if (m_headerExtensions.extensionsVec.size() == m_numHeaderExtensionTlvs) {
                        if (!NextStateAfterHeaderExtensions(errorMessage)) return false;
                    }
                    else {
                        m_headerRxState = LTP_HEADER_RX_STATE::READ_ONE_HEADER_EXTENSION_TAG_BYTE;
                    }
                }
            }
        }
        else if (mainRxState == LTP_MAIN_RX_STATE::READ_DATA_SEGMENT_CONTENT) {
            const LTP_DATA_SEGMENT_RX_STATE dataSegmentRxState = m_dataSegmentRxState; //const for optimization
            if (dataSegmentRxState == LTP_DATA_SEGMENT_RX_STATE::READ_CLIENT_SERVICE_ID_SDNV) {
                m_sdnvTempVec.push_back(rxVal);
                if (m_sdnvTempVec.size() > 10) {
                    errorMessage = "error in LTP_DATA_SEGMENT_RX_STATE::READ_CLIENT_SERVICE_ID_SDNV, sdnv > 10 bytes";
                    return false;
                }
                else if ((rxVal & 0x80) == 0) { //if msbit is a 0 then stop
                    uint8_t sdnvSize;
                    m_dataSegmentMetadata.clientServiceId = SdnvDecodeU64(m_sdnvTempVec.data(), &sdnvSize);
                    if (sdnvSize != m_sdnvTempVec.size()) {
                        errorMessage = "error in LTP_DATA_SEGMENT_RX_STATE::READ_CLIENT_SERVICE_ID_SDNV, sdnvSize != m_sdnvTempVec.size()";
                        return false;
                    }
                    else {
                        m_sdnvTempVec.clear();
                        m_dataSegmentRxState = LTP_DATA_SEGMENT_RX_STATE::READ_OFFSET_SDNV;
                    }
                }
            }
            else if (dataSegmentRxState == LTP_DATA_SEGMENT_RX_STATE::READ_OFFSET_SDNV) {
                m_sdnvTempVec.push_back(rxVal);
                if (m_sdnvTempVec.size() > 10) {
                    errorMessage = "error in LTP_DATA_SEGMENT_RX_STATE::READ_OFFSET_SDNV, sdnv > 10 bytes";
                    return false;
                }
                else if ((rxVal & 0x80) == 0) { //if msbit is a 0 then stop
                    uint8_t sdnvSize;
                    m_dataSegmentMetadata.offset = SdnvDecodeU64(m_sdnvTempVec.data(), &sdnvSize);
                    if (sdnvSize != m_sdnvTempVec.size()) {
                        errorMessage = "error in LTP_DATA_SEGMENT_RX_STATE::READ_OFFSET_SDNV, sdnvSize != m_sdnvTempVec.size()";
                        return false;
                    }
                    else {
                        m_sdnvTempVec.clear();
                        m_dataSegmentRxState = LTP_DATA_SEGMENT_RX_STATE::READ_LENGTH_SDNV;
                    }
                }
            }
            else if (dataSegmentRxState == LTP_DATA_SEGMENT_RX_STATE::READ_LENGTH_SDNV) {
                m_sdnvTempVec.push_back(rxVal);
                if (m_sdnvTempVec.size() > 10) {
                    errorMessage = "error in LTP_DATA_SEGMENT_RX_STATE::READ_LENGTH_SDNV, sdnv > 10 bytes";
                    return false;
                }
                else if ((rxVal & 0x80) == 0) { //if msbit is a 0 then stop
                    uint8_t sdnvSize;
                    m_dataSegmentMetadata.length = SdnvDecodeU64(m_sdnvTempVec.data(), &sdnvSize);
                    if (sdnvSize != m_sdnvTempVec.size()) {
                        errorMessage = "error in LTP_DATA_SEGMENT_RX_STATE::READ_LENGTH_SDNV, sdnvSize != m_sdnvTempVec.size()";
                        return false;
                    }
                    else if (m_dataSegmentMetadata.length == 0) { //not sure if this is correct
                        errorMessage = "error in LTP_DATA_SEGMENT_RX_STATE::READ_LENGTH_SDNV, length == 0";
                        return false;
                    }
                    else if ((m_segmentTypeFlags >= 1) && (m_segmentTypeFlags <= 3)) { //checkpoint
                        m_sdnvTempVec.clear();
                        m_dataSegmentRxState = LTP_DATA_SEGMENT_RX_STATE::READ_CHECKPOINT_SERIAL_NUMBER_SDNV;
                        m_dataSegmentMetadata.checkpointSerialNumber = &m_dataSegment_checkpointSerialNumber;
                        m_dataSegmentMetadata.reportSerialNumber = &m_dataSegment_reportSerialNumber;
                    }
                    else {
                        m_dataSegment_clientServiceData.clear();
                        m_dataSegment_clientServiceData.reserve(m_dataSegmentMetadata.length); //todo make sure cant crash
                        m_dataSegmentRxState = LTP_DATA_SEGMENT_RX_STATE::READ_CLIENT_SERVICE_DATA;
                        m_dataSegmentMetadata.checkpointSerialNumber = NULL;
                        m_dataSegmentMetadata.reportSerialNumber = NULL;
                    }
                }
            }
            else if (dataSegmentRxState == LTP_DATA_SEGMENT_RX_STATE::READ_CHECKPOINT_SERIAL_NUMBER_SDNV) {
                m_sdnvTempVec.push_back(rxVal);
                if (m_sdnvTempVec.size() > 10) {
                    errorMessage = "error in LTP_DATA_SEGMENT_RX_STATE::READ_CHECKPOINT_SERIAL_NUMBER_SDNV, sdnv > 10 bytes";
                    return false;
                }
                else if ((rxVal & 0x80) == 0) { //if msbit is a 0 then stop
                    uint8_t sdnvSize;
                    m_dataSegment_checkpointSerialNumber = SdnvDecodeU64(m_sdnvTempVec.data(), &sdnvSize);
                    if (sdnvSize != m_sdnvTempVec.size()) {
                        errorMessage = "error in LTP_DATA_SEGMENT_RX_STATE::READ_CHECKPOINT_SERIAL_NUMBER_SDNV, sdnvSize != m_sdnvTempVec.size()";
                        return false;
                    }
                    else {
                        m_sdnvTempVec.clear();
                        m_dataSegmentRxState = LTP_DATA_SEGMENT_RX_STATE::READ_REPORT_SERIAL_NUMBER_SDNV;
                    }
                }
            }
            else if (dataSegmentRxState == LTP_DATA_SEGMENT_RX_STATE::READ_REPORT_SERIAL_NUMBER_SDNV) {
                m_sdnvTempVec.push_back(rxVal);
                if (m_sdnvTempVec.size() > 10) {
                    errorMessage = "error in LTP_DATA_SEGMENT_RX_STATE::READ_REPORT_SERIAL_NUMBER_SDNV, sdnv > 10 bytes";
                    return false;
                }
                else if ((rxVal & 0x80) == 0) { //if msbit is a 0 then stop
                    uint8_t sdnvSize;
                    m_dataSegment_reportSerialNumber = SdnvDecodeU64(m_sdnvTempVec.data(), &sdnvSize);
                    if (sdnvSize != m_sdnvTempVec.size()) {
                        errorMessage = "error in LTP_DATA_SEGMENT_RX_STATE::READ_REPORT_SERIAL_NUMBER_SDNV, sdnvSize != m_sdnvTempVec.size()";
                        return false;
                    }
                    else {
                        m_dataSegment_clientServiceData.clear();
                        m_dataSegment_clientServiceData.reserve(m_dataSegmentMetadata.length); //todo make sure cant crash
                        m_dataSegmentRxState = LTP_DATA_SEGMENT_RX_STATE::READ_CLIENT_SERVICE_DATA;
                    }
                }
            }
            else if (dataSegmentRxState == LTP_DATA_SEGMENT_RX_STATE::READ_CLIENT_SERVICE_DATA) {
                m_dataSegment_clientServiceData.push_back(rxVal);
                if (m_dataSegment_clientServiceData.size() == m_dataSegmentMetadata.length) {


                    if (m_numTrailerExtensionTlvs) { //todo should callback be after trailer
                        m_trailerRxState = LTP_TRAILER_RX_STATE::READ_ONE_TRAILER_EXTENSION_TAG_BYTE;
                        m_mainRxState = LTP_MAIN_RX_STATE::READ_TRAILER;
                    }
                    else {
                        //callback data segment
                        if (m_dataSegmentContentsReadCallback) {
                            m_dataSegmentContentsReadCallback(m_segmentTypeFlags, m_sessionOriginatorEngineId, m_sessionNumber, m_dataSegment_clientServiceData, m_dataSegmentMetadata, m_headerExtensions, m_trailerExtensions);
                        }
                        SetBeginningState();
                    }
                }
                else {
                    const std::size_t bytesRemainingToCopy = m_dataSegmentMetadata.length - m_dataSegment_clientServiceData.size(); //guaranteed to be at least 1 from "if" above
                    const std::size_t bytesToCopy = std::min(numChars, bytesRemainingToCopy - 1); //leave last byte to go through the state machine
                    if (bytesToCopy) {
                        m_dataSegment_clientServiceData.insert(m_dataSegment_clientServiceData.end(), rxVals, rxVals + bytesToCopy); //concatenate
                        rxVals += bytesToCopy;
                        numChars -= bytesToCopy;
                    }
                }
            }
        }
        else if (mainRxState == LTP_MAIN_RX_STATE::READ_REPORT_SEGMENT_CONTENT) {
            const LTP_REPORT_SEGMENT_RX_STATE reportSegmentRxState = m_reportSegmentRxState; //const for optimization
            if (reportSegmentRxState == LTP_REPORT_SEGMENT_RX_STATE::READ_REPORT_SERIAL_NUMBER_SDNV) {
                m_sdnvTempVec.push_back(rxVal);
                if (m_sdnvTempVec.size() > 10) {
                    errorMessage = "error in LTP_REPORT_SEGMENT_RX_STATE::READ_REPORT_SERIAL_NUMBER_SDNV, sdnv > 10 bytes";
                    return false;
                }
                else if ((rxVal & 0x80) == 0) { //if msbit is a 0 then stop
                    uint8_t sdnvSize;
                    m_reportSegment.reportSerialNumber = SdnvDecodeU64(m_sdnvTempVec.data(), &sdnvSize);
                    if (sdnvSize != m_sdnvTempVec.size()) {
                        errorMessage = "error in LTP_REPORT_SEGMENT_RX_STATE::READ_REPORT_SERIAL_NUMBER_SDNV, sdnvSize != m_sdnvTempVec.size()";
                        return false;
                    }
                    else {
                        m_sdnvTempVec.clear();
                        m_reportSegmentRxState = LTP_REPORT_SEGMENT_RX_STATE::READ_CHECKPOINT_SERIAL_NUMBER_SDNV;
                    }
                }
            }
            else if (reportSegmentRxState == LTP_REPORT_SEGMENT_RX_STATE::READ_CHECKPOINT_SERIAL_NUMBER_SDNV) {
                m_sdnvTempVec.push_back(rxVal);
                if (m_sdnvTempVec.size() > 10) {
                    errorMessage = "error in LTP_REPORT_SEGMENT_RX_STATE::READ_CHECKPOINT_SERIAL_NUMBER_SDNV, sdnv > 10 bytes";
                    return false;
                }
                else if ((rxVal & 0x80) == 0) { //if msbit is a 0 then stop
                    uint8_t sdnvSize;
                    m_reportSegment.checkpointSerialNumber = SdnvDecodeU64(m_sdnvTempVec.data(), &sdnvSize);
                    if (sdnvSize != m_sdnvTempVec.size()) {
                        errorMessage = "error in LTP_REPORT_SEGMENT_RX_STATE::READ_CHECKPOINT_SERIAL_NUMBER_SDNV, sdnvSize != m_sdnvTempVec.size()";
                        return false;
                    }
                    else {
                        m_sdnvTempVec.clear();
                        m_reportSegmentRxState = LTP_REPORT_SEGMENT_RX_STATE::READ_UPPER_BOUND_SDNV;
                    }
                }
            }
            else if (reportSegmentRxState == LTP_REPORT_SEGMENT_RX_STATE::READ_UPPER_BOUND_SDNV) {
                m_sdnvTempVec.push_back(rxVal);
                if (m_sdnvTempVec.size() > 10) {
                    errorMessage = "error in LTP_REPORT_SEGMENT_RX_STATE::READ_UPPER_BOUND_SDNV, sdnv > 10 bytes";
                    return false;
                }
                else if ((rxVal & 0x80) == 0) { //if msbit is a 0 then stop
                    uint8_t sdnvSize;
                    m_reportSegment.upperBound = SdnvDecodeU64(m_sdnvTempVec.data(), &sdnvSize);
                    if (sdnvSize != m_sdnvTempVec.size()) {
                        errorMessage = "error in LTP_REPORT_SEGMENT_RX_STATE::READ_UPPER_BOUND_SDNV, sdnvSize != m_sdnvTempVec.size()";
                        return false;
                    }
                    else {
                        m_sdnvTempVec.clear();
                        m_reportSegmentRxState = LTP_REPORT_SEGMENT_RX_STATE::READ_LOWER_BOUND_SDNV;
                    }
                }
            }
            else if (reportSegmentRxState == LTP_REPORT_SEGMENT_RX_STATE::READ_LOWER_BOUND_SDNV) {
                m_sdnvTempVec.push_back(rxVal);
                if (m_sdnvTempVec.size() > 10) {
                    errorMessage = "error in LTP_REPORT_SEGMENT_RX_STATE::READ_LOWER_BOUND_SDNV, sdnv > 10 bytes";
                    return false;
                }
                else if ((rxVal & 0x80) == 0) { //if msbit is a 0 then stop
                    uint8_t sdnvSize;
                    m_reportSegment.lowerBound = SdnvDecodeU64(m_sdnvTempVec.data(), &sdnvSize);
                    if (sdnvSize != m_sdnvTempVec.size()) {
                        errorMessage = "error in LTP_REPORT_SEGMENT_RX_STATE::READ_LOWER_BOUND_SDNV, sdnvSize != m_sdnvTempVec.size()";
                        return false;
                    }
                    else {
                        m_sdnvTempVec.clear();
                        m_reportSegmentRxState = LTP_REPORT_SEGMENT_RX_STATE::READ_RECEPTION_CLAIM_COUNT_SDNV;
                    }
                }
            }
            else if (reportSegmentRxState == LTP_REPORT_SEGMENT_RX_STATE::READ_RECEPTION_CLAIM_COUNT_SDNV) {
                m_sdnvTempVec.push_back(rxVal);
                if (m_sdnvTempVec.size() > 10) {
                    errorMessage = "error in LTP_REPORT_SEGMENT_RX_STATE::READ_RECEPTION_CLAIM_COUNT_SDNV, sdnv > 10 bytes";
                    return false;
                }
                else if ((rxVal & 0x80) == 0) { //if msbit is a 0 then stop
                    uint8_t sdnvSize;
                    m_reportSegment.receptionClaimCount = SdnvDecodeU64(m_sdnvTempVec.data(), &sdnvSize);
                    if (sdnvSize != m_sdnvTempVec.size()) {
                        errorMessage = "error in LTP_REPORT_SEGMENT_RX_STATE::READ_RECEPTION_CLAIM_COUNT_SDNV, sdnvSize != m_sdnvTempVec.size()";
                        return false;
                    }
                    else if (m_reportSegment.receptionClaimCount == 0) { //must be 1 or more (The content of an RS comprises one or more data reception claims)
                        errorMessage = "error in LTP_REPORT_SEGMENT_RX_STATE::READ_RECEPTION_CLAIM_COUNT_SDNV, count == 0";
                        return false;
                    }
                    else {
                        m_sdnvTempVec.clear();
                        m_reportSegment.receptionClaims.clear();
                        m_reportSegment.receptionClaims.reserve(m_reportSegment.receptionClaimCount);
                        m_reportSegmentRxState = LTP_REPORT_SEGMENT_RX_STATE::READ_ONE_RECEPTION_CLAIM_OFFSET_SDNV;
                    }
                }
            }
            else if (reportSegmentRxState == LTP_REPORT_SEGMENT_RX_STATE::READ_ONE_RECEPTION_CLAIM_OFFSET_SDNV) {
                m_sdnvTempVec.push_back(rxVal);
                if (m_sdnvTempVec.size() > 10) {
                    errorMessage = "error in LTP_REPORT_SEGMENT_RX_STATE::READ_ONE_RECEPTION_CLAIM_OFFSET_SDNV, sdnv > 10 bytes";
                    return false;
                }
                else if ((rxVal & 0x80) == 0) { //if msbit is a 0 then stop
                    uint8_t sdnvSize;
                    const uint64_t claimOffset = SdnvDecodeU64(m_sdnvTempVec.data(), &sdnvSize);
                    if (sdnvSize != m_sdnvTempVec.size()) {
                        errorMessage = "error in LTP_REPORT_SEGMENT_RX_STATE::READ_ONE_RECEPTION_CLAIM_OFFSET_SDNV, sdnvSize != m_sdnvTempVec.size()";
                        return false;
                    }
                    else {
                        m_reportSegment.receptionClaims.resize(m_reportSegment.receptionClaims.size() + 1);
                        m_reportSegment.receptionClaims.back().offset = claimOffset;
                        m_sdnvTempVec.clear();
                        m_reportSegmentRxState = LTP_REPORT_SEGMENT_RX_STATE::READ_ONE_RECEPTION_CLAIM_LENGTH_SDNV;
                    }
                }
            }
            else if (reportSegmentRxState == LTP_REPORT_SEGMENT_RX_STATE::READ_ONE_RECEPTION_CLAIM_LENGTH_SDNV) {
                m_sdnvTempVec.push_back(rxVal);
                if (m_sdnvTempVec.size() > 10) {
                    errorMessage = "error in LTP_REPORT_SEGMENT_RX_STATE::READ_ONE_RECEPTION_CLAIM_LENGTH_SDNV, sdnv > 10 bytes";
                    return false;
                }
                else if ((rxVal & 0x80) == 0) { //if msbit is a 0 then stop
                    uint8_t sdnvSize;
                    const uint64_t claimLength = SdnvDecodeU64(m_sdnvTempVec.data(), &sdnvSize);
                    if (sdnvSize != m_sdnvTempVec.size()) {
                        errorMessage = "error in LTP_REPORT_SEGMENT_RX_STATE::READ_ONE_RECEPTION_CLAIM_LENGTH_SDNV, sdnvSize != m_sdnvTempVec.size()";
                        return false;
                    }
                    else if (claimLength == 0) { //must be 1 or more (A reception claim's length shall never be less than 1)
                        errorMessage = "error in LTP_REPORT_SEGMENT_RX_STATE::READ_ONE_RECEPTION_CLAIM_LENGTH_SDNV, count == 0";
                        return false;
                    }
                    else {
                        m_reportSegment.receptionClaims.back().length = claimLength;
                        m_sdnvTempVec.clear();
                        if (m_reportSegment.receptionClaims.size() < m_reportSegment.receptionClaimCount) {
                            m_reportSegmentRxState = LTP_REPORT_SEGMENT_RX_STATE::READ_ONE_RECEPTION_CLAIM_OFFSET_SDNV;
                        }
                        else if (m_numTrailerExtensionTlvs) {
                            m_trailerRxState = LTP_TRAILER_RX_STATE::READ_ONE_TRAILER_EXTENSION_TAG_BYTE;
                            m_mainRxState = LTP_MAIN_RX_STATE::READ_TRAILER;
                        }
                        else {
                            //callback report segment
                            if (m_reportSegmentContentsReadCallback) {
                                m_reportSegmentContentsReadCallback(m_sessionOriginatorEngineId, m_sessionNumber, m_reportSegment, m_headerExtensions, m_trailerExtensions);
                            }
                            SetBeginningState();
                        }
                    }
                }
            }
        }
        else if (mainRxState == LTP_MAIN_RX_STATE::READ_REPORT_ACKNOWLEDGEMENT_SEGMENT_CONTENT) {
            m_sdnvTempVec.push_back(rxVal);
            if (m_sdnvTempVec.size() > 10) {
                errorMessage = "error in LTP_MAIN_RX_STATE::READ_REPORT_ACKNOWLEDGEMENT_SEGMENT_CONTENT, sdnv > 10 bytes";
                return false;
            }
            else if ((rxVal & 0x80) == 0) { //if msbit is a 0 then stop
                uint8_t sdnvSize;
                m_reportAcknowledgementSegment_reportSerialNumber = SdnvDecodeU64(m_sdnvTempVec.data(), &sdnvSize);
                if (sdnvSize != m_sdnvTempVec.size()) {
                    errorMessage = "error in LTP_MAIN_RX_STATE::READ_REPORT_ACKNOWLEDGEMENT_SEGMENT_CONTENT, sdnvSize != m_sdnvTempVec.size()";
                    return false;
                }
                else if (m_numTrailerExtensionTlvs) {
                    m_trailerRxState = LTP_TRAILER_RX_STATE::READ_ONE_TRAILER_EXTENSION_TAG_BYTE;
                    m_mainRxState = LTP_MAIN_RX_STATE::READ_TRAILER;
                }
                else {
                    //callback report acknowledgement segment
                    if (m_reportAcknowledgementSegmentContentsReadCallback) {
                        m_reportAcknowledgementSegmentContentsReadCallback(m_sessionOriginatorEngineId, m_sessionNumber, m_reportAcknowledgementSegment_reportSerialNumber, m_headerExtensions, m_trailerExtensions);
                    }
                    SetBeginningState();
                }
            }

        }
        else if (mainRxState == LTP_MAIN_RX_STATE::READ_CANCEL_SEGMENT_CONTENT_BYTE) {
            m_cancelSegment_reasonCode = rxVal;
            if (m_numTrailerExtensionTlvs) {
                m_trailerRxState = LTP_TRAILER_RX_STATE::READ_ONE_TRAILER_EXTENSION_TAG_BYTE;
                m_mainRxState = LTP_MAIN_RX_STATE::READ_TRAILER;
            }
            else {
                //callback
                SetBeginningState();
            }
        }
        else if (mainRxState == LTP_MAIN_RX_STATE::READ_TRAILER) {
            const LTP_TRAILER_RX_STATE trailerRxState = m_trailerRxState;
            if (trailerRxState == LTP_TRAILER_RX_STATE::READ_ONE_TRAILER_EXTENSION_TAG_BYTE) {
#if (__cplusplus >= 201703L)
                ltp_extension_t & ext = m_trailerExtensions.extensionsVec.emplace_back();
                ext.tag = rxVal;
#else
                m_trailerExtensions.extensionsVec.emplace_back();
                m_trailerExtensions.extensionsVec.back().tag = rxVal;
#endif
                m_sdnvTempVec.clear();
                m_trailerRxState = LTP_TRAILER_RX_STATE::READ_ONE_TRAILER_EXTENSION_LENGTH_SDNV;
            }
            else if (trailerRxState == LTP_TRAILER_RX_STATE::READ_ONE_TRAILER_EXTENSION_LENGTH_SDNV) {
                m_sdnvTempVec.push_back(rxVal);
                if (m_sdnvTempVec.size() > 10) {
                    errorMessage = "error in LTP_TRAILER_RX_STATE::READ_ONE_TRAILER_EXTENSION_LENGTH_SDNV, sdnv > 10 bytes";
                    return false;
                }
                else if ((rxVal & 0x80) == 0) { //if msbit is a 0 then stop
                    uint8_t sdnvSize;
                    m_currentTrailerExtensionLength = SdnvDecodeU64(m_sdnvTempVec.data(), &sdnvSize);
                    if (sdnvSize != m_sdnvTempVec.size()) {
                        errorMessage = "error in LTP_TRAILER_RX_STATE::READ_ONE_TRAILER_EXTENSION_LENGTH_SDNV, sdnvSize != m_sdnvTempVec.size()";
                        return false;
                    }
                    else if (m_currentTrailerExtensionLength == 0) {
                        if (m_trailerExtensions.extensionsVec.size() == m_numTrailerExtensionTlvs) {
                            if(!NextStateAfterTrailerExtensions(errorMessage)) return false;
                        }
                        else {
                            m_trailerRxState = LTP_TRAILER_RX_STATE::READ_ONE_TRAILER_EXTENSION_TAG_BYTE;
                        }
                    }
                    else {
                        m_trailerExtensions.extensionsVec.back().valueVec.reserve(m_currentTrailerExtensionLength);
                        m_trailerRxState = LTP_TRAILER_RX_STATE::READ_ONE_TRAILER_EXTENSION_VALUE;
                    }
                }
            }
            else if (trailerRxState == LTP_TRAILER_RX_STATE::READ_ONE_TRAILER_EXTENSION_VALUE) {
                std::vector<uint8_t> & valueVec = m_trailerExtensions.extensionsVec.back().valueVec;
                valueVec.push_back(rxVal);
                if (valueVec.size() == m_currentTrailerExtensionLength) {
                    if (m_trailerExtensions.extensionsVec.size() == m_numTrailerExtensionTlvs) {
                        if (!NextStateAfterTrailerExtensions(errorMessage)) return false;
                    }
                    else {
                        m_trailerRxState = LTP_TRAILER_RX_STATE::READ_ONE_TRAILER_EXTENSION_TAG_BYTE; //continue loop
                    }
                }
            }
        }
    }
    return true;
}

bool Ltp::NextStateAfterHeaderExtensions(std::string & errorMessage) {
    if ((m_segmentTypeFlags & 0xd) == 0xd) { //CAx (cancel ack) with no contents
        if (m_numTrailerExtensionTlvs) {
            m_trailerRxState = LTP_TRAILER_RX_STATE::READ_ONE_TRAILER_EXTENSION_TAG_BYTE;
            m_mainRxState = LTP_MAIN_RX_STATE::READ_TRAILER;
        }
        else {
            //callback
        }
    }
    else if ((m_segmentTypeFlags == 5) || (m_segmentTypeFlags == 6) || (m_segmentTypeFlags == 10) || (m_segmentTypeFlags == 11)) { // undefined
        errorMessage = "error in NextStateAfterHeaderExtensions: undefined segment type flags: " + boost::lexical_cast<std::string>((int)m_segmentTypeFlags);
        return false;
    }
    else if (m_segmentTypeFlags <= 7) {
        m_sdnvTempVec.clear();
        m_dataSegmentRxState = LTP_DATA_SEGMENT_RX_STATE::READ_CLIENT_SERVICE_ID_SDNV;
        m_mainRxState = LTP_MAIN_RX_STATE::READ_DATA_SEGMENT_CONTENT;
    }
    else if (m_segmentTypeFlags == 8) {
        m_sdnvTempVec.clear();
        m_reportSegmentRxState = LTP_REPORT_SEGMENT_RX_STATE::READ_REPORT_SERIAL_NUMBER_SDNV;
        m_mainRxState = LTP_MAIN_RX_STATE::READ_REPORT_SEGMENT_CONTENT;
    }
    else if (m_segmentTypeFlags == 9) {
        m_sdnvTempVec.clear();
        m_mainRxState = LTP_MAIN_RX_STATE::READ_REPORT_ACKNOWLEDGEMENT_SEGMENT_CONTENT;
    }
    else { //12 or 14 => cancel segment
        m_mainRxState = LTP_MAIN_RX_STATE::READ_CANCEL_SEGMENT_CONTENT_BYTE;
    }
    return true;
}

bool Ltp::NextStateAfterTrailerExtensions(std::string & errorMessage) {
    
    if (m_numTrailerExtensionTlvs) {
        //callback trailer
    }

    if ((m_segmentTypeFlags & 0xd) == 0xd) { //CAx (cancel ack) with no contents
        //callback cax
    }
    else if ((m_segmentTypeFlags == 5) || (m_segmentTypeFlags == 6) || (m_segmentTypeFlags == 10) || (m_segmentTypeFlags == 11)) { // undefined
        errorMessage = "error in NextStateAfterTrailerExtensions: undefined segment type flags: " + boost::lexical_cast<std::string>((int)m_segmentTypeFlags);
        return false;
    }
    else if (m_segmentTypeFlags <= 7) {
        //callback data segment
        if (m_dataSegmentContentsReadCallback) {
            m_dataSegmentContentsReadCallback(m_segmentTypeFlags, m_sessionOriginatorEngineId, m_sessionNumber, m_dataSegment_clientServiceData, m_dataSegmentMetadata, m_headerExtensions, m_trailerExtensions);
        }
    }
    else if (m_segmentTypeFlags == 8) {
        //callback report segment
        if (m_reportSegmentContentsReadCallback) {
            m_reportSegmentContentsReadCallback(m_sessionOriginatorEngineId, m_sessionNumber, m_reportSegment, m_headerExtensions, m_trailerExtensions);
        }
    }
    else if (m_segmentTypeFlags == 9) {
        //callback report acknowledgement segment
        if (m_reportAcknowledgementSegmentContentsReadCallback) {
            m_reportAcknowledgementSegmentContentsReadCallback(m_sessionOriginatorEngineId, m_sessionNumber, m_reportAcknowledgementSegment_reportSerialNumber, m_headerExtensions, m_trailerExtensions);
        }
    }
    else { //12 or 14 => cancel segment
        //callback cancel segment
    }
    SetBeginningState();
    return true;
}


/*
void Tcpcl::GenerateContactHeader(std::vector<uint8_t> & hdr, CONTACT_HEADER_FLAGS flags, uint16_t keepAliveIntervalSeconds, const std::string & localEid) {
    uint8_t localEidSdnv[10];
    const uint64_t sdnvSize = SdnvEncodeU64(localEidSdnv, localEid.size());

    hdr.resize(8 + sdnvSize + localEid.size());

    hdr[0] = 'd';
    hdr[1] = 't';
    hdr[2] = 'n';
    hdr[3] = '!';
    hdr[4] = 3; //version
    hdr[5] = static_cast<uint8_t>(flags);
    boost::endian::native_to_big_inplace(keepAliveIntervalSeconds);
    memcpy(&hdr[6], &keepAliveIntervalSeconds, sizeof(keepAliveIntervalSeconds));
    memcpy(&hdr[8], localEidSdnv, sdnvSize);
    memcpy(&hdr[8 + sdnvSize], localEid.data(), localEid.size());
}*/

void Ltp::GenerateLtpHeaderPlusDataSegmentMetadata(std::vector<uint8_t> & ltpHeaderPlusDataSegmentMetadata, LTP_DATA_SEGMENT_TYPE_FLAGS dataSegmentTypeFlags, 
    uint64_t sessionOriginatorEngineId, uint64_t sessionNumber, const data_segment_metadata_t & dataSegmentMetadata,
    ltp_extensions_t * headerExtensions, uint8_t numTrailerExtensions)
{
    uint8_t numHeaderExtensions = 0;
    uint64_t maxBytesRequiredForHeaderExtensions = 0;
    if (headerExtensions) {
        numHeaderExtensions = static_cast<uint8_t>(headerExtensions->extensionsVec.size());
        maxBytesRequiredForHeaderExtensions = headerExtensions->GetMaximumDataRequiredForSerialization();
    }
    ltpHeaderPlusDataSegmentMetadata.resize(1 + 1 + (2 * 10) + dataSegmentMetadata.GetMaximumDataRequiredForSerialization() + maxBytesRequiredForHeaderExtensions); //flags + extensionCounts + 2 10-byte session sdnvs + metadata sdnvs + header extensions
    uint8_t * encodedPtr = ltpHeaderPlusDataSegmentMetadata.data();
    *encodedPtr++ = static_cast<uint8_t>(dataSegmentTypeFlags); //assumes version 0 in most significant 4 bits
    encodedPtr += SdnvEncodeU64(encodedPtr, sessionOriginatorEngineId);
    encodedPtr += SdnvEncodeU64(encodedPtr, sessionNumber);
    *encodedPtr++ = (numHeaderExtensions << 4) | numTrailerExtensions;
    if (headerExtensions) {
        encodedPtr += headerExtensions->Serialize(encodedPtr);
    }
    encodedPtr += dataSegmentMetadata.Serialize(encodedPtr);
    ltpHeaderPlusDataSegmentMetadata.resize(encodedPtr - ltpHeaderPlusDataSegmentMetadata.data());
}

void Ltp::GenerateReportSegmentLtpPacket(std::vector<uint8_t> & ltpReportSegmentPacket, uint64_t sessionOriginatorEngineId,
    uint64_t sessionNumber, const report_segment_t & reportSegmentStruct,
    ltp_extensions_t * headerExtensions, ltp_extensions_t * trailerExtensions)
{
    uint8_t numHeaderExtensions = 0;
    uint64_t maxBytesRequiredForHeaderExtensions = 0;
    if (headerExtensions) {
        numHeaderExtensions = static_cast<uint8_t>(headerExtensions->extensionsVec.size());
        maxBytesRequiredForHeaderExtensions = headerExtensions->GetMaximumDataRequiredForSerialization();
    }
    uint8_t numTrailerExtensions = 0;
    uint64_t maxBytesRequiredForTrailerExtensions = 0;
    if (trailerExtensions) {
        numTrailerExtensions = static_cast<uint8_t>(trailerExtensions->extensionsVec.size());
        maxBytesRequiredForTrailerExtensions = trailerExtensions->GetMaximumDataRequiredForSerialization();
    }
    ltpReportSegmentPacket.resize(1 + 1 + (2 * 10) + reportSegmentStruct.GetMaximumDataRequiredForSerialization() + maxBytesRequiredForHeaderExtensions + maxBytesRequiredForTrailerExtensions); //flags + extensionCounts + 2 session 10-byte sdnvs + rest of data
    uint8_t * encodedPtr = ltpReportSegmentPacket.data();
    *encodedPtr++ = static_cast<uint8_t>(LTP_SEGMENT_TYPE_FLAGS::REPORT_SEGMENT); //assumes version 0 in most significant 4 bits
    encodedPtr += SdnvEncodeU64(encodedPtr, sessionOriginatorEngineId);
    encodedPtr += SdnvEncodeU64(encodedPtr, sessionNumber);
    *encodedPtr++ = (numHeaderExtensions << 4) | numTrailerExtensions;
    if (headerExtensions) {
        encodedPtr += headerExtensions->Serialize(encodedPtr);
    }
    encodedPtr += reportSegmentStruct.Serialize(encodedPtr);
    if (trailerExtensions) {
        encodedPtr += trailerExtensions->Serialize(encodedPtr);
    }
    ltpReportSegmentPacket.resize(encodedPtr - ltpReportSegmentPacket.data());
}

void Ltp::GenerateReportAcknowledgementSegmentLtpPacket(std::vector<uint8_t> & ltpReportAcknowledgementSegmentPacket, uint64_t sessionOriginatorEngineId,
    uint64_t sessionNumber, uint64_t reportSerialNumberBeingAcknowledged,
    ltp_extensions_t * headerExtensions, ltp_extensions_t * trailerExtensions)
{
    uint8_t numHeaderExtensions = 0;
    uint64_t maxBytesRequiredForHeaderExtensions = 0;
    if (headerExtensions) {
        numHeaderExtensions = static_cast<uint8_t>(headerExtensions->extensionsVec.size());
        maxBytesRequiredForHeaderExtensions = headerExtensions->GetMaximumDataRequiredForSerialization();
    }
    uint8_t numTrailerExtensions = 0;
    uint64_t maxBytesRequiredForTrailerExtensions = 0;
    if (trailerExtensions) {
        numTrailerExtensions = static_cast<uint8_t>(trailerExtensions->extensionsVec.size());
        maxBytesRequiredForTrailerExtensions = trailerExtensions->GetMaximumDataRequiredForSerialization();
    }
    ltpReportAcknowledgementSegmentPacket.resize(1 + 1 + (2 * 10) + (1 * 10) + maxBytesRequiredForHeaderExtensions + maxBytesRequiredForTrailerExtensions); //flags + extensionCounts + 2 session 10-byte sdnvs + 1 report serial number 10-byte sdnv + rest of data
    uint8_t * encodedPtr = ltpReportAcknowledgementSegmentPacket.data();
    *encodedPtr++ = static_cast<uint8_t>(LTP_SEGMENT_TYPE_FLAGS::REPORT_ACK_SEGMENT); //assumes version 0 in most significant 4 bits
    encodedPtr += SdnvEncodeU64(encodedPtr, sessionOriginatorEngineId);
    encodedPtr += SdnvEncodeU64(encodedPtr, sessionNumber);
    *encodedPtr++ = (numHeaderExtensions << 4) | numTrailerExtensions;
    if (headerExtensions) {
        encodedPtr += headerExtensions->Serialize(encodedPtr);
    }
    encodedPtr += SdnvEncodeU64(encodedPtr, reportSerialNumberBeingAcknowledged);
    if (trailerExtensions) {
        encodedPtr += trailerExtensions->Serialize(encodedPtr);
    }
    ltpReportAcknowledgementSegmentPacket.resize(encodedPtr - ltpReportAcknowledgementSegmentPacket.data());
}
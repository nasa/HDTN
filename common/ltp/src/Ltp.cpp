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

Ltp::Ltp() {
    InitRx();
}
Ltp::~Ltp() {

}

void Ltp::SetDataSegmentContentsReadCallback(const DataSegmentContentsReadCallback_t & callback) {
    m_dataSegmentContentsReadCallback = callback;
}
/*void Tcpcl::SetContactHeaderReadCallback(ContactHeaderReadCallback_t callback) {
    m_contactHeaderReadCallback = callback;
}
void Tcpcl::SetAckSegmentReadCallback(AckSegmentReadCallback_t callback) {
    m_ackSegmentReadCallback = callback;
}
void Tcpcl::SetBundleRefusalCallback(BundleRefusalCallback_t callback) {
    m_bundleRefusalCallback = callback;
}
void Tcpcl::SetNextBundleLengthCallback(NextBundleLengthCallback_t callback) {
    m_nextBundleLengthCallback = callback;
}
void Tcpcl::SetKeepAliveCallback(KeepAliveCallback_t callback) {
    m_keepAliveCallback = callback;
}
void Tcpcl::SetShutdownMessageCallback(ShutdownMessageCallback_t callback) {
    m_shutdownMessageCallback = callback;
}
*/
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
                            m_dataSegmentContentsReadCallback(m_segmentTypeFlags, m_sessionOriginatorEngineId, m_sessionNumber, m_dataSegment_clientServiceData, m_dataSegmentMetadata);                           
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
                    m_reportSegment_reportSerialNumber = SdnvDecodeU64(m_sdnvTempVec.data(), &sdnvSize);
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
                    m_reportSegment_checkpointSerialNumber = SdnvDecodeU64(m_sdnvTempVec.data(), &sdnvSize);
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
                    m_reportSegment_upperBound = SdnvDecodeU64(m_sdnvTempVec.data(), &sdnvSize);
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
                    m_reportSegment_lowerBound = SdnvDecodeU64(m_sdnvTempVec.data(), &sdnvSize);
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
                    m_reportSegment_receptionClaimCount = SdnvDecodeU64(m_sdnvTempVec.data(), &sdnvSize);
                    if (sdnvSize != m_sdnvTempVec.size()) {
                        errorMessage = "error in LTP_REPORT_SEGMENT_RX_STATE::READ_RECEPTION_CLAIM_COUNT_SDNV, sdnvSize != m_sdnvTempVec.size()";
                        return false;
                    }
                    else if (m_reportSegment_receptionClaimCount == 0) { //must be 1 or more (The content of an RS comprises one or more data reception claims)
                        errorMessage = "error in LTP_REPORT_SEGMENT_RX_STATE::READ_RECEPTION_CLAIM_COUNT_SDNV, count == 0";
                        return false;
                    }
                    else {
                        m_sdnvTempVec.clear();
                        m_reportSegment_receptionClaims.clear();
                        m_reportSegment_receptionClaims.reserve(m_reportSegment_receptionClaimCount);
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
                        m_reportSegment_receptionClaims.resize(m_reportSegment_receptionClaims.size() + 1);
                        m_reportSegment_receptionClaims.back().offset = claimOffset;
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
                        m_reportSegment_receptionClaims.back().length = claimLength;
                        m_sdnvTempVec.clear();
                        if (m_reportSegment_receptionClaims.size() < m_reportSegment_receptionClaimCount) {
                            m_reportSegmentRxState = LTP_REPORT_SEGMENT_RX_STATE::READ_ONE_RECEPTION_CLAIM_OFFSET_SDNV;
                        }
                        else if (m_numTrailerExtensionTlvs) {
                            m_trailerRxState = LTP_TRAILER_RX_STATE::READ_ONE_TRAILER_EXTENSION_TAG_BYTE;
                            m_mainRxState = LTP_MAIN_RX_STATE::READ_TRAILER;
                        }
                        else {
                            //callback
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
                    //callback
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
            m_dataSegmentContentsReadCallback(m_segmentTypeFlags, m_sessionOriginatorEngineId, m_sessionNumber, m_dataSegment_clientServiceData, m_dataSegmentMetadata);
        }
    }
    else if (m_segmentTypeFlags == 8) {
        //callback report segment
    }
    else if (m_segmentTypeFlags == 9) {
        //callback report ack segment
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
    ltpHeaderPlusDataSegmentMetadata.resize(1 + 1 + (7 * 10) + maxBytesRequiredForHeaderExtensions); //flags + extensionCounts + up to 7 10-byte sdnvs + header extensions
    uint8_t * encodedPtr = ltpHeaderPlusDataSegmentMetadata.data();
    *encodedPtr++ = static_cast<uint8_t>(dataSegmentTypeFlags); //assumes version 0 in most significant 4 bits
    encodedPtr += SdnvEncodeU64(encodedPtr, sessionOriginatorEngineId);
    encodedPtr += SdnvEncodeU64(encodedPtr, sessionNumber);
    *encodedPtr++ = (numHeaderExtensions << 4) | numTrailerExtensions;
    if (headerExtensions) {
        encodedPtr += headerExtensions->Serialize(encodedPtr);
    }
    encodedPtr += SdnvEncodeU64(encodedPtr, dataSegmentMetadata.clientServiceId);
    encodedPtr += SdnvEncodeU64(encodedPtr, dataSegmentMetadata.offset);
    encodedPtr += SdnvEncodeU64(encodedPtr, dataSegmentMetadata.length);
    if (dataSegmentMetadata.checkpointSerialNumber && dataSegmentMetadata.reportSerialNumber) {
        encodedPtr += SdnvEncodeU64(encodedPtr, *dataSegmentMetadata.checkpointSerialNumber);
        encodedPtr += SdnvEncodeU64(encodedPtr, *dataSegmentMetadata.reportSerialNumber);
    }
    ltpHeaderPlusDataSegmentMetadata.resize(encodedPtr - ltpHeaderPlusDataSegmentMetadata.data());
}
/*void Ltp::GenerateDataSegmentWithoutCheckpoint(std::vector<uint8_t> & dataSegment, LTP_DATA_SEGMENT_TYPE_FLAGS dataSegmentType, uint64_t sessionOriginatorEngineId,
    uint64_t sessionNumber, uint64_t clientServiceId, uint64_t offsetBytes, uint64_t lengthBytes)
{

}
void Tcpcl::GenerateDataSegment(std::vector<uint8_t> & dataSegment, bool isStartSegment, bool isEndSegment, const uint8_t * contents, uint64_t sizeContents) {
    //std::cout << "szc " << sizeContents << std::endl;
    uint8_t dataSegmentHeader = (static_cast<uint8_t>(MESSAGE_TYPE_BYTE_CODES::DATA_SEGMENT)) << 4;
    if (isStartSegment) {
        dataSegmentHeader |= (1U << 1);
    }
    if (isEndSegment) {
        dataSegmentHeader |= (1U << 0);
    }
    uint8_t contentLengthSdnv[10];
    const uint64_t sdnvSize = SdnvEncodeU64(contentLengthSdnv, sizeContents);

    dataSegment.resize(1 + sdnvSize + sizeContents);

    dataSegment[0] = dataSegmentHeader;

    memcpy(&dataSegment[1], contentLengthSdnv, sdnvSize);
    memcpy(&dataSegment[1 + sdnvSize], contents, sizeContents);
}

void Tcpcl::GenerateDataSegmentHeaderOnly(std::vector<uint8_t> & dataSegmentHeaderDataVec, bool isStartSegment, bool isEndSegment, uint64_t sizeContents) {
    //std::cout << "szc " << sizeContents << std::endl;
    uint8_t dataSegmentHeader = (static_cast<uint8_t>(MESSAGE_TYPE_BYTE_CODES::DATA_SEGMENT)) << 4;
    if (isStartSegment) {
        dataSegmentHeader |= (1U << 1);
    }
    if (isEndSegment) {
        dataSegmentHeader |= (1U << 0);
    }
    uint8_t contentLengthSdnv[10];
    const uint64_t sdnvSize = SdnvEncodeU64(contentLengthSdnv, sizeContents);

    dataSegmentHeaderDataVec.resize(1 + sdnvSize);

    dataSegmentHeaderDataVec[0] = dataSegmentHeader;

    memcpy(&dataSegmentHeaderDataVec[1], contentLengthSdnv, sdnvSize);
}*/
/*
void Tcpcl::GenerateAckSegment(std::vector<uint8_t> & ackSegment, uint64_t totalBytesAcknowledged) {
    const uint8_t ackSegmentHeader = (static_cast<uint8_t>(MESSAGE_TYPE_BYTE_CODES::ACK_SEGMENT)) << 4;
    uint8_t totalBytesAcknowledgedSdnv[10];
    const uint64_t sdnvSize = SdnvEncodeU64(totalBytesAcknowledgedSdnv, totalBytesAcknowledged);
    ackSegment.resize(1 + sdnvSize);
    ackSegment[0] = ackSegmentHeader;
    memcpy(&ackSegment[1], totalBytesAcknowledgedSdnv, sdnvSize);
}

void Tcpcl::GenerateBundleRefusal(std::vector<uint8_t> & refusalMessage, BUNDLE_REFUSAL_CODES refusalCode) {
    uint8_t refusalHeader = (static_cast<uint8_t>(MESSAGE_TYPE_BYTE_CODES::REFUSE_BUNDLE)) << 4;
    refusalHeader |= static_cast<uint8_t>(refusalCode);
    refusalMessage.resize(1);
    refusalMessage[0] = refusalHeader;
}

void Tcpcl::GenerateBundleLength(std::vector<uint8_t> & bundleLengthMessage, uint64_t nextBundleLength) {
    const uint8_t bundleLengthHeader = (static_cast<uint8_t>(MESSAGE_TYPE_BYTE_CODES::LENGTH)) << 4;
    uint8_t nextBundleLengthSdnv[10];
    const uint64_t sdnvSize = SdnvEncodeU64(nextBundleLengthSdnv, nextBundleLength);
    bundleLengthMessage.resize(1 + sdnvSize);
    bundleLengthMessage[0] = bundleLengthHeader;
    memcpy(&bundleLengthMessage[1], nextBundleLengthSdnv, sdnvSize);
}

void Tcpcl::GenerateKeepAliveMessage(std::vector<uint8_t> & keepAliveMessage) {
    keepAliveMessage.resize(1);
    keepAliveMessage[0] = (static_cast<uint8_t>(MESSAGE_TYPE_BYTE_CODES::KEEPALIVE)) << 4;
}

void Tcpcl::GenerateShutdownMessage(std::vector<uint8_t> & shutdownMessage,
                                    bool includeReasonCode, SHUTDOWN_REASON_CODES shutdownReasonCode,
                                    bool includeReconnectionDelay, uint64_t reconnectionDelaySeconds)
{

    uint8_t shutdownHeader = (static_cast<uint8_t>(MESSAGE_TYPE_BYTE_CODES::SHUTDOWN)) << 4;
    uint8_t reconnectionDelaySecondsSdnv[10];
    uint64_t sdnvSize = 0;
    std::size_t totalMessageSizeBytes = 1;
    if (includeReasonCode) {
        shutdownHeader |= (1U << 1);
        totalMessageSizeBytes += 1;
    }
    if (includeReconnectionDelay) {
        shutdownHeader |= (1U << 0);
        sdnvSize = SdnvEncodeU64(reconnectionDelaySecondsSdnv, reconnectionDelaySeconds);
        totalMessageSizeBytes += sdnvSize;
    }

    shutdownMessage.resize(totalMessageSizeBytes);
    shutdownMessage[0] = shutdownHeader;
    uint8_t * ptr = (totalMessageSizeBytes > 1) ? &shutdownMessage[1] : NULL;
    if (includeReasonCode) {
        *ptr++ = static_cast<uint8_t>(shutdownReasonCode);
    }
    if (includeReconnectionDelay) {
        memcpy(ptr, reconnectionDelaySecondsSdnv, sdnvSize);
    }

}
*/
#include "Ltp.h"
#include <boost/make_shared.hpp>
#include <boost/foreach.hpp>
#include <boost/endian/conversion.hpp>
#include <iostream>
#include "Sdnv.h"

Ltp::Ltp() {
    InitRx();
}
Ltp::~Ltp() {

}
/*
void Tcpcl::SetDataSegmentContentsReadCallback(DataSegmentContentsReadCallback_t callback) {
    m_dataSegmentContentsReadCallback = callback;
}
void Tcpcl::SetContactHeaderReadCallback(ContactHeaderReadCallback_t callback) {
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
    m_sdnvTempVec.resize(0);
}

void Ltp::HandleReceivedChar(const uint8_t rxVal) {
    HandleReceivedChars(&rxVal, 1);
}

void Ltp::HandleReceivedChars(const uint8_t * rxVals, std::size_t numChars) {
    while (numChars) {
        --numChars;
        const uint8_t rxVal = *rxVals++;
        const LTP_MAIN_RX_STATE mainRxState = m_mainRxState; //const for optimization
        if (mainRxState == LTP_MAIN_RX_STATE::READ_HEADER) {
            const LTP_HEADER_RX_STATE headerRxState = m_headerRxState; //const for optimization
            //magic:  A four-byte field that always contains the byte sequence 0x64	0x74 0x6e 0x21,
            //i.e., the text string "dtn!" in US - ASCII.
            if (headerRxState == LTP_HEADER_RX_STATE::READ_CONTROL_BYTE) {
                const uint8_t ltpVersion = rxVal >> 4;
                if (ltpVersion != 0) {
                    std::cerr << "error ltp version not 0.. got " << (int)ltpVersion << std::endl;
                }
                else {
                    m_segmentTypeFlags = rxVal & 0x0f;
                    m_sdnvTempVec.resize(0);
                    m_headerRxState = LTP_HEADER_RX_STATE::READ_SESSION_ORIGINATOR_ENGINE_ID_SDNV;
                }
            }
            else if (headerRxState == LTP_HEADER_RX_STATE::READ_SESSION_ORIGINATOR_ENGINE_ID_SDNV) {
                m_sdnvTempVec.push_back(rxVal);
                if (m_sdnvTempVec.size() > 10) {
                    std::cout << "error in LTP_HEADER_RX_STATE::READ_SESSION_ORIGINATOR_ENGINE_ID_SDNV, sdnv > 10 bytes\n";
                    m_headerRxState = LTP_HEADER_RX_STATE::READ_CONTROL_BYTE;
                }
                else if ((rxVal & 0x80) == 0) { //if msbit is a 0 then stop
                    uint8_t sdnvSize;
                    m_sessionOriginatorEngineId = SdnvDecodeU64(m_sdnvTempVec.data(), &sdnvSize);
                    if (sdnvSize != m_sdnvTempVec.size()) {
                        std::cout << "error in LTP_HEADER_RX_STATE::READ_SESSION_ORIGINATOR_ENGINE_ID_SDNV, sdnvSize != m_sdnvTempVec.size()\n";
                        m_headerRxState = LTP_HEADER_RX_STATE::READ_CONTROL_BYTE;
                    }
                    else {
                        m_sdnvTempVec.resize(0);
                        m_headerRxState = LTP_HEADER_RX_STATE::READ_SESSION_NUMBER_SDNV;
                    }
                }
            }
            else if (headerRxState == LTP_HEADER_RX_STATE::READ_SESSION_NUMBER_SDNV) {
                m_sdnvTempVec.push_back(rxVal);
                if (m_sdnvTempVec.size() > 10) {
                    std::cout << "error in LTP_HEADER_RX_STATE::READ_SESSION_NUMBER_SDNV, sdnv > 10 bytes\n";
                    m_headerRxState = LTP_HEADER_RX_STATE::READ_CONTROL_BYTE;
                }
                else if ((rxVal & 0x80) == 0) { //if msbit is a 0 then stop
                    uint8_t sdnvSize;
                    m_sessionNumber = SdnvDecodeU64(m_sdnvTempVec.data(), &sdnvSize);
                    if (sdnvSize != m_sdnvTempVec.size()) {
                        std::cout << "error in LTP_HEADER_RX_STATE::READ_SESSION_NUMBER_SDNV, sdnvSize != m_sdnvTempVec.size()\n";
                        m_headerRxState = LTP_HEADER_RX_STATE::READ_CONTROL_BYTE;
                    }
                    else {
                        m_headerRxState = LTP_HEADER_RX_STATE::READ_NUM_EXTENSIONS_BYTE;
                    }
                }
            }
            else if (headerRxState == LTP_HEADER_RX_STATE::READ_NUM_EXTENSIONS_BYTE) { //msb
                m_numHeaderExtensionTlvs = rxVal >> 4;;
                m_numTrailerExtensionTlvs = rxVal & 0x0f;
                if (m_numHeaderExtensionTlvs) {
                    m_headerRxState = LTP_HEADER_RX_STATE::READ_ONE_HEADER_EXTENSION_TAG_BYTE;
                }
                else if ((m_segmentTypeFlags & 0xd) == 0xd) { //CAx (cancel ack) with no contents
                    if (m_numTrailerExtensionTlvs) {
                        m_trailerRxState = LTP_TRAILER_RX_STATE::READ_ONE_TRAILER_EXTENSION_TAG_BYTE;
                        m_mainRxState = LTP_MAIN_RX_STATE::READ_TRAILER;
                    }
                    else {
                        //callback
                    }
                }
                else if ((m_segmentTypeFlags == 5) || (m_segmentTypeFlags == 6) || (m_segmentTypeFlags == 10) || (m_segmentTypeFlags == 11)) { // undefined
                    std::cerr << "error in LTP_HEADER_RX_STATE::READ_NUM_EXTENSIONS_BYTE: undefined segment type flags: " << (int)m_segmentTypeFlags << std::endl;
                    m_headerRxState = LTP_HEADER_RX_STATE::READ_CONTROL_BYTE;
                }
                else if (m_segmentTypeFlags <= 7) {
                    m_sdnvTempVec.resize(0);
                    m_dataSegmentRxState = LTP_DATA_SEGMENT_RX_STATE::READ_CLIENT_SERVICE_ID_SDNV;
                    m_mainRxState = LTP_MAIN_RX_STATE::READ_DATA_SEGMENT_CONTENT;
                }
                else if (m_segmentTypeFlags == 8) {
                    m_sdnvTempVec.resize(0);
                    m_reportSegmentRxState = LTP_REPORT_SEGMENT_RX_STATE::READ_REPORT_SERIAL_NUMBER_SDNV;
                    m_mainRxState = LTP_MAIN_RX_STATE::READ_REPORT_SEGMENT_CONTENT;
                }
                else if (m_segmentTypeFlags == 9) {
                    m_sdnvTempVec.resize(0);
                    m_mainRxState = LTP_MAIN_RX_STATE::READ_REPORT_ACKNOWLEDGEMENT_SEGMENT_CONTENT;
                }
                else { //12 or 14 => cancel segment
                    m_mainRxState = LTP_MAIN_RX_STATE::READ_CANCEL_SEGMENT_CONTENT_BYTE;
                }
            }
        }
        else if (mainRxState == LTP_MAIN_RX_STATE::READ_DATA_SEGMENT_CONTENT) {
            const LTP_DATA_SEGMENT_RX_STATE dataSegmentRxState = m_dataSegmentRxState; //const for optimization
            if (dataSegmentRxState == LTP_DATA_SEGMENT_RX_STATE::READ_CLIENT_SERVICE_ID_SDNV) {
                m_sdnvTempVec.push_back(rxVal);
                if (m_sdnvTempVec.size() > 10) {
                    std::cout << "error in LTP_DATA_SEGMENT_RX_STATE::READ_CLIENT_SERVICE_ID_SDNV, sdnv > 10 bytes\n";
                    m_mainRxState = LTP_MAIN_RX_STATE::READ_HEADER;
                    m_headerRxState = LTP_HEADER_RX_STATE::READ_CONTROL_BYTE;
                }
                else if ((rxVal & 0x80) == 0) { //if msbit is a 0 then stop
                    uint8_t sdnvSize;
                    m_dataSegment_clientServiceId = SdnvDecodeU64(m_sdnvTempVec.data(), &sdnvSize);
                    if (sdnvSize != m_sdnvTempVec.size()) {
                        std::cout << "error in LTP_DATA_SEGMENT_RX_STATE::READ_CLIENT_SERVICE_ID_SDNV, sdnvSize != m_sdnvTempVec.size()\n";
                        m_mainRxState = LTP_MAIN_RX_STATE::READ_HEADER;
                        m_headerRxState = LTP_HEADER_RX_STATE::READ_CONTROL_BYTE;
                    }
                    else {
                        m_sdnvTempVec.resize(0);
                        m_dataSegmentRxState = LTP_DATA_SEGMENT_RX_STATE::READ_OFFSET_SDNV;
                    }
                }
            }
            else if (dataSegmentRxState == LTP_DATA_SEGMENT_RX_STATE::READ_OFFSET_SDNV) {
                m_sdnvTempVec.push_back(rxVal);
                if (m_sdnvTempVec.size() > 10) {
                    std::cout << "error in LTP_DATA_SEGMENT_RX_STATE::READ_OFFSET_SDNV, sdnv > 10 bytes\n";
                    m_mainRxState = LTP_MAIN_RX_STATE::READ_HEADER;
                    m_headerRxState = LTP_HEADER_RX_STATE::READ_CONTROL_BYTE;
                }
                else if ((rxVal & 0x80) == 0) { //if msbit is a 0 then stop
                    uint8_t sdnvSize;
                    m_dataSegment_offset = SdnvDecodeU64(m_sdnvTempVec.data(), &sdnvSize);
                    if (sdnvSize != m_sdnvTempVec.size()) {
                        std::cout << "error in LTP_DATA_SEGMENT_RX_STATE::READ_OFFSET_SDNV, sdnvSize != m_sdnvTempVec.size()\n";
                        m_mainRxState = LTP_MAIN_RX_STATE::READ_HEADER;
                        m_headerRxState = LTP_HEADER_RX_STATE::READ_CONTROL_BYTE;
                    }
                    else {
                        m_sdnvTempVec.resize(0);
                        m_dataSegmentRxState = LTP_DATA_SEGMENT_RX_STATE::READ_LENGTH_SDNV;
                    }
                }
            }
            else if (dataSegmentRxState == LTP_DATA_SEGMENT_RX_STATE::READ_LENGTH_SDNV) {
                m_sdnvTempVec.push_back(rxVal);
                if (m_sdnvTempVec.size() > 10) {
                    std::cout << "error in LTP_DATA_SEGMENT_RX_STATE::READ_LENGTH_SDNV, sdnv > 10 bytes\n";
                    m_mainRxState = LTP_MAIN_RX_STATE::READ_HEADER;
                    m_headerRxState = LTP_HEADER_RX_STATE::READ_CONTROL_BYTE;
                }
                else if ((rxVal & 0x80) == 0) { //if msbit is a 0 then stop
                    uint8_t sdnvSize;
                    m_dataSegment_length = SdnvDecodeU64(m_sdnvTempVec.data(), &sdnvSize);
                    if (sdnvSize != m_sdnvTempVec.size()) {
                        std::cout << "error in LTP_DATA_SEGMENT_RX_STATE::READ_LENGTH_SDNV, sdnvSize != m_sdnvTempVec.size()\n";
                        m_mainRxState = LTP_MAIN_RX_STATE::READ_HEADER;
                        m_headerRxState = LTP_HEADER_RX_STATE::READ_CONTROL_BYTE;
                    }
                    else if (m_dataSegment_length == 0) { //not sure if this is correct
                        std::cout << "error in LTP_DATA_SEGMENT_RX_STATE::READ_LENGTH_SDNV, length == 0\n";
                        m_mainRxState = LTP_MAIN_RX_STATE::READ_HEADER;
                        m_headerRxState = LTP_HEADER_RX_STATE::READ_CONTROL_BYTE;
                    }
                    else if ((m_segmentTypeFlags >= 1) && (m_segmentTypeFlags <= 3)) { //checkpoint
                        m_sdnvTempVec.resize(0);
                        m_dataSegmentRxState = LTP_DATA_SEGMENT_RX_STATE::READ_CHECKPOINT_SERIAL_NUMBER_SDNV;
                    }
                    else {
                        m_dataSegment_clientServiceData.resize(0);
                        m_dataSegment_clientServiceData.reserve(m_dataSegment_length); //todo make sure cant crash
                        m_dataSegmentRxState = LTP_DATA_SEGMENT_RX_STATE::READ_CLIENT_SERVICE_DATA;
                    }
                }
            }
            else if (dataSegmentRxState == LTP_DATA_SEGMENT_RX_STATE::READ_CHECKPOINT_SERIAL_NUMBER_SDNV) {
                m_sdnvTempVec.push_back(rxVal);
                if (m_sdnvTempVec.size() > 10) {
                    std::cout << "error in LTP_DATA_SEGMENT_RX_STATE::READ_CHECKPOINT_SERIAL_NUMBER_SDNV, sdnv > 10 bytes\n";
                    m_mainRxState = LTP_MAIN_RX_STATE::READ_HEADER;
                    m_headerRxState = LTP_HEADER_RX_STATE::READ_CONTROL_BYTE;
                }
                else if ((rxVal & 0x80) == 0) { //if msbit is a 0 then stop
                    uint8_t sdnvSize;
                    m_dataSegment_checkpointSerialNumber = SdnvDecodeU64(m_sdnvTempVec.data(), &sdnvSize);
                    if (sdnvSize != m_sdnvTempVec.size()) {
                        std::cout << "error in LTP_DATA_SEGMENT_RX_STATE::READ_CHECKPOINT_SERIAL_NUMBER_SDNV, sdnvSize != m_sdnvTempVec.size()\n";
                        m_mainRxState = LTP_MAIN_RX_STATE::READ_HEADER;
                        m_headerRxState = LTP_HEADER_RX_STATE::READ_CONTROL_BYTE;
                    }
                    else {
                        m_sdnvTempVec.resize(0);
                        m_dataSegmentRxState = LTP_DATA_SEGMENT_RX_STATE::READ_REPORT_SERIAL_NUMBER_SDNV;
                    }
                }
            }
            else if (dataSegmentRxState == LTP_DATA_SEGMENT_RX_STATE::READ_REPORT_SERIAL_NUMBER_SDNV) {
                m_sdnvTempVec.push_back(rxVal);
                if (m_sdnvTempVec.size() > 10) {
                    std::cout << "error in LTP_DATA_SEGMENT_RX_STATE::READ_REPORT_SERIAL_NUMBER_SDNV, sdnv > 10 bytes\n";
                    m_mainRxState = LTP_MAIN_RX_STATE::READ_HEADER;
                    m_headerRxState = LTP_HEADER_RX_STATE::READ_CONTROL_BYTE;
                }
                else if ((rxVal & 0x80) == 0) { //if msbit is a 0 then stop
                    uint8_t sdnvSize;
                    m_dataSegment_reportSerialNumber = SdnvDecodeU64(m_sdnvTempVec.data(), &sdnvSize);
                    if (sdnvSize != m_sdnvTempVec.size()) {
                        std::cout << "error in LTP_DATA_SEGMENT_RX_STATE::READ_REPORT_SERIAL_NUMBER_SDNV, sdnvSize != m_sdnvTempVec.size()\n";
                        m_mainRxState = LTP_MAIN_RX_STATE::READ_HEADER;
                        m_headerRxState = LTP_HEADER_RX_STATE::READ_CONTROL_BYTE;
                    }
                    else {
                        m_dataSegment_clientServiceData.resize(0);
                        m_dataSegment_clientServiceData.reserve(m_dataSegment_length); //todo make sure cant crash
                        m_dataSegmentRxState = LTP_DATA_SEGMENT_RX_STATE::READ_CLIENT_SERVICE_DATA;
                    }
                }
            }
            else if (dataSegmentRxState == LTP_DATA_SEGMENT_RX_STATE::READ_CLIENT_SERVICE_DATA) {
                m_dataSegment_clientServiceData.push_back(rxVal);
                if (m_dataSegment_clientServiceData.size() == m_dataSegment_length) {


                    if (m_numTrailerExtensionTlvs) { //todo should callback be after trailer
                        m_trailerRxState = LTP_TRAILER_RX_STATE::READ_ONE_TRAILER_EXTENSION_TAG_BYTE;
                        m_mainRxState = LTP_MAIN_RX_STATE::READ_TRAILER;
                    }
                    else {
                        //callback
                            //if (m_dataSegmentContentsReadCallback) {
                        //    m_dataSegmentContentsReadCallback(m_dataSegmentDataVec, m_dataSegmentStartFlag, m_dataSegmentEndFlag);
                        //}
                        m_mainRxState = LTP_MAIN_RX_STATE::READ_HEADER;
                        m_headerRxState = LTP_HEADER_RX_STATE::READ_CONTROL_BYTE;
                    }
                }
                else {
                    const std::size_t bytesRemainingToCopy = m_dataSegment_length - m_dataSegment_clientServiceData.size(); //guaranteed to be at least 1 from "if" above
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
                    std::cout << "error in LTP_REPORT_SEGMENT_RX_STATE::READ_REPORT_SERIAL_NUMBER_SDNV, sdnv > 10 bytes\n";
                    m_mainRxState = LTP_MAIN_RX_STATE::READ_HEADER;
                    m_headerRxState = LTP_HEADER_RX_STATE::READ_CONTROL_BYTE;
                }
                else if ((rxVal & 0x80) == 0) { //if msbit is a 0 then stop
                    uint8_t sdnvSize;
                    m_reportSegment_reportSerialNumber = SdnvDecodeU64(m_sdnvTempVec.data(), &sdnvSize);
                    if (sdnvSize != m_sdnvTempVec.size()) {
                        std::cout << "error in LTP_REPORT_SEGMENT_RX_STATE::READ_REPORT_SERIAL_NUMBER_SDNV, sdnvSize != m_sdnvTempVec.size()\n";
                        m_mainRxState = LTP_MAIN_RX_STATE::READ_HEADER;
                        m_headerRxState = LTP_HEADER_RX_STATE::READ_CONTROL_BYTE;
                    }
                    else {
                        m_sdnvTempVec.resize(0);
                        m_reportSegmentRxState = LTP_REPORT_SEGMENT_RX_STATE::READ_CHECKPOINT_SERIAL_NUMBER_SDNV;
                    }
                }
            }
            else if (reportSegmentRxState == LTP_REPORT_SEGMENT_RX_STATE::READ_CHECKPOINT_SERIAL_NUMBER_SDNV) {
                m_sdnvTempVec.push_back(rxVal);
                if (m_sdnvTempVec.size() > 10) {
                    std::cout << "error in LTP_REPORT_SEGMENT_RX_STATE::READ_CHECKPOINT_SERIAL_NUMBER_SDNV, sdnv > 10 bytes\n";
                    m_mainRxState = LTP_MAIN_RX_STATE::READ_HEADER;
                    m_headerRxState = LTP_HEADER_RX_STATE::READ_CONTROL_BYTE;
                }
                else if ((rxVal & 0x80) == 0) { //if msbit is a 0 then stop
                    uint8_t sdnvSize;
                    m_reportSegment_checkpointSerialNumber = SdnvDecodeU64(m_sdnvTempVec.data(), &sdnvSize);
                    if (sdnvSize != m_sdnvTempVec.size()) {
                        std::cout << "error in LTP_REPORT_SEGMENT_RX_STATE::READ_CHECKPOINT_SERIAL_NUMBER_SDNV, sdnvSize != m_sdnvTempVec.size()\n";
                        m_mainRxState = LTP_MAIN_RX_STATE::READ_HEADER;
                        m_headerRxState = LTP_HEADER_RX_STATE::READ_CONTROL_BYTE;
                    }
                    else {
                        m_sdnvTempVec.resize(0);
                        m_reportSegmentRxState = LTP_REPORT_SEGMENT_RX_STATE::READ_UPPER_BOUND_SDNV;
                    }
                }
            }
            else if (reportSegmentRxState == LTP_REPORT_SEGMENT_RX_STATE::READ_UPPER_BOUND_SDNV) {
                m_sdnvTempVec.push_back(rxVal);
                if (m_sdnvTempVec.size() > 10) {
                    std::cout << "error in LTP_REPORT_SEGMENT_RX_STATE::READ_UPPER_BOUND_SDNV, sdnv > 10 bytes\n";
                    m_mainRxState = LTP_MAIN_RX_STATE::READ_HEADER;
                    m_headerRxState = LTP_HEADER_RX_STATE::READ_CONTROL_BYTE;
                }
                else if ((rxVal & 0x80) == 0) { //if msbit is a 0 then stop
                    uint8_t sdnvSize;
                    m_reportSegment_upperBound = SdnvDecodeU64(m_sdnvTempVec.data(), &sdnvSize);
                    if (sdnvSize != m_sdnvTempVec.size()) {
                        std::cout << "error in LTP_REPORT_SEGMENT_RX_STATE::READ_UPPER_BOUND_SDNV, sdnvSize != m_sdnvTempVec.size()\n";
                        m_mainRxState = LTP_MAIN_RX_STATE::READ_HEADER;
                        m_headerRxState = LTP_HEADER_RX_STATE::READ_CONTROL_BYTE;
                    }
                    else {
                        m_sdnvTempVec.resize(0);
                        m_reportSegmentRxState = LTP_REPORT_SEGMENT_RX_STATE::READ_LOWER_BOUND_SDNV;
                    }
                }
            }
            else if (reportSegmentRxState == LTP_REPORT_SEGMENT_RX_STATE::READ_LOWER_BOUND_SDNV) {
                m_sdnvTempVec.push_back(rxVal);
                if (m_sdnvTempVec.size() > 10) {
                    std::cout << "error in LTP_REPORT_SEGMENT_RX_STATE::READ_LOWER_BOUND_SDNV, sdnv > 10 bytes\n";
                    m_mainRxState = LTP_MAIN_RX_STATE::READ_HEADER;
                    m_headerRxState = LTP_HEADER_RX_STATE::READ_CONTROL_BYTE;
                }
                else if ((rxVal & 0x80) == 0) { //if msbit is a 0 then stop
                    uint8_t sdnvSize;
                    m_reportSegment_lowerBound = SdnvDecodeU64(m_sdnvTempVec.data(), &sdnvSize);
                    if (sdnvSize != m_sdnvTempVec.size()) {
                        std::cout << "error in LTP_REPORT_SEGMENT_RX_STATE::READ_LOWER_BOUND_SDNV, sdnvSize != m_sdnvTempVec.size()\n";
                        m_mainRxState = LTP_MAIN_RX_STATE::READ_HEADER;
                        m_headerRxState = LTP_HEADER_RX_STATE::READ_CONTROL_BYTE;
                    }
                    else {
                        m_sdnvTempVec.resize(0);
                        m_reportSegmentRxState = LTP_REPORT_SEGMENT_RX_STATE::READ_RECEPTION_CLAIM_COUNT_SDNV;
                    }
                }
            }
            else if (reportSegmentRxState == LTP_REPORT_SEGMENT_RX_STATE::READ_RECEPTION_CLAIM_COUNT_SDNV) {
                m_sdnvTempVec.push_back(rxVal);
                if (m_sdnvTempVec.size() > 10) {
                    std::cout << "error in LTP_REPORT_SEGMENT_RX_STATE::READ_RECEPTION_CLAIM_COUNT_SDNV, sdnv > 10 bytes\n";
                    m_mainRxState = LTP_MAIN_RX_STATE::READ_HEADER;
                    m_headerRxState = LTP_HEADER_RX_STATE::READ_CONTROL_BYTE;
                }
                else if ((rxVal & 0x80) == 0) { //if msbit is a 0 then stop
                    uint8_t sdnvSize;
                    m_reportSegment_receptionClaimCount = SdnvDecodeU64(m_sdnvTempVec.data(), &sdnvSize);
                    if (sdnvSize != m_sdnvTempVec.size()) {
                        std::cout << "error in LTP_REPORT_SEGMENT_RX_STATE::READ_RECEPTION_CLAIM_COUNT_SDNV, sdnvSize != m_sdnvTempVec.size()\n";
                        m_mainRxState = LTP_MAIN_RX_STATE::READ_HEADER;
                        m_headerRxState = LTP_HEADER_RX_STATE::READ_CONTROL_BYTE;
                    }
                    else if (m_reportSegment_receptionClaimCount == 0) { //must be 1 or more (The content of an RS comprises one or more data reception claims)
                        std::cout << "error in LTP_REPORT_SEGMENT_RX_STATE::READ_RECEPTION_CLAIM_COUNT_SDNV, count == 0\n";
                        m_mainRxState = LTP_MAIN_RX_STATE::READ_HEADER;
                        m_headerRxState = LTP_HEADER_RX_STATE::READ_CONTROL_BYTE;
                    }
                    else {
                        m_sdnvTempVec.resize(0);
                        m_reportSegment_receptionClaims.resize(0);
                        m_reportSegment_receptionClaims.reserve(m_reportSegment_receptionClaimCount);
                        m_reportSegmentRxState = LTP_REPORT_SEGMENT_RX_STATE::READ_ONE_RECEPTION_CLAIM_OFFSET_SDNV;
                    }
                }
            }
            else if (reportSegmentRxState == LTP_REPORT_SEGMENT_RX_STATE::READ_ONE_RECEPTION_CLAIM_OFFSET_SDNV) {
                m_sdnvTempVec.push_back(rxVal);
                if (m_sdnvTempVec.size() > 10) {
                    std::cout << "error in LTP_REPORT_SEGMENT_RX_STATE::READ_ONE_RECEPTION_CLAIM_OFFSET_SDNV, sdnv > 10 bytes\n";
                    m_mainRxState = LTP_MAIN_RX_STATE::READ_HEADER;
                    m_headerRxState = LTP_HEADER_RX_STATE::READ_CONTROL_BYTE;
                }
                else if ((rxVal & 0x80) == 0) { //if msbit is a 0 then stop
                    uint8_t sdnvSize;
                    const uint64_t claimOffset = SdnvDecodeU64(m_sdnvTempVec.data(), &sdnvSize);
                    if (sdnvSize != m_sdnvTempVec.size()) {
                        std::cout << "error in LTP_REPORT_SEGMENT_RX_STATE::READ_ONE_RECEPTION_CLAIM_OFFSET_SDNV, sdnvSize != m_sdnvTempVec.size()\n";
                        m_mainRxState = LTP_MAIN_RX_STATE::READ_HEADER;
                        m_headerRxState = LTP_HEADER_RX_STATE::READ_CONTROL_BYTE;
                    }
                    else {
                        m_reportSegment_receptionClaims.resize(m_reportSegment_receptionClaims.size() + 1);
                        m_reportSegment_receptionClaims.back().offset = claimOffset;
                        m_sdnvTempVec.resize(0);
                        m_reportSegmentRxState = LTP_REPORT_SEGMENT_RX_STATE::READ_ONE_RECEPTION_CLAIM_LENGTH_SDNV;
                    }
                }
            }
            else if (reportSegmentRxState == LTP_REPORT_SEGMENT_RX_STATE::READ_ONE_RECEPTION_CLAIM_LENGTH_SDNV) {
                m_sdnvTempVec.push_back(rxVal);
                if (m_sdnvTempVec.size() > 10) {
                    std::cout << "error in LTP_REPORT_SEGMENT_RX_STATE::READ_ONE_RECEPTION_CLAIM_LENGTH_SDNV, sdnv > 10 bytes\n";
                    m_mainRxState = LTP_MAIN_RX_STATE::READ_HEADER;
                    m_headerRxState = LTP_HEADER_RX_STATE::READ_CONTROL_BYTE;
                }
                else if ((rxVal & 0x80) == 0) { //if msbit is a 0 then stop
                    uint8_t sdnvSize;
                    const uint64_t claimLength = SdnvDecodeU64(m_sdnvTempVec.data(), &sdnvSize);
                    if (sdnvSize != m_sdnvTempVec.size()) {
                        std::cout << "error in LTP_REPORT_SEGMENT_RX_STATE::READ_ONE_RECEPTION_CLAIM_LENGTH_SDNV, sdnvSize != m_sdnvTempVec.size()\n";
                        m_mainRxState = LTP_MAIN_RX_STATE::READ_HEADER;
                        m_headerRxState = LTP_HEADER_RX_STATE::READ_CONTROL_BYTE;
                    }
                    else if (claimLength == 0) { //must be 1 or more (A reception claim's length shall never be less than 1)
                        std::cout << "error in LTP_REPORT_SEGMENT_RX_STATE::READ_ONE_RECEPTION_CLAIM_LENGTH_SDNV, count == 0\n";
                        m_mainRxState = LTP_MAIN_RX_STATE::READ_HEADER;
                        m_headerRxState = LTP_HEADER_RX_STATE::READ_CONTROL_BYTE;
                    }
                    else {
                        m_reportSegment_receptionClaims.back().length = claimLength;
                        m_sdnvTempVec.resize(0);
                        if (m_reportSegment_receptionClaims.size() < m_reportSegment_receptionClaimCount) {
                            m_reportSegmentRxState = LTP_REPORT_SEGMENT_RX_STATE::READ_ONE_RECEPTION_CLAIM_OFFSET_SDNV;
                        }
                        else if (m_numTrailerExtensionTlvs) {
                            m_trailerRxState = LTP_TRAILER_RX_STATE::READ_ONE_TRAILER_EXTENSION_TAG_BYTE;
                            m_mainRxState = LTP_MAIN_RX_STATE::READ_TRAILER;
                        }
                        else {
                            //callback
                            m_mainRxState = LTP_MAIN_RX_STATE::READ_HEADER;
                            m_headerRxState = LTP_HEADER_RX_STATE::READ_CONTROL_BYTE;
                        }
                    }
                }
            }
        }
        else if (mainRxState == LTP_MAIN_RX_STATE::READ_REPORT_ACKNOWLEDGEMENT_SEGMENT_CONTENT) {
            m_sdnvTempVec.push_back(rxVal);
            if (m_sdnvTempVec.size() > 10) {
                std::cout << "error in LTP_MAIN_RX_STATE::READ_REPORT_ACKNOWLEDGEMENT_SEGMENT_CONTENT, sdnv > 10 bytes\n";
                m_mainRxState = LTP_MAIN_RX_STATE::READ_HEADER;
                m_headerRxState = LTP_HEADER_RX_STATE::READ_CONTROL_BYTE;
            }
            else if ((rxVal & 0x80) == 0) { //if msbit is a 0 then stop
                uint8_t sdnvSize;
                m_reportAcknowledgementSegment_reportSerialNumber = SdnvDecodeU64(m_sdnvTempVec.data(), &sdnvSize);
                if (sdnvSize != m_sdnvTempVec.size()) {
                    std::cout << "error in LTP_MAIN_RX_STATE::READ_REPORT_ACKNOWLEDGEMENT_SEGMENT_CONTENT, sdnvSize != m_sdnvTempVec.size()\n";
                    m_mainRxState = LTP_MAIN_RX_STATE::READ_HEADER;
                    m_headerRxState = LTP_HEADER_RX_STATE::READ_CONTROL_BYTE;
                }
                else if (m_numTrailerExtensionTlvs) {
                    m_trailerRxState = LTP_TRAILER_RX_STATE::READ_ONE_TRAILER_EXTENSION_TAG_BYTE;
                    m_mainRxState = LTP_MAIN_RX_STATE::READ_TRAILER;
                }
                else {
                    //callback
                    m_mainRxState = LTP_MAIN_RX_STATE::READ_HEADER;
                    m_headerRxState = LTP_HEADER_RX_STATE::READ_CONTROL_BYTE;
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
                m_mainRxState = LTP_MAIN_RX_STATE::READ_HEADER;
                m_headerRxState = LTP_HEADER_RX_STATE::READ_CONTROL_BYTE;
            }
        }
    }
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
}

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
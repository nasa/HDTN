#include "Tcpcl.h"
#include <boost/make_shared.hpp>
#include <boost/foreach.hpp>
#include <boost/endian/conversion.hpp>
#include <iostream>
#include "Sdnv.h"

Tcpcl::Tcpcl() : M_MAX_RX_BUNDLE_SIZE_BYTES(10000000) { //default 10MB unless changed by SetMaxReceiveBundleSizeBytes
    InitRx();
}
Tcpcl::~Tcpcl() {

}
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
void Tcpcl::SetMaxReceiveBundleSizeBytes(const uint64_t maxRxBundleSizeBytes) {
    M_MAX_RX_BUNDLE_SIZE_BYTES = maxRxBundleSizeBytes;
}
uint64_t Tcpcl::GetMaxReceiveBundleSizeBytes() const {
    return M_MAX_RX_BUNDLE_SIZE_BYTES;
}

void Tcpcl::InitRx() {
    m_mainRxState = TCPCL_MAIN_RX_STATE::READ_CONTACT_HEADER;
    m_contactHeaderRxState = TCPCL_CONTACT_HEADER_RX_STATE::READ_SYNC_1;
    m_keepAliveInterval = 0;
    m_sdnvTempVec.reserve(32); //critical for hardware accelerated decode (min size 16 (sizeof(__m128i)) to prevent out of bounds
    m_sdnvTempVec.resize(0);
    m_localEidLength = 0;
    m_localEidStr = "";
}

void Tcpcl::HandleReceivedChar(const uint8_t rxVal) {
    HandleReceivedChars(&rxVal, 1);
}

void Tcpcl::HandleReceivedChars(const uint8_t * rxVals, std::size_t numChars) {
    while (numChars) {
        --numChars;
        const uint8_t rxVal = *rxVals++;
        const TCPCL_MAIN_RX_STATE mainRxState = m_mainRxState; //const for optimization
        if (mainRxState == TCPCL_MAIN_RX_STATE::READ_CONTACT_HEADER) {
            const TCPCL_CONTACT_HEADER_RX_STATE contactHeaderRxState = m_contactHeaderRxState; //const for optimization
            //magic:  A four-byte field that always contains the byte sequence 0x64	0x74 0x6e 0x21,
            //i.e., the text string "dtn!" in US - ASCII.
            if (contactHeaderRxState == TCPCL_CONTACT_HEADER_RX_STATE::READ_SYNC_1) {
                if (rxVal == 0x64) { //'d'
                    m_contactHeaderRxState = TCPCL_CONTACT_HEADER_RX_STATE::READ_SYNC_2;
                }
            }
            else if (contactHeaderRxState == TCPCL_CONTACT_HEADER_RX_STATE::READ_SYNC_2) {
                if (rxVal == 0x74) { //'t'
                    m_contactHeaderRxState = TCPCL_CONTACT_HEADER_RX_STATE::READ_SYNC_3;
                }
                else if (rxVal != 0x64) { //error, but if 'd' remain in this state2 in case "ddt"
                    m_contactHeaderRxState = TCPCL_CONTACT_HEADER_RX_STATE::READ_SYNC_1;
                }
            }
            else if (contactHeaderRxState == TCPCL_CONTACT_HEADER_RX_STATE::READ_SYNC_3) {
                if (rxVal == 0x6e) { //'n'
                    m_contactHeaderRxState = TCPCL_CONTACT_HEADER_RX_STATE::READ_SYNC_4;
                }
                else if (rxVal == 0x64) { //error, but if 'd' goto state2 in case "dtd"
                    m_contactHeaderRxState = TCPCL_CONTACT_HEADER_RX_STATE::READ_SYNC_2;
                }
                else {
                    m_contactHeaderRxState = TCPCL_CONTACT_HEADER_RX_STATE::READ_SYNC_1;
                }
            }
            else if (contactHeaderRxState == TCPCL_CONTACT_HEADER_RX_STATE::READ_SYNC_4) {
                if (rxVal == 0x21) { //'!'
                    m_contactHeaderRxState = TCPCL_CONTACT_HEADER_RX_STATE::READ_VERSION;
                }
                else if (rxVal == 0x64) { //error, but if 'd' goto state2 in case "dtnd"
                    m_contactHeaderRxState = TCPCL_CONTACT_HEADER_RX_STATE::READ_SYNC_2;
                }
                else {
                    m_contactHeaderRxState = TCPCL_CONTACT_HEADER_RX_STATE::READ_SYNC_1;
                }
            }
            else if (contactHeaderRxState == TCPCL_CONTACT_HEADER_RX_STATE::READ_VERSION) {
                if (rxVal == TCPCL_VERSION) {
                    m_contactHeaderRxState = TCPCL_CONTACT_HEADER_RX_STATE::READ_FLAGS;
                }
                else if (rxVal == 0x64) { //error, but if 'd' goto state2 in case "dtn!d"
                    m_contactHeaderRxState = TCPCL_CONTACT_HEADER_RX_STATE::READ_SYNC_2;
                }
                else {
                    m_contactHeaderRxState = TCPCL_CONTACT_HEADER_RX_STATE::READ_SYNC_1;
                }
            }
            else if (contactHeaderRxState == TCPCL_CONTACT_HEADER_RX_STATE::READ_FLAGS) {
                m_contactHeaderFlags = static_cast<CONTACT_HEADER_FLAGS>(rxVal);
                m_contactHeaderRxState = TCPCL_CONTACT_HEADER_RX_STATE::READ_KEEPALIVE_INTERVAL_BYTE1;
            }
            else if (contactHeaderRxState == TCPCL_CONTACT_HEADER_RX_STATE::READ_KEEPALIVE_INTERVAL_BYTE1) { //msb
                m_keepAliveInterval = rxVal;
                m_keepAliveInterval <<= 8;
                m_contactHeaderRxState = TCPCL_CONTACT_HEADER_RX_STATE::READ_KEEPALIVE_INTERVAL_BYTE2;
            }
            else if (contactHeaderRxState == TCPCL_CONTACT_HEADER_RX_STATE::READ_KEEPALIVE_INTERVAL_BYTE2) {
                m_keepAliveInterval |= rxVal;
                m_sdnvTempVec.resize(0);
                m_localEidLength = 0;
                m_contactHeaderRxState = TCPCL_CONTACT_HEADER_RX_STATE::READ_LOCAL_EID_LENGTH_SDNV;
            }
            else if (contactHeaderRxState == TCPCL_CONTACT_HEADER_RX_STATE::READ_LOCAL_EID_LENGTH_SDNV) {
                m_sdnvTempVec.push_back(rxVal);
                if (m_sdnvTempVec.size() > 10) {
                    std::cout << "error in TCPCL_CONTACT_HEADER_RX_STATE::READ_LOCAL_EID_LENGTH_SDNV, sdnv > 10 bytes\n";
                    m_contactHeaderRxState = TCPCL_CONTACT_HEADER_RX_STATE::READ_SYNC_1;
                }
                else if ((rxVal & 0x80) == 0) { //if msbit is a 0 then stop
                    uint8_t sdnvSize;
                    m_localEidLength = SdnvDecodeU64(m_sdnvTempVec.data(), &sdnvSize);
                    if (sdnvSize == 0) {
                        std::cout << "error in TCPCL_CONTACT_HEADER_RX_STATE::READ_LOCAL_EID_LENGTH_SDNV, sdnvSize is 0\n";
                        m_contactHeaderRxState = TCPCL_CONTACT_HEADER_RX_STATE::READ_SYNC_1;
                    }
                    else if (sdnvSize != m_sdnvTempVec.size()) {
                        std::cout << "error in TCPCL_CONTACT_HEADER_RX_STATE::READ_LOCAL_EID_LENGTH_SDNV, sdnvSize != m_sdnvTempVec.size()\n";
                        m_contactHeaderRxState = TCPCL_CONTACT_HEADER_RX_STATE::READ_SYNC_1;
                    }
                    else {
                        m_localEidStr.resize(0);
                        m_localEidStr.reserve(m_localEidLength);
                        m_contactHeaderRxState = TCPCL_CONTACT_HEADER_RX_STATE::READ_LOCAL_EID_STRING;
                    }
                }
            }
            else if (contactHeaderRxState == TCPCL_CONTACT_HEADER_RX_STATE::READ_LOCAL_EID_STRING) {
                m_localEidStr.push_back(rxVal);
                if (m_localEidStr.size() == m_localEidLength) {
                    m_contactHeaderRxState = TCPCL_CONTACT_HEADER_RX_STATE::READ_SYNC_1;
                    m_mainRxState = TCPCL_MAIN_RX_STATE::READ_MESSAGE_TYPE_BYTE;
                    if (m_contactHeaderReadCallback) {
                        m_contactHeaderReadCallback(m_contactHeaderFlags, m_keepAliveInterval, m_localEidStr);
                    }
                }
            }
        }
        else if (mainRxState == TCPCL_MAIN_RX_STATE::READ_MESSAGE_TYPE_BYTE) {
            //the ietf is confusing.. please ignore the bit numbering in the ietf
            //m_messageTypeByte = static_cast<MESSAGE_TYPE_BYTE_CODES>(rxVal & 0x0f);
            //m_messageTypeFlags = rxVal & 0xf0;
            m_messageTypeByte = static_cast<MESSAGE_TYPE_BYTE_CODES>((rxVal >> 4) & 0x0f);
            m_messageTypeFlags = rxVal & 0x0f;
            if (m_messageTypeByte == MESSAGE_TYPE_BYTE_CODES::DATA_SEGMENT) {
                m_dataSegmentStartFlag = ((m_messageTypeFlags & (1U << 1)) != 0);
                m_dataSegmentEndFlag = ((m_messageTypeFlags & (1U << 0)) != 0);
                m_sdnvTempVec.resize(0);
                m_dataSegmentLength = 0;
                m_mainRxState = TCPCL_MAIN_RX_STATE::READ_DATA_SEGMENT;
                if (numChars >= 16) { //shortcut/optimization to avoid reading populating m_sdnvTempVec, just decode from rxVals if there's enough bytes remaining 
                    uint8_t sdnvSize;
                    m_dataSegmentLength = SdnvDecodeU64(rxVals, &sdnvSize);
                    if (sdnvSize == 0) {
                        std::cout << "error in TCPCL_MAIN_RX_STATE::READ_MESSAGE_TYPE_BYTE (shortcut READ_CONTENT_LENGTH_SDNV), sdnvSize is 0\n";
                        m_contactHeaderRxState = TCPCL_CONTACT_HEADER_RX_STATE::READ_SYNC_1;
                        m_mainRxState = TCPCL_MAIN_RX_STATE::READ_CONTACT_HEADER;
                    }
                    else {
                        numChars -= sdnvSize;
                        rxVals += sdnvSize;
                        m_dataSegmentDataVec.resize(0);
                        m_dataSegmentDataVec.reserve(m_dataSegmentLength);
                        //std::cout << "tcpcl sdnv shortcut" << std::endl;
                        m_dataSegmentRxState = TCPCL_DATA_SEGMENT_RX_STATE::READ_CONTENTS;
                    }
                }
                else { //not enough bytes, populate m_sdnvTempVec and then decode sdnv
                    //std::cout << "skipping tcpcl sdnv shortcut" << std::endl;
                    m_dataSegmentRxState = TCPCL_DATA_SEGMENT_RX_STATE::READ_CONTENT_LENGTH_SDNV;
                }
            }
            else if (m_messageTypeByte == MESSAGE_TYPE_BYTE_CODES::ACK_SEGMENT) {
                m_sdnvTempVec.resize(0);
                m_ackSegmentLength = 0;
                m_mainRxState = TCPCL_MAIN_RX_STATE::READ_ACK_SEGMENT;
            }
            else if (m_messageTypeByte == MESSAGE_TYPE_BYTE_CODES::REFUSE_BUNDLE) {
                m_bundleRefusalCode = m_messageTypeFlags;// (m_messageTypeFlags >> 4);
                if (m_bundleRefusalCallback) {
                    m_bundleRefusalCallback(static_cast<BUNDLE_REFUSAL_CODES>(m_bundleRefusalCode));
                }
                //remain in state TCPCL_MAIN_RX_STATE::READ_MESSAGE_TYPE_BYTE
            }
            else if (m_messageTypeByte == MESSAGE_TYPE_BYTE_CODES::LENGTH) {
                m_sdnvTempVec.resize(0);
                m_nextBundleLength = 0;
                m_mainRxState = TCPCL_MAIN_RX_STATE::READ_LENGTH_SEGMENT;
            }
            else if (m_messageTypeByte == MESSAGE_TYPE_BYTE_CODES::KEEPALIVE) {
                if (m_keepAliveCallback) {
                    m_keepAliveCallback();
                }
                //remain in state TCPCL_MAIN_RX_STATE::READ_MESSAGE_TYPE_BYTE
            }
            else if (m_messageTypeByte == MESSAGE_TYPE_BYTE_CODES::SHUTDOWN) {
                m_shutdownHasReasonFlag = ((m_messageTypeFlags & (1U << 1)) != 0);
                m_shutdownHasReconnectionDelayFlag = ((m_messageTypeFlags & (1U << 0)) != 0);
                m_sdnvTempVec.resize(0);
                m_shutdownReconnectionDelay = 0;
                if (m_shutdownHasReasonFlag) {
                    m_mainRxState = TCPCL_MAIN_RX_STATE::READ_SHUTDOWN_SEGMENT_REASON_CODE;
                }
                else if (m_shutdownHasReconnectionDelayFlag) {
                    m_mainRxState = TCPCL_MAIN_RX_STATE::READ_SHUTDOWN_SEGMENT_RECONNECTION_DELAY_SDNV;
                }
                else {
                    //full shutdown (no reason or reconnection delay).. back to beginning
                    m_contactHeaderRxState = TCPCL_CONTACT_HEADER_RX_STATE::READ_SYNC_1;
                    m_mainRxState = TCPCL_MAIN_RX_STATE::READ_CONTACT_HEADER;
                    if (m_shutdownMessageCallback) {
                        m_shutdownMessageCallback(m_shutdownHasReasonFlag, SHUTDOWN_REASON_CODES::UNASSIGNED, m_shutdownHasReconnectionDelayFlag, 0);
                    }
                }

            }
        }
        else if (mainRxState == TCPCL_MAIN_RX_STATE::READ_DATA_SEGMENT) {
            if (m_dataSegmentRxState == TCPCL_DATA_SEGMENT_RX_STATE::READ_CONTENT_LENGTH_SDNV) {
                m_sdnvTempVec.push_back(rxVal);
                if (m_sdnvTempVec.size() > 10) {
                    std::cout << "error in TCPCL_DATA_SEGMENT_RX_STATE::READ_CONTENT_LENGTH_SDNV, sdnv > 10 bytes\n";
                    m_contactHeaderRxState = TCPCL_CONTACT_HEADER_RX_STATE::READ_SYNC_1;
                    m_mainRxState = TCPCL_MAIN_RX_STATE::READ_CONTACT_HEADER;
                }
                else if ((rxVal & 0x80) == 0) { //if msbit is a 0 then stop
                    uint8_t sdnvSize;
                    m_dataSegmentLength = SdnvDecodeU64(m_sdnvTempVec.data(), &sdnvSize);
                    if (sdnvSize == 0) {
                        std::cout << "error in TCPCL_DATA_SEGMENT_RX_STATE::READ_CONTENT_LENGTH_SDNV, sdnvSize is 0\n";
                        m_contactHeaderRxState = TCPCL_CONTACT_HEADER_RX_STATE::READ_SYNC_1;
                        m_mainRxState = TCPCL_MAIN_RX_STATE::READ_CONTACT_HEADER;
                    }
                    else if (sdnvSize != m_sdnvTempVec.size()) {
                        std::cout << "error in TCPCL_DATA_SEGMENT_RX_STATE::READ_CONTENT_LENGTH_SDNV, sdnvSize != m_sdnvTempVec.size()\n";
                        m_contactHeaderRxState = TCPCL_CONTACT_HEADER_RX_STATE::READ_SYNC_1;
                        m_mainRxState = TCPCL_MAIN_RX_STATE::READ_CONTACT_HEADER;
                    }
                    else if (m_dataSegmentLength > M_MAX_RX_BUNDLE_SIZE_BYTES) {
                        std::cout << "error in TCPCL_DATA_SEGMENT_RX_STATE::READ_CONTENT_LENGTH_SDNV, data segment length ("
                            << m_dataSegmentLength << " bytes) is greater than the bundle size limit of " << M_MAX_RX_BUNDLE_SIZE_BYTES << " bytes\n";
                        m_contactHeaderRxState = TCPCL_CONTACT_HEADER_RX_STATE::READ_SYNC_1;
                        m_mainRxState = TCPCL_MAIN_RX_STATE::READ_CONTACT_HEADER;
                    }
                    else {
                        m_dataSegmentDataVec.resize(0);
                        m_dataSegmentDataVec.reserve(m_dataSegmentLength);
                        //std::cout << "l " << m_dataSegmentLength << std::endl;
                        m_dataSegmentRxState = TCPCL_DATA_SEGMENT_RX_STATE::READ_CONTENTS;
                    }
                }
            }
            else if (m_dataSegmentRxState == TCPCL_DATA_SEGMENT_RX_STATE::READ_CONTENTS) {
                m_dataSegmentDataVec.push_back(rxVal);
                if (m_dataSegmentDataVec.size() == m_dataSegmentLength) {
                    m_mainRxState = TCPCL_MAIN_RX_STATE::READ_MESSAGE_TYPE_BYTE;
                    if (m_dataSegmentContentsReadCallback) {
                        m_dataSegmentContentsReadCallback(m_dataSegmentDataVec, m_dataSegmentStartFlag, m_dataSegmentEndFlag);
                    }
                }
                else {
                    const std::size_t bytesRemainingToCopy = m_dataSegmentLength - m_dataSegmentDataVec.size(); //guaranteed to be at least 1 from "if" above
                    const std::size_t bytesToCopy = std::min(numChars, bytesRemainingToCopy - 1); //leave last byte to go through the state machine
                    if (bytesToCopy) {
                        m_dataSegmentDataVec.insert(m_dataSegmentDataVec.end(), rxVals, rxVals + bytesToCopy); //concatenate
                        rxVals += bytesToCopy;
                        numChars -= bytesToCopy;
                    }
                }
            }
        }
        else if (mainRxState == TCPCL_MAIN_RX_STATE::READ_ACK_SEGMENT) {
            //no intermediate states in here, just an sdnv to read
            m_sdnvTempVec.push_back(rxVal);
            if (m_sdnvTempVec.size() > 10) {
                std::cout << "error in TCPCL_MAIN_RX_STATE::READ_ACK_SEGMENT, sdnv > 10 bytes\n";
                m_contactHeaderRxState = TCPCL_CONTACT_HEADER_RX_STATE::READ_SYNC_1;
                m_mainRxState = TCPCL_MAIN_RX_STATE::READ_CONTACT_HEADER;
            }
            else if ((rxVal & 0x80) == 0) { //if msbit is a 0 then stop
                uint8_t sdnvSize;
                m_ackSegmentLength = SdnvDecodeU64(m_sdnvTempVec.data(), &sdnvSize);
                if (sdnvSize == 0) {
                    std::cout << "error in TCPCL_MAIN_RX_STATE::READ_ACK_SEGMENT, sdnvSize is 0\n";
                    m_contactHeaderRxState = TCPCL_CONTACT_HEADER_RX_STATE::READ_SYNC_1;
                    m_mainRxState = TCPCL_MAIN_RX_STATE::READ_CONTACT_HEADER;
                }
                else if (sdnvSize != m_sdnvTempVec.size()) {
                    std::cout << "error in TCPCL_MAIN_RX_STATE::READ_ACK_SEGMENT, sdnvSize != m_sdnvTempVec.size()\n";
                    m_contactHeaderRxState = TCPCL_CONTACT_HEADER_RX_STATE::READ_SYNC_1;
                    m_mainRxState = TCPCL_MAIN_RX_STATE::READ_CONTACT_HEADER;
                }
                else {
                    m_mainRxState = TCPCL_MAIN_RX_STATE::READ_MESSAGE_TYPE_BYTE;
                    if (m_ackSegmentReadCallback) {
                        m_ackSegmentReadCallback(m_ackSegmentLength);
                    }
                }

            }

        }

        else if (mainRxState == TCPCL_MAIN_RX_STATE::READ_LENGTH_SEGMENT) {
            //no intermediate states in here, just an sdnv to read
            m_sdnvTempVec.push_back(rxVal);
            if (m_sdnvTempVec.size() > 10) {
                std::cout << "error in TCPCL_MAIN_RX_STATE::READ_LENGTH_SEGMENT, sdnv > 10 bytes\n";
                m_contactHeaderRxState = TCPCL_CONTACT_HEADER_RX_STATE::READ_SYNC_1;
                m_mainRxState = TCPCL_MAIN_RX_STATE::READ_CONTACT_HEADER;
            }
            else if ((rxVal & 0x80) == 0) { //if msbit is a 0 then stop
                uint8_t sdnvSize;
                m_nextBundleLength = SdnvDecodeU64(m_sdnvTempVec.data(), &sdnvSize);
                if (sdnvSize == 0) {
                    std::cout << "error in TCPCL_MAIN_RX_STATE::READ_LENGTH_SEGMENT, sdnvSize is 0\n";
                    m_contactHeaderRxState = TCPCL_CONTACT_HEADER_RX_STATE::READ_SYNC_1;
                    m_mainRxState = TCPCL_MAIN_RX_STATE::READ_CONTACT_HEADER;
                }
                else if (sdnvSize != m_sdnvTempVec.size()) {
                    std::cout << "error in TCPCL_MAIN_RX_STATE::READ_LENGTH_SEGMENT, sdnvSize != m_sdnvTempVec.size()\n";
                    m_contactHeaderRxState = TCPCL_CONTACT_HEADER_RX_STATE::READ_SYNC_1;
                    m_mainRxState = TCPCL_MAIN_RX_STATE::READ_CONTACT_HEADER;
                }
                else {
                    m_mainRxState = TCPCL_MAIN_RX_STATE::READ_MESSAGE_TYPE_BYTE;
                    if (m_nextBundleLengthCallback) {
                        m_nextBundleLengthCallback(m_nextBundleLength);
                    }
                }

            }

        }
        else if (mainRxState == TCPCL_MAIN_RX_STATE::READ_SHUTDOWN_SEGMENT_REASON_CODE) {
            m_shutdownReasonCode = static_cast<SHUTDOWN_REASON_CODES>(rxVal);
            if (m_shutdownHasReconnectionDelayFlag) {
                m_mainRxState = TCPCL_MAIN_RX_STATE::READ_SHUTDOWN_SEGMENT_RECONNECTION_DELAY_SDNV;
            }
            else {
                //full shutdown with reason code, but no reconnection delay.. back to beginning
                m_contactHeaderRxState = TCPCL_CONTACT_HEADER_RX_STATE::READ_SYNC_1;
                m_mainRxState = TCPCL_MAIN_RX_STATE::READ_CONTACT_HEADER;
                if (m_shutdownMessageCallback) {
                    m_shutdownMessageCallback(m_shutdownHasReasonFlag, m_shutdownReasonCode, m_shutdownHasReconnectionDelayFlag, 0);
                }
            }
        }
        else if (mainRxState == TCPCL_MAIN_RX_STATE::READ_SHUTDOWN_SEGMENT_RECONNECTION_DELAY_SDNV) {
            //no intermediate states in here, just an sdnv to read
            m_sdnvTempVec.push_back(rxVal);
            if (m_sdnvTempVec.size() > 10) {
                std::cout << "error in TCPCL_MAIN_RX_STATE::READ_SHUTDOWN_SEGMENT_RECONNECTION_DELAY_SDNV, sdnv > 10 bytes\n";
                m_contactHeaderRxState = TCPCL_CONTACT_HEADER_RX_STATE::READ_SYNC_1;
                m_mainRxState = TCPCL_MAIN_RX_STATE::READ_CONTACT_HEADER;
            }
            else if ((rxVal & 0x80) == 0) { //if msbit is a 0 then stop
                uint8_t sdnvSize;
                m_shutdownReconnectionDelay = SdnvDecodeU64(m_sdnvTempVec.data(), &sdnvSize);
                if (sdnvSize == 0) {
                    std::cout << "error in TCPCL_MAIN_RX_STATE::READ_SHUTDOWN_SEGMENT_RECONNECTION_DELAY_SDNV, sdnvSize is 0\n";
                    m_contactHeaderRxState = TCPCL_CONTACT_HEADER_RX_STATE::READ_SYNC_1;
                    m_mainRxState = TCPCL_MAIN_RX_STATE::READ_CONTACT_HEADER;
                }
                else if (sdnvSize != m_sdnvTempVec.size()) {
                    std::cout << "error in TCPCL_MAIN_RX_STATE::READ_SHUTDOWN_SEGMENT_RECONNECTION_DELAY_SDNV, sdnvSize != m_sdnvTempVec.size()\n";
                    m_contactHeaderRxState = TCPCL_CONTACT_HEADER_RX_STATE::READ_SYNC_1;
                    m_mainRxState = TCPCL_MAIN_RX_STATE::READ_CONTACT_HEADER;
                }
                else {
                    //full shutdown.. back to beginning
                    m_contactHeaderRxState = TCPCL_CONTACT_HEADER_RX_STATE::READ_SYNC_1;
                    m_mainRxState = TCPCL_MAIN_RX_STATE::READ_CONTACT_HEADER;
                    if (m_shutdownMessageCallback) {
                        m_shutdownMessageCallback(m_shutdownHasReasonFlag, m_shutdownReasonCode, m_shutdownHasReconnectionDelayFlag, m_shutdownReconnectionDelay);
                    }
                }

            }
        }
    }
}

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

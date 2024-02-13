/**
 * @file TcpclV4.cpp
 * @author  Brian Tomko <brian.j.tomko@nasa.gov>
 *
 * @copyright Copyright (c) 2021 United States Government as represented by
 * the National Aeronautics and Space Administration.
 * No copyright is claimed in the United States under Title 17, U.S.Code.
 * All Other Rights Reserved.
 *
 * @section LICENSE
 * Released under the NASA Open Source Agreement (NOSA)
 * See LICENSE.md in the source root directory for more information.
 */

#include "TcpclV4.h"
#include "Logger.h"
#include <boost/multiprecision/cpp_int.hpp>
#include <boost/multiprecision/detail/bitscan.hpp>
#include <boost/endian/conversion.hpp>
#ifdef USE_SSE_SSE2
#include <immintrin.h>
#include <emmintrin.h>
#endif


static constexpr hdtn::Logger::SubProcess subprocess = hdtn::Logger::SubProcess::none;

static BOOST_FORCEINLINE uint64_t UnalignedBigEndianToNativeU64(const uint8_t * const data) {
#ifndef USE_SSE_SSE2
    uint64_t result64Be;
    uint8_t * const result64BeAs8Ptr = (uint8_t*)&result64Be;
    result64BeAs8Ptr[0] = data[0];
    result64BeAs8Ptr[1] = data[1];
    result64BeAs8Ptr[2] = data[2];
    result64BeAs8Ptr[3] = data[3];
    result64BeAs8Ptr[4] = data[4];
    result64BeAs8Ptr[5] = data[5];
    result64BeAs8Ptr[6] = data[6];
    result64BeAs8Ptr[7] = data[7];
    return boost::endian::big_to_native(result64Be);
#elif 1
    const __m128i enc = _mm_castpd_si128(_mm_load_sd((double const*)(data))); //Load a double-precision (64-bit) floating-point element from memory into the lower of dst, and zero the upper element. mem_addr does not need to be aligned on any particular boundary.
    const uint64_t result64Be = _mm_cvtsi128_si64(enc); //SSE2 Copy the lower 64-bit integer in a to dst.
    return boost::endian::big_to_native(result64Be);
#else
    return static_cast<uint64_t>(_loadbe_i64(data));
#endif
}

static BOOST_FORCEINLINE void NativeU64ToUnalignedBigEndian(uint8_t * const output, uint64_t nativeValue) {
    const uint64_t valueBe = boost::endian::native_to_big(nativeValue);
#ifndef USE_SSE_SSE2
    uint8_t * const valueBeAs8Ptr = (uint8_t*)&valueBe;
    output[0] = valueBeAs8Ptr[0];
    output[1] = valueBeAs8Ptr[1];
    output[2] = valueBeAs8Ptr[2];
    output[3] = valueBeAs8Ptr[3];
    output[4] = valueBeAs8Ptr[4];
    output[5] = valueBeAs8Ptr[5];
    output[6] = valueBeAs8Ptr[6];
    output[7] = valueBeAs8Ptr[7];
#else
    _mm_stream_si64((long long int *)output, valueBe);
#endif
}

static BOOST_FORCEINLINE uint32_t UnalignedBigEndianToNativeU32(const uint8_t * const data) {
#ifndef USE_SSE_SSE2
    uint32_t result32Be;
    uint8_t * const result32BeAs8Ptr = (uint8_t*)&result32Be;
    result32BeAs8Ptr[0] = data[0];
    result32BeAs8Ptr[1] = data[1];
    result32BeAs8Ptr[2] = data[2];
    result32BeAs8Ptr[3] = data[3];
    return boost::endian::big_to_native(result32Be);
#elif 1
    const __m128i enc = _mm_castps_si128(_mm_load_ss((float const*)(data))); //Load a single-precision (32-bit) floating-point element from memory into the lower of dst, and zero the upper 3 elements. mem_addr does not need to be aligned on any particular boundary.
    const uint32_t result32Be = _mm_cvtsi128_si32(enc); //SSE2 Copy the lower 32-bit integer in a to dst.
    return boost::endian::big_to_native(result32Be);
#else
    return static_cast<uint32_t>(_loadbe_i32(data));
#endif
}

static BOOST_FORCEINLINE void NativeU32ToUnalignedBigEndian(uint8_t * const output, uint32_t nativeValue) {
    const uint32_t valueBe = boost::endian::native_to_big(nativeValue);
#ifndef USE_SSE_SSE2
    uint8_t * const valueBeAs8Ptr = (uint8_t*)&valueBe;
    output[0] = valueBeAs8Ptr[0];
    output[1] = valueBeAs8Ptr[1];
    output[2] = valueBeAs8Ptr[2];
    output[3] = valueBeAs8Ptr[3];
#else
    _mm_stream_si32((int32_t *)output, valueBe);
#endif
}

static BOOST_FORCEINLINE void ClearFourBytes(uint8_t * const output) {
#ifndef USE_SSE_SSE2
    output[0] = 0;
    output[1] = 0;
    output[2] = 0;
    output[3] = 0;
#else
    _mm_stream_si32((int32_t *)output, 0);
#endif
}

static BOOST_FORCEINLINE uint16_t UnalignedBigEndianToNativeU16(const uint8_t * const data) {
#if 1
    return ((static_cast<uint16_t>(data[0])) << 8) | data[1];
#else
    return static_cast<uint16_t>(_loadbe_i16(data));
#endif
}

static BOOST_FORCEINLINE void NativeU16ToUnalignedBigEndian(uint8_t * const output, uint16_t nativeValue) {
    output[0] = (uint8_t)(nativeValue >> 8); //big endian most significant byte first
    output[1] = (uint8_t)nativeValue; //big endian least significant byte last
}

TcpclV4::tcpclv4_extension_t::tcpclv4_extension_t() : flags(0), type(0) { } //a default constructor: X()
TcpclV4::tcpclv4_extension_t::tcpclv4_extension_t(bool isCriticalFlag, uint16_t itemType, const std::vector<uint8_t> & valueAsVec) :
    flags(isCriticalFlag), type(itemType), valueVec(valueAsVec) {}
TcpclV4::tcpclv4_extension_t::tcpclv4_extension_t(bool isCriticalFlag, uint16_t itemType, std::vector<uint8_t> && valueAsVec) :
    flags(isCriticalFlag), type(itemType), valueVec(std::move(valueAsVec)) {}
TcpclV4::tcpclv4_extension_t::~tcpclv4_extension_t() { } //a destructor: ~X()
TcpclV4::tcpclv4_extension_t::tcpclv4_extension_t(const tcpclv4_extension_t& o) : flags(o.flags), type(o.type), valueVec(o.valueVec) { } //a copy constructor: X(const X&)
TcpclV4::tcpclv4_extension_t::tcpclv4_extension_t(tcpclv4_extension_t&& o) : flags(o.flags), type(o.type), valueVec(std::move(o.valueVec)) { } //a move constructor: X(X&&)
TcpclV4::tcpclv4_extension_t& TcpclV4::tcpclv4_extension_t::operator=(const tcpclv4_extension_t& o) { //a copy assignment: operator=(const X&)
    flags = o.flags;
    type = o.type;
    valueVec = o.valueVec;
    return *this;
}
TcpclV4::tcpclv4_extension_t& TcpclV4::tcpclv4_extension_t::operator=(tcpclv4_extension_t && o) { //a move assignment: operator=(X&&)
    flags = o.flags;
    type = o.type;
    valueVec = std::move(o.valueVec);
    return *this;
}
bool TcpclV4::tcpclv4_extension_t::operator==(const tcpclv4_extension_t & o) const {
    return (flags == o.flags) && (type == o.type) && (valueVec == o.valueVec);
}
bool TcpclV4::tcpclv4_extension_t::operator!=(const tcpclv4_extension_t & o) const {
    return (flags != o.flags) || (type != o.type) || (valueVec != o.valueVec);
}
bool TcpclV4::tcpclv4_extension_t::IsCritical() const {
    return ((flags & 1) != 0);
}
void TcpclV4::tcpclv4_extension_t::AppendSerialize(std::vector<uint8_t> & serialization) const {
    serialization.push_back(flags);
    serialization.push_back(static_cast<uint8_t>(type >> 8));
    serialization.push_back(static_cast<uint8_t>(type));
    serialization.push_back(static_cast<uint8_t>(valueVec.size() >> 8));
    serialization.push_back(static_cast<uint8_t>(valueVec.size()));

    //data copy valueVec
    serialization.insert(serialization.end(), valueVec.begin(), valueVec.end()); //concatenate
}
uint64_t TcpclV4::tcpclv4_extension_t::Serialize(uint8_t * serialization) const {
    *serialization++ = flags;
    *serialization++ = static_cast<uint8_t>(type >> 8);
    *serialization++ = static_cast<uint8_t>(type);
    *serialization++ = static_cast<uint8_t>(valueVec.size() >> 8);
    *serialization++ = static_cast<uint8_t>(valueVec.size());
    
    //data copy valueVec
    memcpy(serialization, valueVec.data(), valueVec.size()); //concatenate

    return 5 + valueVec.size();
}
uint64_t TcpclV4::tcpclv4_extension_t::SerializeTransferLengthExtension(uint8_t * serialization, const uint64_t totalLength) {
    //If a TCPCL entity receives a
    //Transfer Extension Item with an unknown Item Type and the CRITICAL
    //flag is 1, the entity SHALL refuse the transfer with an
    //XFER_REFUSE reason code of "Extension Failure".  If the CRITICAL
    //flag is 0, an entity SHALL skip over and ignore any item with an
    //unknown Item Type.
    *serialization++ = 1; //critical flag set

    //The Transfer Length extension SHALL be assigned transfer extension type ID 0x0001.
    *serialization++ = 0x00; //big endian
    *serialization++ = 0x01;

    //encode sizeof(uint64_t) = sizeof(totalLength) = 8 big endian as 16-bit
    *serialization++ = 0; //big endian
    *serialization++ = 8;

    //data copy valueVec
    NativeU64ToUnalignedBigEndian(serialization, totalLength);
    
    return SIZE_OF_SERIALIZED_TRANSFER_LENGTH_EXTENSION;
}

TcpclV4::tcpclv4_extensions_t::tcpclv4_extensions_t() { } //a default constructor: X()
TcpclV4::tcpclv4_extensions_t::~tcpclv4_extensions_t() { } //a destructor: ~X()
TcpclV4::tcpclv4_extensions_t::tcpclv4_extensions_t(const tcpclv4_extensions_t& o) : extensionsVec(o.extensionsVec) { } //a copy constructor: X(const X&)
TcpclV4::tcpclv4_extensions_t::tcpclv4_extensions_t(tcpclv4_extensions_t&& o) : extensionsVec(std::move(o.extensionsVec)) { } //a move constructor: X(X&&)
TcpclV4::tcpclv4_extensions_t& TcpclV4::tcpclv4_extensions_t::operator=(const tcpclv4_extensions_t& o) { //a copy assignment: operator=(const X&)
    extensionsVec = o.extensionsVec;
    return *this;
}
TcpclV4::tcpclv4_extensions_t& TcpclV4::tcpclv4_extensions_t::operator=(tcpclv4_extensions_t && o) { //a move assignment: operator=(X&&)
    extensionsVec = std::move(o.extensionsVec);
    return *this;
}
bool TcpclV4::tcpclv4_extensions_t::operator==(const tcpclv4_extensions_t & o) const {
    return (extensionsVec == o.extensionsVec);
}
bool TcpclV4::tcpclv4_extensions_t::operator!=(const tcpclv4_extensions_t & o) const {
    return (extensionsVec != o.extensionsVec);
}
void TcpclV4::tcpclv4_extensions_t::AppendSerialize(std::vector<uint8_t> & serialization) const {
    for (std::vector<tcpclv4_extension_t>::const_iterator it = extensionsVec.cbegin(); it != extensionsVec.cend(); ++it) {
        it->AppendSerialize(serialization);
    }
}
uint64_t TcpclV4::tcpclv4_extensions_t::Serialize(uint8_t * serialization) const {
    uint8_t * serializationBase = serialization;
    for (std::vector<tcpclv4_extension_t>::const_iterator it = extensionsVec.cbegin(); it != extensionsVec.cend(); ++it) {
        serialization += it->Serialize(serialization);
    }
    return serialization - serializationBase;
}
uint64_t TcpclV4::tcpclv4_extensions_t::GetTotalDataRequiredForSerialization() const {
    uint64_t maximumBytesRequired = extensionsVec.size() * 5; //5 bytes of flags, type, length
    for (std::vector<tcpclv4_extension_t>::const_iterator it = extensionsVec.cbegin(); it != extensionsVec.cend(); ++it) {
        maximumBytesRequired += it->valueVec.size();
    }
    return maximumBytesRequired;
}


TcpclV4::tcpclv4_ack_t::tcpclv4_ack_t() : isStartSegment(false), isEndSegment(false), transferId(0), totalBytesAcknowledged(0) { } //a default constructor: X()
TcpclV4::tcpclv4_ack_t::tcpclv4_ack_t(bool paramIsStartSegment, bool paramIsEndSegment, uint64_t paramTransferId, uint64_t paramTotalBytesAcknowledged) :
    isStartSegment(paramIsStartSegment), isEndSegment(paramIsEndSegment), transferId(paramTransferId), totalBytesAcknowledged(paramTotalBytesAcknowledged) {}
TcpclV4::tcpclv4_ack_t::~tcpclv4_ack_t() { } //a destructor: ~X()
TcpclV4::tcpclv4_ack_t::tcpclv4_ack_t(const tcpclv4_ack_t& o) :
    isStartSegment(o.isStartSegment), isEndSegment(o.isEndSegment), transferId(o.transferId), totalBytesAcknowledged(o.totalBytesAcknowledged) { } //a copy constructor: X(const X&)
TcpclV4::tcpclv4_ack_t::tcpclv4_ack_t(tcpclv4_ack_t&& o) :
    isStartSegment(o.isStartSegment), isEndSegment(o.isEndSegment), transferId(o.transferId), totalBytesAcknowledged(o.totalBytesAcknowledged) { } //a move constructor: X(X&&)
TcpclV4::tcpclv4_ack_t& TcpclV4::tcpclv4_ack_t::operator=(const tcpclv4_ack_t& o) { //a copy assignment: operator=(const X&)
    isStartSegment = o.isStartSegment;
    isEndSegment = o.isEndSegment;
    transferId = o.transferId;
    totalBytesAcknowledged = o.totalBytesAcknowledged;
    return *this;
}
TcpclV4::tcpclv4_ack_t& TcpclV4::tcpclv4_ack_t::operator=(tcpclv4_ack_t && o) { //a move assignment: operator=(X&&)
    isStartSegment = o.isStartSegment;
    isEndSegment = o.isEndSegment;
    transferId = o.transferId;
    totalBytesAcknowledged = o.totalBytesAcknowledged;
    return *this;
}
bool TcpclV4::tcpclv4_ack_t::operator==(const tcpclv4_ack_t & o) const {
    return (isStartSegment == o.isStartSegment) && (isEndSegment == o.isEndSegment) && (transferId == o.transferId) && (totalBytesAcknowledged == o.totalBytesAcknowledged);
}
bool TcpclV4::tcpclv4_ack_t::operator!=(const tcpclv4_ack_t & o) const {
    return (isStartSegment != o.isStartSegment) || (isEndSegment != o.isEndSegment) || (transferId != o.transferId) || (totalBytesAcknowledged != o.totalBytesAcknowledged);
}
std::ostream& operator<<(std::ostream& os, const TcpclV4::tcpclv4_ack_t & o) {
    os << "[isStart:" << o.isStartSegment << ", isEnd:" << o.isEndSegment << ", transferId:" << o.transferId << ", totalBytesAcknowledged:" << o.totalBytesAcknowledged << "]";
    return os;
}


TcpclV4::TcpclV4() : M_MAX_RX_BUNDLE_SIZE_BYTES(10000000) { //default 10MB unless changed by SetMaxReceiveBundleSizeBytes
    InitRx();
}
TcpclV4::~TcpclV4() {

}
void TcpclV4::SetDataSegmentContentsReadCallback(const DataSegmentContentsReadCallback_t & callback) {
    m_dataSegmentContentsReadCallback = callback;
}
void TcpclV4::SetContactHeaderReadCallback(const ContactHeaderReadCallback_t & callback) {
    m_contactHeaderReadCallback = callback;
}
void TcpclV4::SetSessionInitReadCallback(const SessionInitCallback_t & callback) {
    m_sessionInitCallback = callback;
}
void TcpclV4::SetAckSegmentReadCallback(const AckSegmentReadCallback_t & callback) {
    m_ackSegmentReadCallback = callback;
}
void TcpclV4::SetBundleRefusalCallback(const BundleRefusalCallback_t & callback) {
    m_bundleRefusalCallback = callback;
}
void TcpclV4::SetMessageRejectCallback(const MessageRejectCallback_t & callback) {
    m_messageRejectCallback = callback;
}
void TcpclV4::SetKeepAliveCallback(const KeepAliveCallback_t & callback) {
    m_keepAliveCallback = callback;
}
void TcpclV4::SetSessionTerminationMessageCallback(const SessionTerminationMessageCallback_t & callback) {
    m_sessionTerminationMessageCallback = callback;
}
void TcpclV4::SetMaxReceiveBundleSizeBytes(const uint64_t maxRxBundleSizeBytes) {
    M_MAX_RX_BUNDLE_SIZE_BYTES = maxRxBundleSizeBytes;
}

void TcpclV4::InitRx() {
    m_mainRxState = TCPCLV4_MAIN_RX_STATE::READ_CONTACT_HEADER;
    m_contactHeaderRxState = TCPCLV4_CONTACT_HEADER_RX_STATE::READ_SYNC_1;
}

void TcpclV4::HandleReceivedChar(const uint8_t rxVal) {
    HandleReceivedChars(&rxVal, 1);
}

void TcpclV4::HandleReceivedChars(const uint8_t * rxVals, std::size_t numChars) {
    while (numChars) {
        --numChars;
        const uint8_t rxVal = *rxVals++;
        const TCPCLV4_MAIN_RX_STATE mainRxState = m_mainRxState; //const for optimization
        if (mainRxState == TCPCLV4_MAIN_RX_STATE::READ_CONTACT_HEADER) {
            const TCPCLV4_CONTACT_HEADER_RX_STATE contactHeaderRxState = m_contactHeaderRxState; //const for optimization
            //magic:  A four-byte field that always contains the byte sequence 0x64	0x74 0x6e 0x21,
            //i.e., the text string "dtn!" in US - ASCII.
            if (contactHeaderRxState == TCPCLV4_CONTACT_HEADER_RX_STATE::READ_SYNC_1) {
                if (rxVal == 0x64) { //'d'
                    m_contactHeaderRxState = TCPCLV4_CONTACT_HEADER_RX_STATE::READ_SYNC_2;
                }
            }
            else if (contactHeaderRxState == TCPCLV4_CONTACT_HEADER_RX_STATE::READ_SYNC_2) {
                if (rxVal == 0x74) { //'t'
                    m_contactHeaderRxState = TCPCLV4_CONTACT_HEADER_RX_STATE::READ_SYNC_3;
                }
                else if (rxVal != 0x64) { //error, but if 'd' remain in this state2 in case "ddt"
                    m_contactHeaderRxState = TCPCLV4_CONTACT_HEADER_RX_STATE::READ_SYNC_1;
                }
            }
            else if (contactHeaderRxState == TCPCLV4_CONTACT_HEADER_RX_STATE::READ_SYNC_3) {
                if (rxVal == 0x6e) { //'n'
                    m_contactHeaderRxState = TCPCLV4_CONTACT_HEADER_RX_STATE::READ_SYNC_4;
                }
                else if (rxVal == 0x64) { //error, but if 'd' goto state2 in case "dtd"
                    m_contactHeaderRxState = TCPCLV4_CONTACT_HEADER_RX_STATE::READ_SYNC_2;
                }
                else {
                    m_contactHeaderRxState = TCPCLV4_CONTACT_HEADER_RX_STATE::READ_SYNC_1;
                }
            }
            else if (contactHeaderRxState == TCPCLV4_CONTACT_HEADER_RX_STATE::READ_SYNC_4) {
                if (rxVal == 0x21) { //'!'
                    m_contactHeaderRxState = TCPCLV4_CONTACT_HEADER_RX_STATE::READ_VERSION;
                }
                else if (rxVal == 0x64) { //error, but if 'd' goto state2 in case "dtnd"
                    m_contactHeaderRxState = TCPCLV4_CONTACT_HEADER_RX_STATE::READ_SYNC_2;
                }
                else {
                    m_contactHeaderRxState = TCPCLV4_CONTACT_HEADER_RX_STATE::READ_SYNC_1;
                }
            }
            else if (contactHeaderRxState == TCPCLV4_CONTACT_HEADER_RX_STATE::READ_VERSION) {
                if (rxVal == 4) {
                    m_contactHeaderRxState = TCPCLV4_CONTACT_HEADER_RX_STATE::READ_FLAGS;
                }
                else if (rxVal == 0x64) { //error, but if 'd' goto state2 in case "dtn!d"
                    m_contactHeaderRxState = TCPCLV4_CONTACT_HEADER_RX_STATE::READ_SYNC_2;
                }
                else {
                    m_contactHeaderRxState = TCPCLV4_CONTACT_HEADER_RX_STATE::READ_SYNC_1;
                }
            }
            else if (contactHeaderRxState == TCPCLV4_CONTACT_HEADER_RX_STATE::READ_FLAGS) {
                m_remoteHasEnabledTlsSecurity = ((rxVal & 1) != 0); //only contact header flag
                m_contactHeaderRxState = TCPCLV4_CONTACT_HEADER_RX_STATE::READ_SYNC_1;
                m_mainRxState = TCPCLV4_MAIN_RX_STATE::READ_MESSAGE_TYPE_BYTE;
                if (m_contactHeaderReadCallback) {
                    m_contactHeaderReadCallback(m_remoteHasEnabledTlsSecurity);
                }
            }
        }
        else if (mainRxState == TCPCLV4_MAIN_RX_STATE::READ_MESSAGE_TYPE_BYTE) {
            
            m_messageTypeByte = static_cast<TCPCLV4_MESSAGE_TYPE_BYTE_CODES>(rxVal);
            if (m_messageTypeByte == TCPCLV4_MESSAGE_TYPE_BYTE_CODES::XFER_SEGMENT) {
                m_mainRxState = TCPCLV4_MAIN_RX_STATE::READ_DATA_SEGMENT;
                m_dataSegmentRxState = TCPCLV4_DATA_SEGMENT_RX_STATE::READ_MESSAGE_FLAGS_BYTE;
            }
            else if (m_messageTypeByte == TCPCLV4_MESSAGE_TYPE_BYTE_CODES::XFER_ACK) {
                m_mainRxState = TCPCLV4_MAIN_RX_STATE::READ_ACK_SEGMENT;
                m_dataAckRxState = TCPCLV4_DATA_ACK_RX_STATE::READ_MESSAGE_FLAGS_BYTE;
            }
            else if (m_messageTypeByte == TCPCLV4_MESSAGE_TYPE_BYTE_CODES::MSG_REJECT) {
                m_mainRxState = TCPCLV4_MAIN_RX_STATE::READ_MESSAGE_REJECTION;
                m_messageRejectRxState = TCPCLV4_MESSAGE_REJECT_RX_STATE::READ_REASON_CODE_BYTE;
            }
            else if (m_messageTypeByte == TCPCLV4_MESSAGE_TYPE_BYTE_CODES::XFER_REFUSE) {
                m_mainRxState = TCPCLV4_MAIN_RX_STATE::READ_TRANSFER_REFUSAL;
                m_transferRefusalRxState = TCPCLV4_TRANSFER_REFUSAL_RX_STATE::READ_REASON_CODE_BYTE;
            }
            else if (m_messageTypeByte == TCPCLV4_MESSAGE_TYPE_BYTE_CODES::KEEPALIVE) {
                if (m_keepAliveCallback) {
                    m_keepAliveCallback();
                }
                //remain in state TCPCL_MAIN_RX_STATE::READ_MESSAGE_TYPE_BYTE
            }
            else if (m_messageTypeByte == TCPCLV4_MESSAGE_TYPE_BYTE_CODES::SESS_TERM) {
                m_mainRxState = TCPCLV4_MAIN_RX_STATE::READ_SESSION_TERMINATION;
                m_sessionTerminationRxState = TCPCLV4_SESSION_TERMINATION_RX_STATE::READ_MESSAGE_FLAGS_BYTE;
            }
            else if (m_messageTypeByte == TCPCLV4_MESSAGE_TYPE_BYTE_CODES::SESS_INIT) {
                m_mainRxState = TCPCLV4_MAIN_RX_STATE::READ_SESSION_INIT;
                m_sessionInitRxState = TCPCLV4_SESSION_INIT_RX_STATE::READ_KEEPALIVE_INTERVAL_U16;
                m_readValueByteIndex = 0;
            }
        }
        else if (mainRxState == TCPCLV4_MAIN_RX_STATE::READ_DATA_SEGMENT) {
            if (m_dataSegmentRxState == TCPCLV4_DATA_SEGMENT_RX_STATE::READ_MESSAGE_FLAGS_BYTE) {
                m_messageFlags = rxVal;
                m_dataSegmentStartFlag = ((m_messageFlags & (1U << 1)) != 0);
                m_dataSegmentEndFlag = ((m_messageFlags & (1U << 0)) != 0);

                if (numChars >= sizeof(uint64_t)) { //shortcut/optimization to read transfer id (u64) now 
                    m_transferId = UnalignedBigEndianToNativeU64(rxVals);
                    numChars -= sizeof(uint64_t);
                    rxVals += sizeof(uint64_t);
                    if (m_dataSegmentStartFlag) {
                        if (numChars >= sizeof(uint32_t)) { //shortcut/optimization to read transfer extension items length (u32) now 
                            m_transferExtensionItemsLengthBytes = UnalignedBigEndianToNativeU32(rxVals);
                            numChars -= sizeof(uint32_t);
                            rxVals += sizeof(uint32_t);
                            m_transferExtensions.extensionsVec.clear();
                            if (m_transferExtensionItemsLengthBytes) { //optimizations cease at this point if there are transfer extensions to read
                                m_transferExtensions.extensionsVec.reserve(5); //todo
                                m_currentCountOfTransferExtensionEncodedBytes = 0;
                                m_dataSegmentRxState = TCPCLV4_DATA_SEGMENT_RX_STATE::READ_ONE_START_SEGMENT_TRANSFER_EXTENSION_ITEM_FLAG;
                                continue;
                            }
                            //else try the shortcut/optimization to read data length (u64) in if statement below
                        }
                        else { //not enough bytes to read transfer extension items length (u32) now 
                            m_dataSegmentRxState = TCPCLV4_DATA_SEGMENT_RX_STATE::READ_START_SEGMENT_TRANSFER_EXTENSION_ITEMS_LENGTH_U32;
                            m_readValueByteIndex = 0;
                            continue; //prevent from entering if statement below
                        }
                    }

                    if (numChars >= sizeof(uint64_t)) { //shortcut/optimization to read data length (u64) now 
                        m_dataSegmentLength = UnalignedBigEndianToNativeU64(rxVals);

                        numChars -= sizeof(uint64_t);
                        rxVals += sizeof(uint64_t);
                        if ((m_dataSegmentLength > M_MAX_RX_BUNDLE_SIZE_BYTES) || (m_dataSegmentLength == 0)) {
                            LOG_ERROR(subprocess) << "TCPCLV4_DATA_SEGMENT_RX_STATE::READ_DATA_LENGTH_U64 shortcut, data segment length ("
                                << m_dataSegmentLength << " bytes) is not between 1 and the bundle size limit of " << M_MAX_RX_BUNDLE_SIZE_BYTES << " bytes";
                            m_contactHeaderRxState = TCPCLV4_CONTACT_HEADER_RX_STATE::READ_SYNC_1;
                            m_mainRxState = TCPCLV4_MAIN_RX_STATE::READ_CONTACT_HEADER;
                        }
                        else {
                            m_dataSegmentDataVec.resize(0);
                            m_dataSegmentDataVec.reserve(m_dataSegmentLength);
                            m_dataSegmentRxState = TCPCLV4_DATA_SEGMENT_RX_STATE::READ_DATA_CONTENTS;
                        }

                    }
                    else { //not enough bytes to read data length (u64) now 
                        m_dataSegmentRxState = TCPCLV4_DATA_SEGMENT_RX_STATE::READ_DATA_LENGTH_U64;
                        m_readValueByteIndex = 0;
                    }
                }
                else { //not enough bytes to read transfer id (u64) now 
                    m_dataSegmentRxState = TCPCLV4_DATA_SEGMENT_RX_STATE::READ_TRANSFER_ID_U64;
                    m_readValueByteIndex = 0;
                }
            }
            else if (m_dataSegmentRxState == TCPCLV4_DATA_SEGMENT_RX_STATE::READ_TRANSFER_ID_U64) {
                (reinterpret_cast<uint8_t*>(&m_transferId))[m_readValueByteIndex++] = rxVal; 
                if (m_readValueByteIndex == sizeof(m_transferId)) {
                    boost::endian::big_to_native_inplace(m_transferId);
                    if (m_dataSegmentStartFlag) {
                        m_dataSegmentRxState = TCPCLV4_DATA_SEGMENT_RX_STATE::READ_START_SEGMENT_TRANSFER_EXTENSION_ITEMS_LENGTH_U32;
                    }
                    else {
                        m_dataSegmentRxState = TCPCLV4_DATA_SEGMENT_RX_STATE::READ_DATA_LENGTH_U64;
                    }
                    m_readValueByteIndex = 0;
                }
            }
            else if (m_dataSegmentRxState == TCPCLV4_DATA_SEGMENT_RX_STATE::READ_START_SEGMENT_TRANSFER_EXTENSION_ITEMS_LENGTH_U32) {
                (reinterpret_cast<uint8_t*>(&m_transferExtensionItemsLengthBytes))[m_readValueByteIndex++] = rxVal; 
                if (m_readValueByteIndex == sizeof(m_transferExtensionItemsLengthBytes)) {
                    m_transferExtensions.extensionsVec.clear();
                    if (m_transferExtensionItemsLengthBytes) {
                        boost::endian::big_to_native_inplace(m_transferExtensionItemsLengthBytes);
                        m_transferExtensions.extensionsVec.reserve(5); //todo
                        m_currentCountOfTransferExtensionEncodedBytes = 0;
                        m_dataSegmentRxState = TCPCLV4_DATA_SEGMENT_RX_STATE::READ_ONE_START_SEGMENT_TRANSFER_EXTENSION_ITEM_FLAG;
                    }
                    else {
                        m_dataSegmentRxState = TCPCLV4_DATA_SEGMENT_RX_STATE::READ_DATA_LENGTH_U64;
                        m_readValueByteIndex = 0;
                    }
                }
            }
            else if (m_dataSegmentRxState == TCPCLV4_DATA_SEGMENT_RX_STATE::READ_DATA_LENGTH_U64) {
                (reinterpret_cast<uint8_t*>(&m_dataSegmentLength))[m_readValueByteIndex++] = rxVal; 
                if (m_readValueByteIndex == sizeof(m_dataSegmentLength)) {
                    m_readValueByteIndex = 0;
                    boost::endian::big_to_native_inplace(m_dataSegmentLength);
                    if ((m_dataSegmentLength > M_MAX_RX_BUNDLE_SIZE_BYTES) || (m_dataSegmentLength == 0)) {
                        LOG_ERROR(subprocess) << "TCPCLV4_DATA_SEGMENT_RX_STATE::READ_DATA_LENGTH_U64, data segment length ("
                            << m_dataSegmentLength << " bytes) is not between 1 and the bundle size limit of " << M_MAX_RX_BUNDLE_SIZE_BYTES << " bytes";
                        m_contactHeaderRxState = TCPCLV4_CONTACT_HEADER_RX_STATE::READ_SYNC_1;
                        m_mainRxState = TCPCLV4_MAIN_RX_STATE::READ_CONTACT_HEADER;
                    }
                    else {
                        m_dataSegmentDataVec.resize(0);
                        m_dataSegmentDataVec.reserve(m_dataSegmentLength);
                        m_dataSegmentRxState = TCPCLV4_DATA_SEGMENT_RX_STATE::READ_DATA_CONTENTS;
                    }
                }
            }
            else if (m_dataSegmentRxState == TCPCLV4_DATA_SEGMENT_RX_STATE::READ_DATA_CONTENTS) {
                m_dataSegmentDataVec.push_back(rxVal);
                if (m_dataSegmentDataVec.size() == m_dataSegmentLength) {
                    m_mainRxState = TCPCLV4_MAIN_RX_STATE::READ_MESSAGE_TYPE_BYTE;
                    if (m_dataSegmentContentsReadCallback) {
                        m_dataSegmentContentsReadCallback(m_dataSegmentDataVec, m_dataSegmentStartFlag, m_dataSegmentEndFlag, m_transferId, m_transferExtensions);
                    }
                    m_transferExtensions.extensionsVec.clear();
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
            else if (m_dataSegmentRxState == TCPCLV4_DATA_SEGMENT_RX_STATE::READ_ONE_START_SEGMENT_TRANSFER_EXTENSION_ITEM_FLAG) {
#if (__cplusplus >= 201703L)
                tcpclv4_extension_t & ext = m_transferExtensions.extensionsVec.emplace_back();
                ext.flags = rxVal;
#else
                m_transferExtensions.extensionsVec.emplace_back();
                m_transferExtensions.extensionsVec.back().flags = rxVal;
#endif
                m_dataSegmentRxState = TCPCLV4_DATA_SEGMENT_RX_STATE::READ_ONE_START_SEGMENT_TRANSFER_EXTENSION_ITEM_TYPE;
                m_readValueByteIndex = 0;
            }
            else if (m_dataSegmentRxState == TCPCLV4_DATA_SEGMENT_RX_STATE::READ_ONE_START_SEGMENT_TRANSFER_EXTENSION_ITEM_TYPE) {
                uint16_t & type = m_transferExtensions.extensionsVec.back().type;
                (reinterpret_cast<uint8_t*>(&type))[m_readValueByteIndex++] = rxVal; 
                if (m_readValueByteIndex == sizeof(uint16_t)) {
                    boost::endian::big_to_native_inplace(type);
                    m_dataSegmentRxState = TCPCLV4_DATA_SEGMENT_RX_STATE::READ_ONE_START_SEGMENT_TRANSFER_EXTENSION_ITEM_LENGTH;
                    m_readValueByteIndex = 0;
                }
            }
            else if (m_dataSegmentRxState == TCPCLV4_DATA_SEGMENT_RX_STATE::READ_ONE_START_SEGMENT_TRANSFER_EXTENSION_ITEM_LENGTH) {
                (reinterpret_cast<uint8_t*>(&m_currentTransferExtensionLength))[m_readValueByteIndex++] = rxVal; 
                if (m_readValueByteIndex == sizeof(uint16_t)) {
                    m_readValueByteIndex = 0;
                    boost::endian::big_to_native_inplace(m_currentTransferExtensionLength);
                    m_currentCountOfTransferExtensionEncodedBytes += (5 + m_currentTransferExtensionLength);
                    //If the content
                    //of the Transfer Extension Items data disagrees with the Transfer
                    //Extension Length (e.g., the last Item claims to use more octets
                    //than are present in the Transfer Extension Length), the reception
                    //of the XFER_SEGMENT is considered to have failed.
                    if (m_currentCountOfTransferExtensionEncodedBytes > m_transferExtensionItemsLengthBytes) {
                        LOG_ERROR(subprocess) << "TCPCLV4_DATA_SEGMENT_RX_STATE::READ_ONE_START_SEGMENT_TRANSFER_EXTENSION_ITEM_LENGTH, m_currentCountOfTransferExtensionEncodedBytes > m_transferExtensionItemsLengthBytes";
                        m_contactHeaderRxState = TCPCLV4_CONTACT_HEADER_RX_STATE::READ_SYNC_1;
                        m_mainRxState = TCPCLV4_MAIN_RX_STATE::READ_CONTACT_HEADER;
                    }
                    else if (m_currentTransferExtensionLength) {
                        m_transferExtensions.extensionsVec.back().valueVec.reserve(m_currentTransferExtensionLength);
                        m_dataSegmentRxState = TCPCLV4_DATA_SEGMENT_RX_STATE::READ_ONE_START_SEGMENT_TRANSFER_EXTENSION_ITEM_VALUE;
                    }
                    else { //zero-length extension
                        if (m_currentCountOfTransferExtensionEncodedBytes == m_transferExtensionItemsLengthBytes) {
                            m_dataSegmentRxState = TCPCLV4_DATA_SEGMENT_RX_STATE::READ_DATA_LENGTH_U64;
                            m_readValueByteIndex = 0;
                        }
                        else {
                            m_dataSegmentRxState = TCPCLV4_DATA_SEGMENT_RX_STATE::READ_ONE_START_SEGMENT_TRANSFER_EXTENSION_ITEM_FLAG;
                        }
                    }
                }
            }
            else if (m_dataSegmentRxState == TCPCLV4_DATA_SEGMENT_RX_STATE::READ_ONE_START_SEGMENT_TRANSFER_EXTENSION_ITEM_VALUE) {
                std::vector<uint8_t> & valueVec = m_transferExtensions.extensionsVec.back().valueVec;
                valueVec.push_back(rxVal);
                if (valueVec.size() == m_currentTransferExtensionLength) {
                    if (m_currentCountOfTransferExtensionEncodedBytes == m_transferExtensionItemsLengthBytes) {
                        m_dataSegmentRxState = TCPCLV4_DATA_SEGMENT_RX_STATE::READ_DATA_LENGTH_U64;
                        m_readValueByteIndex = 0;
                    }
                    else {
                        m_dataSegmentRxState = TCPCLV4_DATA_SEGMENT_RX_STATE::READ_ONE_START_SEGMENT_TRANSFER_EXTENSION_ITEM_FLAG;
                    }
                }
                else {
                    const std::size_t bytesRemainingToCopy = m_currentTransferExtensionLength - valueVec.size(); //guaranteed to be at least 1 from "if" above
                    const std::size_t bytesToCopy = std::min(numChars, bytesRemainingToCopy - 1); //leave last byte to go through the state machine
                    if (bytesToCopy) {
                        valueVec.insert(valueVec.end(), rxVals, rxVals + bytesToCopy); //concatenate
                        rxVals += bytesToCopy;
                        numChars -= bytesToCopy;
                    }
                }
            }
        }
        else if (mainRxState == TCPCLV4_MAIN_RX_STATE::READ_ACK_SEGMENT) {
            if (m_dataAckRxState == TCPCLV4_DATA_ACK_RX_STATE::READ_MESSAGE_FLAGS_BYTE) {
                m_ackFlags = rxVal;
                m_ack.isStartSegment = ((m_ackFlags & (1U << 1)) != 0);
                m_ack.isEndSegment = ((m_ackFlags & (1U << 0)) != 0);

                if (numChars >= sizeof(uint64_t)) { //shortcut/optimization to read ack transfer id (u64) now 
                    m_ack.transferId = UnalignedBigEndianToNativeU64(rxVals);
                    numChars -= sizeof(uint64_t);
                    rxVals += sizeof(uint64_t);
                    
                    if (numChars >= sizeof(uint64_t)) { //shortcut/optimization to read acked length (u64) now 
                        m_ack.totalBytesAcknowledged = UnalignedBigEndianToNativeU64(rxVals);

                        numChars -= sizeof(uint64_t);
                        rxVals += sizeof(uint64_t);
                        m_mainRxState = TCPCLV4_MAIN_RX_STATE::READ_MESSAGE_TYPE_BYTE;
                        if (m_ackSegmentReadCallback) {
                            m_ackSegmentReadCallback(m_ack);
                        }
                    }
                    else { //not enough bytes to read acked length (u64) now 
                        m_dataAckRxState = TCPCLV4_DATA_ACK_RX_STATE::READ_ACKNOWLEDGED_LENGTH_U64;
                        m_readValueByteIndex = 0;
                    }
                }
                else { //not enough bytes to read ack transfer id (u64) now 
                    m_dataAckRxState = TCPCLV4_DATA_ACK_RX_STATE::READ_TRANSFER_ID_U64;
                    m_readValueByteIndex = 0;
                }
            }
            else if (m_dataAckRxState == TCPCLV4_DATA_ACK_RX_STATE::READ_TRANSFER_ID_U64) {
                (reinterpret_cast<uint8_t*>(&m_ack.transferId))[m_readValueByteIndex++] = rxVal; 
                if (m_readValueByteIndex == sizeof(m_ack.transferId)) {
                    boost::endian::big_to_native_inplace(m_ack.transferId);
                    m_dataAckRxState = TCPCLV4_DATA_ACK_RX_STATE::READ_ACKNOWLEDGED_LENGTH_U64;
                    m_readValueByteIndex = 0;
                }
            }
            else if (m_dataAckRxState == TCPCLV4_DATA_ACK_RX_STATE::READ_ACKNOWLEDGED_LENGTH_U64) {
                (reinterpret_cast<uint8_t*>(&m_ack.totalBytesAcknowledged))[m_readValueByteIndex++] = rxVal; 
                if (m_readValueByteIndex == sizeof(m_ack.totalBytesAcknowledged)) {
                    boost::endian::big_to_native_inplace(m_ack.totalBytesAcknowledged);
                    m_mainRxState = TCPCLV4_MAIN_RX_STATE::READ_MESSAGE_TYPE_BYTE;
                    if (m_ackSegmentReadCallback) {
                        m_ackSegmentReadCallback(m_ack);
                    }
                }
            }
        }

        else if (mainRxState == TCPCLV4_MAIN_RX_STATE::READ_MESSAGE_REJECTION) {
            if (m_messageRejectRxState == TCPCLV4_MESSAGE_REJECT_RX_STATE::READ_REASON_CODE_BYTE) {
                m_messageRejectionReasonCode = rxVal;// (m_messageTypeFlags >> 4);
                m_messageRejectRxState = TCPCLV4_MESSAGE_REJECT_RX_STATE::READ_REJECTED_MESSAGE_HEADER;
            }
            else if (m_messageRejectRxState == TCPCLV4_MESSAGE_REJECT_RX_STATE::READ_REJECTED_MESSAGE_HEADER) {
                m_rejectedMessageHeader = rxVal;// (m_messageTypeFlags >> 4);
                m_mainRxState = TCPCLV4_MAIN_RX_STATE::READ_MESSAGE_TYPE_BYTE;
                if (m_messageRejectCallback) {
                    m_messageRejectCallback(static_cast<TCPCLV4_MESSAGE_REJECT_REASON_CODES>(m_messageRejectionReasonCode), m_rejectedMessageHeader);
                }
            }
        }
        else if (mainRxState == TCPCLV4_MAIN_RX_STATE::READ_TRANSFER_REFUSAL) {
            if (m_transferRefusalRxState == TCPCLV4_TRANSFER_REFUSAL_RX_STATE::READ_REASON_CODE_BYTE) {
                m_bundleTranferRefusalReasonCode = rxVal;// (m_messageTypeFlags >> 4);
                m_transferRefusalRxState = TCPCLV4_TRANSFER_REFUSAL_RX_STATE::READ_TRANSFER_ID;
                m_readValueByteIndex = 0;
            }
            else if (m_transferRefusalRxState == TCPCLV4_TRANSFER_REFUSAL_RX_STATE::READ_TRANSFER_ID) {
                (reinterpret_cast<uint8_t*>(&m_bundleTranferRefusalTransferId))[m_readValueByteIndex++] = rxVal; 
                if (m_readValueByteIndex == sizeof(m_bundleTranferRefusalTransferId)) {
                    boost::endian::big_to_native_inplace(m_bundleTranferRefusalTransferId);
                    m_mainRxState = TCPCLV4_MAIN_RX_STATE::READ_MESSAGE_TYPE_BYTE;
                    if (m_bundleRefusalCallback) {
                        m_bundleRefusalCallback(static_cast<TCPCLV4_TRANSFER_REFUSE_REASON_CODES>(m_bundleTranferRefusalReasonCode), m_bundleTranferRefusalTransferId);
                    }
                }
            }
        }
        else if (m_mainRxState == TCPCLV4_MAIN_RX_STATE::READ_SESSION_TERMINATION) {
            if (m_sessionTerminationRxState == TCPCLV4_SESSION_TERMINATION_RX_STATE::READ_MESSAGE_FLAGS_BYTE) {
                m_sessionTerminationFlags = rxVal;
                m_isSessionTerminationAck = ((m_sessionTerminationFlags & 1) != 0);
                m_sessionTerminationRxState = TCPCLV4_SESSION_TERMINATION_RX_STATE::READ_REASON_CODE_BYTE;
            }
            else if (m_sessionTerminationRxState == TCPCLV4_SESSION_TERMINATION_RX_STATE::READ_REASON_CODE_BYTE) {
                m_rejectedMessageHeader = rxVal;// (m_messageTypeFlags >> 4);
                m_sessionTerminationReasonCode = static_cast<TCPCLV4_SESSION_TERMINATION_REASON_CODES>(rxVal);
                m_mainRxState = TCPCLV4_MAIN_RX_STATE::READ_MESSAGE_TYPE_BYTE;
                if (m_sessionTerminationMessageCallback) {
                    m_sessionTerminationMessageCallback(m_sessionTerminationReasonCode, m_isSessionTerminationAck);
                }
            }
        }
        else if (mainRxState == TCPCLV4_MAIN_RX_STATE::READ_SESSION_INIT) {
            if (m_sessionInitRxState == TCPCLV4_SESSION_INIT_RX_STATE::READ_KEEPALIVE_INTERVAL_U16) {
                (reinterpret_cast<uint8_t*>(&m_keepAliveInterval))[m_readValueByteIndex++] = rxVal; 
                if (m_readValueByteIndex == sizeof(m_keepAliveInterval)) {
                    boost::endian::big_to_native_inplace(m_keepAliveInterval);
                    m_sessionInitRxState = TCPCLV4_SESSION_INIT_RX_STATE::READ_SEGMENT_MRU_U64;
                    m_readValueByteIndex = 0;
                }
            }
            else if (m_sessionInitRxState == TCPCLV4_SESSION_INIT_RX_STATE::READ_SEGMENT_MRU_U64) {
                (reinterpret_cast<uint8_t*>(&m_segmentMru))[m_readValueByteIndex++] = rxVal; 
                if (m_readValueByteIndex == sizeof(m_segmentMru)) {
                    boost::endian::big_to_native_inplace(m_segmentMru);
                    m_sessionInitRxState = TCPCLV4_SESSION_INIT_RX_STATE::READ_TRANSFER_MRU_U64;
                    m_readValueByteIndex = 0;
                }
            }
            else if (m_sessionInitRxState == TCPCLV4_SESSION_INIT_RX_STATE::READ_TRANSFER_MRU_U64) {
                (reinterpret_cast<uint8_t*>(&m_transferMru))[m_readValueByteIndex++] = rxVal; 
                if (m_readValueByteIndex == sizeof(m_transferMru)) {
                    boost::endian::big_to_native_inplace(m_transferMru);
                    m_sessionInitRxState = TCPCLV4_SESSION_INIT_RX_STATE::READ_NODE_ID_LENGTH_U16;
                    m_readValueByteIndex = 0;
                }
            }
            else if (m_sessionInitRxState == TCPCLV4_SESSION_INIT_RX_STATE::READ_NODE_ID_LENGTH_U16) {
                (reinterpret_cast<uint8_t*>(&m_remoteNodeUriLength))[m_readValueByteIndex++] = rxVal; 
                if (m_readValueByteIndex == sizeof(m_remoteNodeUriLength)) {
                    boost::endian::big_to_native_inplace(m_remoteNodeUriLength);
                    m_sessionInitRxState = TCPCLV4_SESSION_INIT_RX_STATE::READ_NODE_ID;
                    m_remoteNodeUriStr.resize(0);
                    m_remoteNodeUriStr.reserve(m_remoteNodeUriLength);
                }
            }
            else if (m_sessionInitRxState == TCPCLV4_SESSION_INIT_RX_STATE::READ_NODE_ID) {
                m_remoteNodeUriStr.push_back(rxVal);
                if (m_remoteNodeUriStr.size() == m_remoteNodeUriLength) {
                    m_sessionInitRxState = TCPCLV4_SESSION_INIT_RX_STATE::READ_SESSION_EXTENSION_ITEMS_LENGTH_U32;
                    m_readValueByteIndex = 0;
                }
            }
            else if (m_sessionInitRxState == TCPCLV4_SESSION_INIT_RX_STATE::READ_SESSION_EXTENSION_ITEMS_LENGTH_U32) {
                (reinterpret_cast<uint8_t*>(&m_sessionExtensionItemsLengthBytes))[m_readValueByteIndex++] = rxVal; 
                if (m_readValueByteIndex == sizeof(m_sessionExtensionItemsLengthBytes)) {
                    m_sessionExtensions.extensionsVec.clear();
                    if (m_sessionExtensionItemsLengthBytes) {
                        boost::endian::big_to_native_inplace(m_sessionExtensionItemsLengthBytes);
                        m_sessionExtensions.extensionsVec.reserve(5); //todo
                        m_currentCountOfSessionExtensionEncodedBytes = 0;
                        m_sessionInitRxState = TCPCLV4_SESSION_INIT_RX_STATE::READ_ONE_SESSION_EXTENSION_ITEM_FLAG;
                    }
                    else {
                        m_mainRxState = TCPCLV4_MAIN_RX_STATE::READ_MESSAGE_TYPE_BYTE;
                        if (m_sessionInitCallback) {
                            m_sessionInitCallback(m_keepAliveInterval, m_segmentMru, m_transferMru, m_remoteNodeUriStr, m_sessionExtensions);
                        }
                    }
                }
            }
            else if (m_sessionInitRxState == TCPCLV4_SESSION_INIT_RX_STATE::READ_ONE_SESSION_EXTENSION_ITEM_FLAG) {
#if (__cplusplus >= 201703L)
                tcpclv4_extension_t & ext = m_sessionExtensions.extensionsVec.emplace_back();
                ext.flags = rxVal;
#else
                m_sessionExtensions.extensionsVec.emplace_back();
                m_sessionExtensions.extensionsVec.back().flags = rxVal;
#endif
                m_sessionInitRxState = TCPCLV4_SESSION_INIT_RX_STATE::READ_ONE_SESSION_EXTENSION_ITEM_TYPE;
                m_readValueByteIndex = 0;
            }
            else if (m_sessionInitRxState == TCPCLV4_SESSION_INIT_RX_STATE::READ_ONE_SESSION_EXTENSION_ITEM_TYPE) {
                uint16_t & type = m_sessionExtensions.extensionsVec.back().type;
                (reinterpret_cast<uint8_t*>(&type))[m_readValueByteIndex++] = rxVal; 
                if (m_readValueByteIndex == sizeof(uint16_t)) {
                    boost::endian::big_to_native_inplace(type);
                    m_sessionInitRxState = TCPCLV4_SESSION_INIT_RX_STATE::READ_ONE_SESSION_EXTENSION_ITEM_LENGTH;
                    m_readValueByteIndex = 0;
                }
            }
            else if (m_sessionInitRxState == TCPCLV4_SESSION_INIT_RX_STATE::READ_ONE_SESSION_EXTENSION_ITEM_LENGTH) {
                (reinterpret_cast<uint8_t*>(&m_currentSessionExtensionLength))[m_readValueByteIndex++] = rxVal; 
                if (m_readValueByteIndex == sizeof(uint16_t)) {
                    m_readValueByteIndex = 0;
                    boost::endian::big_to_native_inplace(m_currentSessionExtensionLength);
                    m_currentCountOfSessionExtensionEncodedBytes += (5 + m_currentSessionExtensionLength);
                    //If the content of the Session Extension Items
                    //data disagrees with the Session Extension Length (e.g., the last
                    //Item claims to use more octets than are present in the Session
                    //Extension Length), the reception of the SESS_INIT is considered to
                    //have failed.
                    if (m_currentCountOfSessionExtensionEncodedBytes > m_sessionExtensionItemsLengthBytes) {
                        LOG_ERROR(subprocess) << "TCPCLV4_SESSION_INIT_RX_STATE::READ_ONE_SESSION_EXTENSION_ITEM_LENGTH, m_currentCountOfSessionExtensionEncodedBytes > m_sessionExtensionItemsLengthBytes";
                        m_contactHeaderRxState = TCPCLV4_CONTACT_HEADER_RX_STATE::READ_SYNC_1;
                        m_mainRxState = TCPCLV4_MAIN_RX_STATE::READ_CONTACT_HEADER;
                    }
                    else if (m_currentSessionExtensionLength) {
                        m_sessionExtensions.extensionsVec.back().valueVec.reserve(m_currentSessionExtensionLength);
                        m_sessionInitRxState = TCPCLV4_SESSION_INIT_RX_STATE::READ_ONE_SESSION_EXTENSION_ITEM_VALUE;
                    }
                    else { //zero-length extension
                        if (m_currentCountOfSessionExtensionEncodedBytes == m_sessionExtensionItemsLengthBytes) {
                            m_mainRxState = TCPCLV4_MAIN_RX_STATE::READ_MESSAGE_TYPE_BYTE;
                            if (m_sessionInitCallback) {
                                m_sessionInitCallback(m_keepAliveInterval, m_segmentMru, m_transferMru, m_remoteNodeUriStr, m_sessionExtensions);
                            }
                        }
                        else {
                            m_sessionInitRxState = TCPCLV4_SESSION_INIT_RX_STATE::READ_ONE_SESSION_EXTENSION_ITEM_FLAG;
                        }
                    }
                }
            }
            else if (m_sessionInitRxState == TCPCLV4_SESSION_INIT_RX_STATE::READ_ONE_SESSION_EXTENSION_ITEM_VALUE) {
                std::vector<uint8_t> & valueVec = m_sessionExtensions.extensionsVec.back().valueVec;
                valueVec.push_back(rxVal);
                if (valueVec.size() == m_currentSessionExtensionLength) {
                    if (m_currentCountOfSessionExtensionEncodedBytes == m_sessionExtensionItemsLengthBytes) {
                        m_mainRxState = TCPCLV4_MAIN_RX_STATE::READ_MESSAGE_TYPE_BYTE;
                        if (m_sessionInitCallback) {
                            m_sessionInitCallback(m_keepAliveInterval, m_segmentMru, m_transferMru, m_remoteNodeUriStr, m_sessionExtensions);
                        }
                    }
                    else {
                        m_sessionInitRxState = TCPCLV4_SESSION_INIT_RX_STATE::READ_ONE_SESSION_EXTENSION_ITEM_FLAG;
                    }
                }
                else {
                    const std::size_t bytesRemainingToCopy = m_currentSessionExtensionLength - valueVec.size(); //guaranteed to be at least 1 from "if" above
                    const std::size_t bytesToCopy = std::min(numChars, bytesRemainingToCopy - 1); //leave last byte to go through the state machine
                    if (bytesToCopy) {
                        valueVec.insert(valueVec.end(), rxVals, rxVals + bytesToCopy); //concatenate
                        rxVals += bytesToCopy;
                        numChars -= bytesToCopy;
                    }
                }
            }
        }
    }
}

void TcpclV4::GenerateContactHeader(std::vector<uint8_t> & hdr, bool remoteHasEnabledTlsSecurity) {
    hdr.resize(6);
    hdr[0] = 'd';
    hdr[1] = 't';
    hdr[2] = 'n';
    hdr[3] = '!';
    hdr[4] = 4; //version
    hdr[5] = remoteHasEnabledTlsSecurity; //works because bit 0 set is the flag for tls enabled
}

bool TcpclV4::GenerateSessionInitMessage(std::vector<uint8_t> & msg, uint16_t keepAliveIntervalSeconds, uint64_t segmentMru, uint64_t transferMru,
        const std::string & myNodeEidUri, const tcpclv4_extensions_t & sessionExtensions)
{
    
    const uint16_t myNodeEidUriLength = static_cast<uint16_t>(myNodeEidUri.size());
    const uint32_t sessionExtensionsLengthBytes = static_cast<uint32_t>(sessionExtensions.GetTotalDataRequiredForSerialization());
    msg.resize(sizeof(uint8_t) + sizeof(keepAliveIntervalSeconds) + sizeof(segmentMru) + sizeof(transferMru)
        + sizeof(myNodeEidUriLength) + sizeof(sessionExtensionsLengthBytes) + myNodeEidUriLength + sessionExtensionsLengthBytes);

    uint8_t * ptr = &msg[0];
    *ptr++ = static_cast<uint8_t>(TCPCLV4_MESSAGE_TYPE_BYTE_CODES::SESS_INIT);
    NativeU16ToUnalignedBigEndian(ptr, keepAliveIntervalSeconds);
    ptr += sizeof(keepAliveIntervalSeconds);
    NativeU64ToUnalignedBigEndian(ptr, segmentMru);
    ptr += sizeof(segmentMru);
    NativeU64ToUnalignedBigEndian(ptr, transferMru);
    ptr += sizeof(transferMru);
    NativeU16ToUnalignedBigEndian(ptr, myNodeEidUriLength);
    ptr += sizeof(myNodeEidUriLength);
    memcpy(ptr, myNodeEidUri.data(), myNodeEidUriLength);
    ptr += myNodeEidUriLength;
    NativeU32ToUnalignedBigEndian(ptr, sessionExtensionsLengthBytes);
    ptr += sizeof(sessionExtensionsLengthBytes);
    ptr += sessionExtensions.Serialize(ptr);
    return ((static_cast<std::size_t>(ptr - msg.data())) == msg.size());
}


bool TcpclV4::GenerateNonFragmentedDataSegment(std::vector<uint8_t> & dataSegment, uint64_t transferId, const uint8_t * contents, uint64_t sizeContents) {
    static constexpr uint8_t startAndEndSegmentFlagsSet = 3U;
    const uint8_t dataSegmentFlags = startAndEndSegmentFlagsSet;

    //The Transfer Extension Length and Transfer
    //Extension Item fields SHALL only be present when the 'START' flag
    //is set to 1 on the message. (which it is here)
    dataSegment.resize(sizeof(uint8_t) + sizeof(dataSegmentFlags) + sizeof(transferId) + sizeof(uint32_t)
        + sizeof(sizeContents) + sizeContents);


    uint8_t * ptr = &dataSegment[0];
    *ptr++ = static_cast<uint8_t>(TCPCLV4_MESSAGE_TYPE_BYTE_CODES::XFER_SEGMENT);
    *ptr++ = dataSegmentFlags;
    NativeU64ToUnalignedBigEndian(ptr, transferId);
    ptr += sizeof(transferId);
    ClearFourBytes(ptr); //transfer extension length 0
    ptr += sizeof(uint32_t);
    NativeU64ToUnalignedBigEndian(ptr, sizeContents);
    ptr += sizeof(sizeContents);
    memcpy(ptr, contents, sizeContents);
    ptr += sizeContents;

    return ((static_cast<std::size_t>(ptr - dataSegment.data())) == dataSegment.size());
}
bool TcpclV4::GenerateNonFragmentedDataSegmentHeaderOnly(std::vector<uint8_t> & dataSegmentHeaderDataVec, uint64_t transferId, uint64_t sizeContents) {
    static constexpr uint8_t startAndEndSegmentFlagsSet = 3U;
    const uint8_t dataSegmentFlags = startAndEndSegmentFlagsSet;

    //The Transfer Extension Length and Transfer
    //Extension Item fields SHALL only be present when the 'START' flag
    //is set to 1 on the message. (which it is here)
    dataSegmentHeaderDataVec.resize(sizeof(uint8_t) + sizeof(dataSegmentFlags) + sizeof(transferId) + sizeof(uint32_t) + sizeof(sizeContents));


    uint8_t * ptr = &dataSegmentHeaderDataVec[0];
    *ptr++ = static_cast<uint8_t>(TCPCLV4_MESSAGE_TYPE_BYTE_CODES::XFER_SEGMENT);
    *ptr++ = dataSegmentFlags;
    NativeU64ToUnalignedBigEndian(ptr, transferId);
    ptr += sizeof(transferId);
    ClearFourBytes(ptr); //transfer extension length 0
    ptr += sizeof(uint32_t);
    NativeU64ToUnalignedBigEndian(ptr, sizeContents);
    ptr += sizeof(sizeContents);

    return ((static_cast<std::size_t>(ptr - dataSegmentHeaderDataVec.data())) == dataSegmentHeaderDataVec.size());
}


bool TcpclV4::GenerateNonFragmentedDataSegment(std::vector<uint8_t> & dataSegment, uint64_t transferId,
    const uint8_t * contents, uint64_t sizeContents, const tcpclv4_extensions_t & transferExtensions)
{
    static constexpr uint8_t startAndEndSegmentFlagsSet = 3U;
    const uint8_t dataSegmentFlags = startAndEndSegmentFlagsSet;

    //The Transfer Extension Length and Transfer
    //Extension Item fields SHALL only be present when the 'START' flag
    //is set to 1 on the message. (which it is here)
    const uint32_t transferExtensionsLengthBytes = static_cast<uint32_t>(transferExtensions.GetTotalDataRequiredForSerialization());
    dataSegment.resize(sizeof(uint8_t) + sizeof(dataSegmentFlags) + sizeof(transferId) + sizeof(transferExtensionsLengthBytes)
        + sizeof(sizeContents) + transferExtensionsLengthBytes + sizeContents);


    uint8_t * ptr = &dataSegment[0];
    *ptr++ = static_cast<uint8_t>(TCPCLV4_MESSAGE_TYPE_BYTE_CODES::XFER_SEGMENT);
    *ptr++ = dataSegmentFlags;
    NativeU64ToUnalignedBigEndian(ptr, transferId);
    ptr += sizeof(transferId);
    NativeU32ToUnalignedBigEndian(ptr, transferExtensionsLengthBytes);
    ptr += sizeof(transferExtensionsLengthBytes);
    ptr += transferExtensions.Serialize(ptr);
    NativeU64ToUnalignedBigEndian(ptr, sizeContents);
    ptr += sizeof(sizeContents);
    memcpy(ptr, contents, sizeContents);
    ptr += sizeContents;

    return ((static_cast<std::size_t>(ptr - dataSegment.data())) == dataSegment.size());
}
bool TcpclV4::GenerateNonFragmentedDataSegmentHeaderOnly(std::vector<uint8_t> & dataSegmentHeaderDataVec, uint64_t transferId,
    uint64_t sizeContents, const tcpclv4_extensions_t & transferExtensions)
{
    static constexpr uint8_t startAndEndSegmentFlagsSet = 3U;
    const uint8_t dataSegmentFlags = startAndEndSegmentFlagsSet;

    //The Transfer Extension Length and Transfer
    //Extension Item fields SHALL only be present when the 'START' flag
    //is set to 1 on the message. (which it is here)
    const uint32_t transferExtensionsLengthBytes = static_cast<uint32_t>(transferExtensions.GetTotalDataRequiredForSerialization());
    dataSegmentHeaderDataVec.resize(sizeof(uint8_t) + sizeof(dataSegmentFlags) + sizeof(transferId) + sizeof(transferExtensionsLengthBytes)
        + sizeof(sizeContents) + transferExtensionsLengthBytes);


    uint8_t * ptr = &dataSegmentHeaderDataVec[0];
    *ptr++ = static_cast<uint8_t>(TCPCLV4_MESSAGE_TYPE_BYTE_CODES::XFER_SEGMENT);
    *ptr++ = dataSegmentFlags;
    NativeU64ToUnalignedBigEndian(ptr, transferId);
    ptr += sizeof(transferId);
    NativeU32ToUnalignedBigEndian(ptr, transferExtensionsLengthBytes);
    ptr += sizeof(transferExtensionsLengthBytes);
    ptr += transferExtensions.Serialize(ptr);
    NativeU64ToUnalignedBigEndian(ptr, sizeContents);
    ptr += sizeof(sizeContents);

    return ((static_cast<std::size_t>(ptr - dataSegmentHeaderDataVec.data())) == dataSegmentHeaderDataVec.size());
}


bool TcpclV4::GenerateStartDataSegment(std::vector<uint8_t> & dataSegment, bool isEndSegment, uint64_t transferId,
    const uint8_t * contents, uint64_t sizeContents, const tcpclv4_extensions_t & transferExtensions)
{
    static constexpr uint8_t startSegmentFlagSet = 1U << 1;
    const uint8_t dataSegmentFlags = startSegmentFlagSet | (static_cast<uint8_t>(isEndSegment));
    
    //The Transfer Extension Length and Transfer
    //Extension Item fields SHALL only be present when the 'START' flag
    //is set to 1 on the message. (which it is here)
    const uint32_t transferExtensionsLengthBytes = static_cast<uint32_t>(transferExtensions.GetTotalDataRequiredForSerialization());
    dataSegment.resize(sizeof(uint8_t) + sizeof(dataSegmentFlags) + sizeof(transferId) + sizeof(transferExtensionsLengthBytes)
        + sizeof(sizeContents) + transferExtensionsLengthBytes + sizeContents);


    uint8_t * ptr = &dataSegment[0];
    *ptr++ = static_cast<uint8_t>(TCPCLV4_MESSAGE_TYPE_BYTE_CODES::XFER_SEGMENT);
    *ptr++ = dataSegmentFlags;
    NativeU64ToUnalignedBigEndian(ptr, transferId);
    ptr += sizeof(transferId);
    NativeU32ToUnalignedBigEndian(ptr, transferExtensionsLengthBytes);
    ptr += sizeof(transferExtensionsLengthBytes);
    ptr += transferExtensions.Serialize(ptr);
    NativeU64ToUnalignedBigEndian(ptr, sizeContents);
    ptr += sizeof(sizeContents);
    memcpy(ptr, contents, sizeContents);
    ptr += sizeContents;
        
    return ((static_cast<std::size_t>(ptr - dataSegment.data())) == dataSegment.size());
}
bool TcpclV4::GenerateStartDataSegmentHeaderOnly(std::vector<uint8_t> & dataSegmentHeaderDataVec, bool isEndSegment, uint64_t transferId,
    uint64_t sizeContents, const tcpclv4_extensions_t & transferExtensions)
{
    static constexpr uint8_t startSegmentFlagSet = 1U << 1;
    const uint8_t dataSegmentFlags = startSegmentFlagSet | (static_cast<uint8_t>(isEndSegment));

    //The Transfer Extension Length and Transfer
    //Extension Item fields SHALL only be present when the 'START' flag
    //is set to 1 on the message. (which it is here)
    const uint32_t transferExtensionsLengthBytes = static_cast<uint32_t>(transferExtensions.GetTotalDataRequiredForSerialization());
    dataSegmentHeaderDataVec.resize(sizeof(uint8_t) + sizeof(dataSegmentFlags) + sizeof(transferId) + sizeof(transferExtensionsLengthBytes)
        + sizeof(sizeContents) + transferExtensionsLengthBytes);


    uint8_t * ptr = &dataSegmentHeaderDataVec[0];
    *ptr++ = static_cast<uint8_t>(TCPCLV4_MESSAGE_TYPE_BYTE_CODES::XFER_SEGMENT);
    *ptr++ = dataSegmentFlags;
    NativeU64ToUnalignedBigEndian(ptr, transferId);
    ptr += sizeof(transferId);
    NativeU32ToUnalignedBigEndian(ptr, transferExtensionsLengthBytes);
    ptr += sizeof(transferExtensionsLengthBytes);
    ptr += transferExtensions.Serialize(ptr);
    NativeU64ToUnalignedBigEndian(ptr, sizeContents);
    ptr += sizeof(sizeContents);

    return ((static_cast<std::size_t>(ptr - dataSegmentHeaderDataVec.data())) == dataSegmentHeaderDataVec.size());
}


bool TcpclV4::GenerateFragmentedStartDataSegmentWithLengthExtension(std::vector<uint8_t> & dataSegment, uint64_t transferId,
    const uint8_t * contents, uint64_t sizeContents, uint64_t totalBundleLengthToBeSent)
{
    //bool isEndSegment is false
    static const uint8_t dataSegmentFlags = 1U << 1; //startSegmentFlagSet

    //The Transfer Extension Length and Transfer
    //Extension Item fields SHALL only be present when the 'START' flag
    //is set to 1 on the message. (which it is here)
    static const uint32_t transferExtensionsLengthBytes = static_cast<uint32_t>(tcpclv4_extension_t::SIZE_OF_SERIALIZED_TRANSFER_LENGTH_EXTENSION);
    dataSegment.resize(sizeof(uint8_t) + sizeof(dataSegmentFlags) + sizeof(transferId) + sizeof(transferExtensionsLengthBytes)
        + sizeof(sizeContents) + transferExtensionsLengthBytes + sizeContents);

    /*
    If a transfer occupies exactly one segment (i.e., both START and END
    flags are 1) the Transfer Length extension SHOULD NOT be present.
    The extension does not provide any additional information for single-
    segment transfers.*/

    uint8_t * ptr = &dataSegment[0];
    *ptr++ = static_cast<uint8_t>(TCPCLV4_MESSAGE_TYPE_BYTE_CODES::XFER_SEGMENT);
    *ptr++ = dataSegmentFlags;
    NativeU64ToUnalignedBigEndian(ptr, transferId);
    ptr += sizeof(transferId);
    NativeU32ToUnalignedBigEndian(ptr, transferExtensionsLengthBytes);
    ptr += sizeof(transferExtensionsLengthBytes);
    ptr += tcpclv4_extension_t::SerializeTransferLengthExtension(ptr, totalBundleLengthToBeSent);
    NativeU64ToUnalignedBigEndian(ptr, sizeContents);
    ptr += sizeof(sizeContents);
    memcpy(ptr, contents, sizeContents);
    ptr += sizeContents;

    return ((static_cast<std::size_t>(ptr - dataSegment.data())) == dataSegment.size());
}
bool TcpclV4::GenerateFragmentedStartDataSegmentWithLengthExtensionHeaderOnly(std::vector<uint8_t> & dataSegmentHeaderDataVec, uint64_t transferId,
    uint64_t sizeContents, uint64_t totalBundleLengthToBeSent)
{
    //bool isEndSegment is false
    static const uint8_t dataSegmentFlags = 1U << 1; // startSegmentFlagSet;

    //The Transfer Extension Length and Transfer
    //Extension Item fields SHALL only be present when the 'START' flag
    //is set to 1 on the message. (which it is here)
    static const uint32_t transferExtensionsLengthBytes = static_cast<uint32_t>(tcpclv4_extension_t::SIZE_OF_SERIALIZED_TRANSFER_LENGTH_EXTENSION);
    dataSegmentHeaderDataVec.resize(sizeof(uint8_t) + sizeof(dataSegmentFlags) + sizeof(transferId) + sizeof(transferExtensionsLengthBytes)
        + sizeof(sizeContents) + transferExtensionsLengthBytes);

    /*
    If a transfer occupies exactly one segment (i.e., both START and END
    flags are 1) the Transfer Length extension SHOULD NOT be present.
    The extension does not provide any additional information for single-
    segment transfers.*/

    uint8_t * ptr = &dataSegmentHeaderDataVec[0];
    *ptr++ = static_cast<uint8_t>(TCPCLV4_MESSAGE_TYPE_BYTE_CODES::XFER_SEGMENT);
    *ptr++ = dataSegmentFlags;
    NativeU64ToUnalignedBigEndian(ptr, transferId);
    ptr += sizeof(transferId);
    NativeU32ToUnalignedBigEndian(ptr, transferExtensionsLengthBytes);
    ptr += sizeof(transferExtensionsLengthBytes);
    ptr += tcpclv4_extension_t::SerializeTransferLengthExtension(ptr, totalBundleLengthToBeSent);
    NativeU64ToUnalignedBigEndian(ptr, sizeContents);
    ptr += sizeof(sizeContents);
    
    return ((static_cast<std::size_t>(ptr - dataSegmentHeaderDataVec.data())) == dataSegmentHeaderDataVec.size());
}


bool TcpclV4::GenerateNonStartDataSegment(std::vector<uint8_t> & dataSegment, bool isEndSegment, uint64_t transferId,
    const uint8_t * contents, uint64_t sizeContents)
{

    const uint8_t dataSegmentFlags = (static_cast<uint8_t>(isEndSegment));

    //The Transfer Extension Length and Transfer
    //Extension Item fields SHALL only be present when the 'START' flag
    //is set to 1 on the message.
    dataSegment.resize(sizeof(uint8_t) + sizeof(dataSegmentFlags) + sizeof(transferId) + sizeof(sizeContents) + sizeContents);

    uint8_t * ptr = &dataSegment[0];
    *ptr++ = static_cast<uint8_t>(TCPCLV4_MESSAGE_TYPE_BYTE_CODES::XFER_SEGMENT);
    *ptr++ = dataSegmentFlags;
    NativeU64ToUnalignedBigEndian(ptr, transferId);
    ptr += sizeof(transferId);
    NativeU64ToUnalignedBigEndian(ptr, sizeContents);
    ptr += sizeof(sizeContents);
    memcpy(ptr, contents, sizeContents);
    ptr += sizeContents;

    return ((static_cast<std::size_t>(ptr - dataSegment.data())) == dataSegment.size());
}
bool TcpclV4::GenerateNonStartDataSegmentHeaderOnly(std::vector<uint8_t> & dataSegmentHeaderDataVec, bool isEndSegment, uint64_t transferId, uint64_t sizeContents) {

    const uint8_t dataSegmentFlags = (static_cast<uint8_t>(isEndSegment));

    //The Transfer Extension Length and Transfer
    //Extension Item fields SHALL only be present when the 'START' flag
    //is set to 1 on the message.
    dataSegmentHeaderDataVec.resize(sizeof(uint8_t) + sizeof(dataSegmentFlags) + sizeof(transferId) + sizeof(sizeContents));

    uint8_t * ptr = &dataSegmentHeaderDataVec[0];
    *ptr++ = static_cast<uint8_t>(TCPCLV4_MESSAGE_TYPE_BYTE_CODES::XFER_SEGMENT);
    *ptr++ = dataSegmentFlags;
    NativeU64ToUnalignedBigEndian(ptr, transferId);
    ptr += sizeof(transferId);
    NativeU64ToUnalignedBigEndian(ptr, sizeContents);
    ptr += sizeof(sizeContents);
    
    return ((static_cast<std::size_t>(ptr - dataSegmentHeaderDataVec.data())) == dataSegmentHeaderDataVec.size());
}

bool TcpclV4::GenerateAckSegment(std::vector<uint8_t> & ackSegment, const tcpclv4_ack_t & ack) {
    const uint8_t ackSegmentFlags = ((static_cast<uint8_t>(ack.isStartSegment)) << 1) | (static_cast<uint8_t>(ack.isEndSegment));

    ackSegment.resize(sizeof(uint8_t) + sizeof(ackSegmentFlags) + sizeof(ack.transferId) + sizeof(ack.totalBytesAcknowledged));

    uint8_t * ptr = &ackSegment[0];
    *ptr++ = static_cast<uint8_t>(TCPCLV4_MESSAGE_TYPE_BYTE_CODES::XFER_ACK);
    *ptr++ = ackSegmentFlags;
    NativeU64ToUnalignedBigEndian(ptr, ack.transferId);
    ptr += sizeof(ack.transferId);
    NativeU64ToUnalignedBigEndian(ptr, ack.totalBytesAcknowledged);
    ptr += sizeof(ack.totalBytesAcknowledged);

    return ((static_cast<std::size_t>(ptr - ackSegment.data())) == ackSegment.size());
}

bool TcpclV4::GenerateAckSegment(std::vector<uint8_t> & ackSegment, bool isStartSegment, bool isEndSegment, uint64_t transferId, uint64_t totalBytesAcknowledged) {
    return GenerateAckSegment(ackSegment, tcpclv4_ack_t(isStartSegment, isEndSegment, transferId, totalBytesAcknowledged));
}

void TcpclV4::GenerateBundleRefusal(std::vector<uint8_t> & refusalMessage, TCPCLV4_TRANSFER_REFUSE_REASON_CODES refusalCode, uint64_t transferId) {

    refusalMessage.resize(sizeof(uint8_t) + sizeof(uint8_t) + sizeof(transferId));
    refusalMessage[0] = static_cast<uint8_t>(TCPCLV4_MESSAGE_TYPE_BYTE_CODES::XFER_REFUSE);
    refusalMessage[1] = static_cast<uint8_t>(refusalCode);
    NativeU64ToUnalignedBigEndian(&refusalMessage[2], transferId);
}

void TcpclV4::GenerateMessageRejection(std::vector<uint8_t> & rejectionMessage, TCPCLV4_MESSAGE_REJECT_REASON_CODES rejectionCode, uint8_t rejectedMessageHeader) {

    rejectionMessage.resize(sizeof(uint8_t) + sizeof(uint8_t) + sizeof(rejectedMessageHeader));
    rejectionMessage[0] = static_cast<uint8_t>(TCPCLV4_MESSAGE_TYPE_BYTE_CODES::MSG_REJECT);
    rejectionMessage[1] = static_cast<uint8_t>(rejectionCode);
    rejectionMessage[2] = rejectedMessageHeader;
}


void TcpclV4::GenerateKeepAliveMessage(std::vector<uint8_t> & keepAliveMessage) {
    keepAliveMessage.resize(1);
    keepAliveMessage[0] = static_cast<uint8_t>(TCPCLV4_MESSAGE_TYPE_BYTE_CODES::KEEPALIVE);
}

void TcpclV4::GenerateSessionTerminationMessage(std::vector<uint8_t> & sessionTerminationMessage,
    TCPCLV4_SESSION_TERMINATION_REASON_CODES sessionTerminationReasonCode, bool isAckOfAnEarlierSessionTerminationMessage)
{
    sessionTerminationMessage.resize(sizeof(uint8_t) + sizeof(uint8_t) + sizeof(uint8_t));
    sessionTerminationMessage[0] = static_cast<uint8_t>(TCPCLV4_MESSAGE_TYPE_BYTE_CODES::SESS_TERM);
    sessionTerminationMessage[1] = static_cast<uint8_t>(isAckOfAnEarlierSessionTerminationMessage); //code 0x01
    sessionTerminationMessage[2] = static_cast<uint8_t>(sessionTerminationReasonCode);
}

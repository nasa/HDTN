/***************************************************************************
 * NASA Glenn Research Center, Cleveland, OH
 * Released under the NASA Open Source Agreement (NOSA)
 * May  2021
 *
 ****************************************************************************
 */
#include "codec/Bpv6AdministrativeRecords.h"
#include "Sdnv.h"

BundleStatusReport::BundleStatusReport() : 
    m_statusFlags(BPV6_BUNDLE_STATUS_REPORT_STATUS_FLAGS::NONE),
    m_reasonCode(BPV6_BUNDLE_STATUS_REPORT_REASON_CODES::NO_ADDITIONAL_INFORMATION),
    m_isFragment(false),
    m_fragmentOffsetIfPresent(0),
    m_fragmentLengthIfPresent(0),
    m_copyOfBundleCreationTimestampTimeSeconds(0),
    m_copyOfBundleCreationTimestampSequenceNumber(0)
    { } //a default constructor: X()
BundleStatusReport::~BundleStatusReport() { } //a destructor: ~X()
BundleStatusReport::BundleStatusReport(const BundleStatusReport& o) : 
    m_statusFlags(o.m_statusFlags),
    m_reasonCode(o.m_reasonCode),
    m_isFragment(o.m_isFragment),
    m_fragmentOffsetIfPresent(o.m_fragmentOffsetIfPresent),
    m_fragmentLengthIfPresent(o.m_fragmentLengthIfPresent),
    m_timeOfReceiptOfBundle(o.m_timeOfReceiptOfBundle),
    m_timeOfCustodyAcceptanceOfBundle(o.m_timeOfCustodyAcceptanceOfBundle),
    m_timeOfForwardingOfBundle(o.m_timeOfForwardingOfBundle),
    m_timeOfDeliveryOfBundle(o.m_timeOfDeliveryOfBundle),
    m_timeOfDeletionOfBundle(o.m_timeOfDeletionOfBundle),
    m_copyOfBundleCreationTimestampTimeSeconds(o.m_copyOfBundleCreationTimestampTimeSeconds),
    m_copyOfBundleCreationTimestampSequenceNumber(o.m_copyOfBundleCreationTimestampSequenceNumber),
    m_bundleSourceEid(o.m_bundleSourceEid)
{ } //a copy constructor: X(const X&)
BundleStatusReport::BundleStatusReport(BundleStatusReport&& o) : 
    m_statusFlags(o.m_statusFlags),
    m_reasonCode(o.m_reasonCode),
    m_isFragment(o.m_isFragment),
    m_fragmentOffsetIfPresent(o.m_fragmentOffsetIfPresent),
    m_fragmentLengthIfPresent(o.m_fragmentLengthIfPresent),
    m_timeOfReceiptOfBundle(std::move(o.m_timeOfReceiptOfBundle)),
    m_timeOfCustodyAcceptanceOfBundle(std::move(o.m_timeOfCustodyAcceptanceOfBundle)),
    m_timeOfForwardingOfBundle(std::move(o.m_timeOfForwardingOfBundle)),
    m_timeOfDeliveryOfBundle(std::move(o.m_timeOfDeliveryOfBundle)),
    m_timeOfDeletionOfBundle(std::move(o.m_timeOfDeletionOfBundle)),
    m_copyOfBundleCreationTimestampTimeSeconds(o.m_copyOfBundleCreationTimestampTimeSeconds),
    m_copyOfBundleCreationTimestampSequenceNumber(o.m_copyOfBundleCreationTimestampSequenceNumber),
    m_bundleSourceEid(std::move(o.m_bundleSourceEid)) { } //a move constructor: X(X&&)
BundleStatusReport& BundleStatusReport::operator=(const BundleStatusReport& o) { //a copy assignment: operator=(const X&)
    m_statusFlags = o.m_statusFlags;
    m_reasonCode = o.m_reasonCode;
    m_isFragment = o.m_isFragment;
    m_fragmentOffsetIfPresent = o.m_fragmentOffsetIfPresent;
    m_fragmentLengthIfPresent = o.m_fragmentLengthIfPresent;
    m_timeOfReceiptOfBundle = o.m_timeOfReceiptOfBundle;
    m_timeOfCustodyAcceptanceOfBundle = o.m_timeOfCustodyAcceptanceOfBundle;
    m_timeOfForwardingOfBundle = o.m_timeOfForwardingOfBundle;
    m_timeOfDeliveryOfBundle = o.m_timeOfDeliveryOfBundle;
    m_timeOfDeletionOfBundle = o.m_timeOfDeletionOfBundle;
    m_copyOfBundleCreationTimestampTimeSeconds = o.m_copyOfBundleCreationTimestampTimeSeconds;
    m_copyOfBundleCreationTimestampSequenceNumber = o.m_copyOfBundleCreationTimestampSequenceNumber;
    m_bundleSourceEid = o.m_bundleSourceEid;
    return *this;
}
BundleStatusReport& BundleStatusReport::operator=(BundleStatusReport && o) { //a move assignment: operator=(X&&)
    m_statusFlags = o.m_statusFlags;
    m_reasonCode = o.m_reasonCode;
    m_isFragment = o.m_isFragment;
    m_fragmentOffsetIfPresent = o.m_fragmentOffsetIfPresent;
    m_fragmentLengthIfPresent = o.m_fragmentLengthIfPresent;
    m_timeOfReceiptOfBundle = std::move(o.m_timeOfReceiptOfBundle);
    m_timeOfCustodyAcceptanceOfBundle = std::move(o.m_timeOfCustodyAcceptanceOfBundle);
    m_timeOfForwardingOfBundle = std::move(o.m_timeOfForwardingOfBundle);
    m_timeOfDeliveryOfBundle = std::move(o.m_timeOfDeliveryOfBundle);
    m_timeOfDeletionOfBundle = std::move(o.m_timeOfDeletionOfBundle);
    m_copyOfBundleCreationTimestampTimeSeconds = o.m_copyOfBundleCreationTimestampTimeSeconds;
    m_copyOfBundleCreationTimestampSequenceNumber = o.m_copyOfBundleCreationTimestampSequenceNumber;
    m_bundleSourceEid = std::move(o.m_bundleSourceEid);
    return *this;
}
bool BundleStatusReport::operator==(const BundleStatusReport & o) const {
    return
        (m_statusFlags == o.m_statusFlags) &&
        (m_reasonCode == o.m_reasonCode) &&
        (m_isFragment == o.m_isFragment) &&
        (m_fragmentOffsetIfPresent == o.m_fragmentOffsetIfPresent) &&
        (m_fragmentLengthIfPresent == o.m_fragmentLengthIfPresent) &&
        (m_timeOfReceiptOfBundle == o.m_timeOfReceiptOfBundle) &&
        (m_timeOfCustodyAcceptanceOfBundle == o.m_timeOfCustodyAcceptanceOfBundle) &&
        (m_timeOfForwardingOfBundle == o.m_timeOfForwardingOfBundle) &&
        (m_timeOfDeliveryOfBundle == o.m_timeOfDeliveryOfBundle) &&
        (m_timeOfDeletionOfBundle == o.m_timeOfDeletionOfBundle) &&
        (m_copyOfBundleCreationTimestampTimeSeconds == o.m_copyOfBundleCreationTimestampTimeSeconds) &&
        (m_copyOfBundleCreationTimestampSequenceNumber == o.m_copyOfBundleCreationTimestampSequenceNumber) &&
        (m_bundleSourceEid == o.m_bundleSourceEid);
}
bool BundleStatusReport::operator!=(const BundleStatusReport & o) const {
    return !(*this == o);
}
void BundleStatusReport::Reset() { //a copy assignment: operator=(const X&)
    m_statusFlags = BPV6_BUNDLE_STATUS_REPORT_STATUS_FLAGS::NONE;
    m_reasonCode = BPV6_BUNDLE_STATUS_REPORT_REASON_CODES::NO_ADDITIONAL_INFORMATION;
    m_isFragment = false;
    m_fragmentOffsetIfPresent = 0;
    m_fragmentLengthIfPresent = 0;
    
    m_timeOfReceiptOfBundle.SetZero();
    m_timeOfCustodyAcceptanceOfBundle.SetZero();
    m_timeOfForwardingOfBundle.SetZero();
    m_timeOfDeliveryOfBundle.SetZero();
    m_timeOfDeletionOfBundle.SetZero();
    m_copyOfBundleCreationTimestampTimeSeconds = 0;
    m_copyOfBundleCreationTimestampSequenceNumber = 0;
    m_bundleSourceEid.clear();
}

uint64_t BundleStatusReport::Serialize(uint8_t * buffer) const { //use MAX_SERIALIZATION_SIZE sized buffer
    uint8_t * const serializationBase = buffer;

    *buffer++ = ((static_cast<uint8_t>(BPV6_ADMINISTRATIVE_RECORD_TYPES::STATUS_REPORT)) << 4) | (static_cast<uint8_t>(m_isFragment)); // works because BUNDLE_IS_A_FRAGMENT == 1
    *buffer++ = static_cast<uint8_t>(m_statusFlags);
    *buffer++ = static_cast<uint8_t>(m_reasonCode);

    if (m_isFragment) {
        buffer += SdnvEncodeU64(buffer, m_fragmentOffsetIfPresent);
        buffer += SdnvEncodeU64(buffer, m_fragmentLengthIfPresent);
    }
    const uint8_t statusFlagsAsUint8 = static_cast<uint8_t>(m_statusFlags);
    if ((static_cast<uint8_t>(BPV6_BUNDLE_STATUS_REPORT_STATUS_FLAGS::REPORTING_NODE_RECEIVED_BUNDLE)) & statusFlagsAsUint8) {
        buffer += m_timeOfReceiptOfBundle.Serialize(buffer);
    }
    if ((static_cast<uint8_t>(BPV6_BUNDLE_STATUS_REPORT_STATUS_FLAGS::REPORTING_NODE_ACCEPTED_CUSTODY_OF_BUNDLE)) & statusFlagsAsUint8) {
        buffer += m_timeOfCustodyAcceptanceOfBundle.Serialize(buffer);
    }
    if ((static_cast<uint8_t>(BPV6_BUNDLE_STATUS_REPORT_STATUS_FLAGS::REPORTING_NODE_FORWARDED_BUNDLE)) & statusFlagsAsUint8) {
        buffer += m_timeOfForwardingOfBundle.Serialize(buffer);
    }
    if ((static_cast<uint8_t>(BPV6_BUNDLE_STATUS_REPORT_STATUS_FLAGS::REPORTING_NODE_DELIVERED_BUNDLE)) & statusFlagsAsUint8) {
        buffer += m_timeOfDeliveryOfBundle.Serialize(buffer);
    }
    if ((static_cast<uint8_t>(BPV6_BUNDLE_STATUS_REPORT_STATUS_FLAGS::REPORTING_NODE_DELETED_BUNDLE)) & statusFlagsAsUint8) {
        buffer += m_timeOfDeletionOfBundle.Serialize(buffer);
    }

    buffer += SdnvEncodeU64(buffer, m_copyOfBundleCreationTimestampTimeSeconds);
    buffer += SdnvEncodeU64(buffer, m_copyOfBundleCreationTimestampSequenceNumber);

    const uint32_t lengthEidStr = static_cast<uint32_t>(m_bundleSourceEid.length());
    if (lengthEidStr <= 127) {
        *buffer++ = static_cast<uint8_t>(lengthEidStr);
    }
    else {
        buffer += SdnvEncodeU32(buffer, lengthEidStr); //work with both dtn and ipn scheme
    }
    //*buffer++ = static_cast<uint8_t>(lengthEidStr); //will always be a 1 byte sdnv
    memcpy(buffer, m_bundleSourceEid.data(), lengthEidStr);
    buffer += lengthEidStr;
    return buffer - serializationBase;
}

uint16_t BundleStatusReport::Deserialize(const uint8_t * serialization) {
    uint8_t sdnvSize;
    const uint8_t * const serializationBase = serialization;
    Reset();

    const BPV6_ADMINISTRATIVE_RECORD_TYPES adminRecordType = static_cast<BPV6_ADMINISTRATIVE_RECORD_TYPES>(*serialization >> 4);
    if (adminRecordType != BPV6_ADMINISTRATIVE_RECORD_TYPES::STATUS_REPORT) {
        return 0; //failure
    }
    m_isFragment = ((*serialization & 1) != 0); // works because BUNDLE_IS_A_FRAGMENT == 1
    ++serialization;

    m_statusFlags = static_cast<BPV6_BUNDLE_STATUS_REPORT_STATUS_FLAGS>(*serialization++);
    m_reasonCode = static_cast<BPV6_BUNDLE_STATUS_REPORT_REASON_CODES>(*serialization++);

    if (m_isFragment) {
        m_fragmentOffsetIfPresent = SdnvDecodeU64(serialization, &sdnvSize);
        if (sdnvSize == 0) {
            return 0; //failure
        }
        serialization += sdnvSize;

        m_fragmentLengthIfPresent = SdnvDecodeU64(serialization, &sdnvSize);
        if (sdnvSize == 0) {
            return 0; //failure
        }
        serialization += sdnvSize;
    }
    
    const uint8_t statusFlagsAsUint8 = static_cast<uint8_t>(m_statusFlags);
    if ((static_cast<uint8_t>(BPV6_BUNDLE_STATUS_REPORT_STATUS_FLAGS::REPORTING_NODE_RECEIVED_BUNDLE)) & statusFlagsAsUint8) {
        if (!m_timeOfReceiptOfBundle.Deserialize(serialization, &sdnvSize)) {
            return 0;
        }
        serialization += sdnvSize;
    }
    if ((static_cast<uint8_t>(BPV6_BUNDLE_STATUS_REPORT_STATUS_FLAGS::REPORTING_NODE_ACCEPTED_CUSTODY_OF_BUNDLE)) & statusFlagsAsUint8) {
        if (!m_timeOfCustodyAcceptanceOfBundle.Deserialize(serialization, &sdnvSize)) {
            return 0;
        }
        serialization += sdnvSize;
    }
    if ((static_cast<uint8_t>(BPV6_BUNDLE_STATUS_REPORT_STATUS_FLAGS::REPORTING_NODE_FORWARDED_BUNDLE)) & statusFlagsAsUint8) {
        if (!m_timeOfForwardingOfBundle.Deserialize(serialization, &sdnvSize)) {
            return 0;
        }
        serialization += sdnvSize;
    }
    if ((static_cast<uint8_t>(BPV6_BUNDLE_STATUS_REPORT_STATUS_FLAGS::REPORTING_NODE_DELIVERED_BUNDLE)) & statusFlagsAsUint8) {
        if (!m_timeOfDeliveryOfBundle.Deserialize(serialization, &sdnvSize)) {
            return 0;
        }
        serialization += sdnvSize;
    }
    if ((static_cast<uint8_t>(BPV6_BUNDLE_STATUS_REPORT_STATUS_FLAGS::REPORTING_NODE_DELETED_BUNDLE)) & statusFlagsAsUint8) {
        if (!m_timeOfDeletionOfBundle.Deserialize(serialization, &sdnvSize)) {
            return 0;
        }
        serialization += sdnvSize;
    }

    m_copyOfBundleCreationTimestampTimeSeconds = SdnvDecodeU64(serialization, &sdnvSize);
    if (sdnvSize == 0) {
        return 0; //failure
    }
    serialization += sdnvSize;

    m_copyOfBundleCreationTimestampSequenceNumber = SdnvDecodeU64(serialization, &sdnvSize);
    if (sdnvSize == 0) {
        return 0; //failure
    }
    serialization += sdnvSize;

    const uint32_t lengthEidStr = SdnvDecodeU32(serialization, &sdnvSize);
    if (sdnvSize == 0) {
        return 0; //failure
    }
    if ((lengthEidStr > UINT16_MAX) || (lengthEidStr < 5)) {
        return 0; //failure
    }
    serialization += sdnvSize;
    m_bundleSourceEid.assign((const char *)serialization, lengthEidStr);
    serialization += lengthEidStr;


    return static_cast<uint16_t>(serialization - serializationBase);
}

void BundleStatusReport::SetTimeOfReceiptOfBundleAndStatusFlag(const TimestampUtil::dtn_time_t & dtnTime) {
    m_timeOfReceiptOfBundle = dtnTime;
    const uint8_t statusFlagsAsUint8 = (static_cast<uint8_t>(m_statusFlags)) | (static_cast<uint8_t>(BPV6_BUNDLE_STATUS_REPORT_STATUS_FLAGS::REPORTING_NODE_RECEIVED_BUNDLE));
    m_statusFlags = static_cast<BPV6_BUNDLE_STATUS_REPORT_STATUS_FLAGS>(statusFlagsAsUint8);
}
void BundleStatusReport::SetTimeOfCustodyAcceptanceOfBundleAndStatusFlag(const TimestampUtil::dtn_time_t & dtnTime) {
    m_timeOfCustodyAcceptanceOfBundle = dtnTime;
    const uint8_t statusFlagsAsUint8 = (static_cast<uint8_t>(m_statusFlags)) | (static_cast<uint8_t>(BPV6_BUNDLE_STATUS_REPORT_STATUS_FLAGS::REPORTING_NODE_ACCEPTED_CUSTODY_OF_BUNDLE));
    m_statusFlags = static_cast<BPV6_BUNDLE_STATUS_REPORT_STATUS_FLAGS>(statusFlagsAsUint8);
}
void BundleStatusReport::SetTimeOfForwardingOfBundleAndStatusFlag(const TimestampUtil::dtn_time_t & dtnTime) {
    m_timeOfForwardingOfBundle = dtnTime;
    const uint8_t statusFlagsAsUint8 = (static_cast<uint8_t>(m_statusFlags)) | (static_cast<uint8_t>(BPV6_BUNDLE_STATUS_REPORT_STATUS_FLAGS::REPORTING_NODE_FORWARDED_BUNDLE));
    m_statusFlags = static_cast<BPV6_BUNDLE_STATUS_REPORT_STATUS_FLAGS>(statusFlagsAsUint8);
}
void BundleStatusReport::SetTimeOfDeliveryOfBundleAndStatusFlag(const TimestampUtil::dtn_time_t & dtnTime) {
    m_timeOfDeliveryOfBundle = dtnTime;
    const uint8_t statusFlagsAsUint8 = (static_cast<uint8_t>(m_statusFlags)) | (static_cast<uint8_t>(BPV6_BUNDLE_STATUS_REPORT_STATUS_FLAGS::REPORTING_NODE_DELIVERED_BUNDLE));
    m_statusFlags = static_cast<BPV6_BUNDLE_STATUS_REPORT_STATUS_FLAGS>(statusFlagsAsUint8);
}
void BundleStatusReport::SetTimeOfDeletionOfBundleAndStatusFlag(const TimestampUtil::dtn_time_t & dtnTime) {
    m_timeOfDeletionOfBundle = dtnTime;
    const uint8_t statusFlagsAsUint8 = (static_cast<uint8_t>(m_statusFlags)) | (static_cast<uint8_t>(BPV6_BUNDLE_STATUS_REPORT_STATUS_FLAGS::REPORTING_NODE_DELETED_BUNDLE));
    m_statusFlags = static_cast<BPV6_BUNDLE_STATUS_REPORT_STATUS_FLAGS>(statusFlagsAsUint8);
}




CustodySignal::CustodySignal() :
    m_statusFlagsPlus7bitReasonCode(0),
    m_isFragment(false),
    m_fragmentOffsetIfPresent(0),
    m_fragmentLengthIfPresent(0),
    m_copyOfBundleCreationTimestampTimeSeconds(0),
    m_copyOfBundleCreationTimestampSequenceNumber(0)
{ } //a default constructor: X()
CustodySignal::~CustodySignal() { } //a destructor: ~X()
CustodySignal::CustodySignal(const CustodySignal& o) :
    m_statusFlagsPlus7bitReasonCode(o.m_statusFlagsPlus7bitReasonCode),
    m_isFragment(o.m_isFragment),
    m_fragmentOffsetIfPresent(o.m_fragmentOffsetIfPresent),
    m_fragmentLengthIfPresent(o.m_fragmentLengthIfPresent),
    m_timeOfSignalGeneration(o.m_timeOfSignalGeneration),
    m_copyOfBundleCreationTimestampTimeSeconds(o.m_copyOfBundleCreationTimestampTimeSeconds),
    m_copyOfBundleCreationTimestampSequenceNumber(o.m_copyOfBundleCreationTimestampSequenceNumber),
    m_bundleSourceEid(o.m_bundleSourceEid)
{ } //a copy constructor: X(const X&)
CustodySignal::CustodySignal(CustodySignal&& o) :
    m_statusFlagsPlus7bitReasonCode(o.m_statusFlagsPlus7bitReasonCode),
    m_isFragment(o.m_isFragment),
    m_fragmentOffsetIfPresent(o.m_fragmentOffsetIfPresent),
    m_fragmentLengthIfPresent(o.m_fragmentLengthIfPresent),
    m_timeOfSignalGeneration(std::move(o.m_timeOfSignalGeneration)),
    m_copyOfBundleCreationTimestampTimeSeconds(o.m_copyOfBundleCreationTimestampTimeSeconds),
    m_copyOfBundleCreationTimestampSequenceNumber(o.m_copyOfBundleCreationTimestampSequenceNumber),
    m_bundleSourceEid(std::move(o.m_bundleSourceEid)) { } //a move constructor: X(X&&)
CustodySignal& CustodySignal::operator=(const CustodySignal& o) { //a copy assignment: operator=(const X&)
    m_statusFlagsPlus7bitReasonCode = o.m_statusFlagsPlus7bitReasonCode;
    m_isFragment = o.m_isFragment;
    m_fragmentOffsetIfPresent = o.m_fragmentOffsetIfPresent;
    m_fragmentLengthIfPresent = o.m_fragmentLengthIfPresent;
    m_timeOfSignalGeneration = o.m_timeOfSignalGeneration;
    m_copyOfBundleCreationTimestampTimeSeconds = o.m_copyOfBundleCreationTimestampTimeSeconds;
    m_copyOfBundleCreationTimestampSequenceNumber = o.m_copyOfBundleCreationTimestampSequenceNumber;
    m_bundleSourceEid = o.m_bundleSourceEid;
    return *this;
}
CustodySignal& CustodySignal::operator=(CustodySignal && o) { //a move assignment: operator=(X&&)
    m_statusFlagsPlus7bitReasonCode = o.m_statusFlagsPlus7bitReasonCode;
    m_isFragment = o.m_isFragment;
    m_fragmentOffsetIfPresent = o.m_fragmentOffsetIfPresent;
    m_fragmentLengthIfPresent = o.m_fragmentLengthIfPresent;
    m_timeOfSignalGeneration = std::move(o.m_timeOfSignalGeneration);
    m_copyOfBundleCreationTimestampTimeSeconds = o.m_copyOfBundleCreationTimestampTimeSeconds;
    m_copyOfBundleCreationTimestampSequenceNumber = o.m_copyOfBundleCreationTimestampSequenceNumber;
    m_bundleSourceEid = std::move(o.m_bundleSourceEid);
    return *this;
}
bool CustodySignal::operator==(const CustodySignal & o) const {
    return
        (m_statusFlagsPlus7bitReasonCode == o.m_statusFlagsPlus7bitReasonCode) &&
        (m_isFragment == o.m_isFragment) &&
        (m_fragmentOffsetIfPresent == o.m_fragmentOffsetIfPresent) &&
        (m_fragmentLengthIfPresent == o.m_fragmentLengthIfPresent) &&
        (m_timeOfSignalGeneration == o.m_timeOfSignalGeneration) &&
        (m_copyOfBundleCreationTimestampTimeSeconds == o.m_copyOfBundleCreationTimestampTimeSeconds) &&
        (m_copyOfBundleCreationTimestampSequenceNumber == o.m_copyOfBundleCreationTimestampSequenceNumber) &&
        (m_bundleSourceEid == o.m_bundleSourceEid);
}
bool CustodySignal::operator!=(const CustodySignal & o) const {
    return !(*this == o);
}
void CustodySignal::Reset() { //a copy assignment: operator=(const X&)
    m_statusFlagsPlus7bitReasonCode = 0;
    m_isFragment = false;
    m_fragmentOffsetIfPresent = 0;
    m_fragmentLengthIfPresent = 0;

    m_timeOfSignalGeneration.SetZero();
    m_copyOfBundleCreationTimestampTimeSeconds = 0;
    m_copyOfBundleCreationTimestampSequenceNumber = 0;
    m_bundleSourceEid.clear();
}

uint64_t CustodySignal::Serialize(uint8_t * buffer) const { //use MAX_SERIALIZATION_SIZE sized buffer
    uint8_t * const serializationBase = buffer;

    *buffer++ = ((static_cast<uint8_t>(BPV6_ADMINISTRATIVE_RECORD_TYPES::CUSTODY_SIGNAL)) << 4) | (static_cast<uint8_t>(m_isFragment)); // works because BUNDLE_IS_A_FRAGMENT == 1
    *buffer++ = m_statusFlagsPlus7bitReasonCode;

    if (m_isFragment) {
        buffer += SdnvEncodeU64(buffer, m_fragmentOffsetIfPresent);
        buffer += SdnvEncodeU64(buffer, m_fragmentLengthIfPresent);
    }
    
    buffer += m_timeOfSignalGeneration.Serialize(buffer);
    

    buffer += SdnvEncodeU64(buffer, m_copyOfBundleCreationTimestampTimeSeconds);
    buffer += SdnvEncodeU64(buffer, m_copyOfBundleCreationTimestampSequenceNumber);

    const uint32_t lengthEidStr = static_cast<uint32_t>(m_bundleSourceEid.length());
    if (lengthEidStr <= 127) {
        *buffer++ = static_cast<uint8_t>(lengthEidStr);
    }
    else {
        buffer += SdnvEncodeU32(buffer, lengthEidStr); //work with both dtn and ipn scheme
    }
    //*buffer++ = static_cast<uint8_t>(lengthEidStr); //will always be a 1 byte sdnv
    memcpy(buffer, m_bundleSourceEid.data(), lengthEidStr);
    buffer += lengthEidStr;
    return buffer - serializationBase;
}

uint16_t CustodySignal::Deserialize(const uint8_t * serialization) {
    uint8_t sdnvSize;
    const uint8_t * const serializationBase = serialization;
    Reset();

    const BPV6_ADMINISTRATIVE_RECORD_TYPES adminRecordType = static_cast<BPV6_ADMINISTRATIVE_RECORD_TYPES>(*serialization >> 4);
    if (adminRecordType != BPV6_ADMINISTRATIVE_RECORD_TYPES::CUSTODY_SIGNAL) {
        return 0; //failure
    }
    m_isFragment = ((*serialization & 1) != 0); // works because BUNDLE_IS_A_FRAGMENT == 1
    ++serialization;

    m_statusFlagsPlus7bitReasonCode = *serialization++;

    if (m_isFragment) {
        m_fragmentOffsetIfPresent = SdnvDecodeU64(serialization, &sdnvSize);
        if (sdnvSize == 0) {
            return 0; //failure
        }
        serialization += sdnvSize;

        m_fragmentLengthIfPresent = SdnvDecodeU64(serialization, &sdnvSize);
        if (sdnvSize == 0) {
            return 0; //failure
        }
        serialization += sdnvSize;
    }


    if (!m_timeOfSignalGeneration.Deserialize(serialization, &sdnvSize)) {
        return 0;
    }
    serialization += sdnvSize;
    

    m_copyOfBundleCreationTimestampTimeSeconds = SdnvDecodeU64(serialization, &sdnvSize);
    if (sdnvSize == 0) {
        return 0; //failure
    }
    serialization += sdnvSize;

    m_copyOfBundleCreationTimestampSequenceNumber = SdnvDecodeU64(serialization, &sdnvSize);
    if (sdnvSize == 0) {
        return 0; //failure
    }
    serialization += sdnvSize;

    const uint32_t lengthEidStr = SdnvDecodeU32(serialization, &sdnvSize);
    if (sdnvSize == 0) {
        return 0; //failure
    }
    if ((lengthEidStr > UINT16_MAX) || (lengthEidStr < 5)) {
        return 0; //failure
    }
    serialization += sdnvSize;
    m_bundleSourceEid.assign((const char *)serialization, lengthEidStr);
    serialization += lengthEidStr;


    return static_cast<uint16_t>(serialization - serializationBase);
}

void CustodySignal::SetTimeOfSignalGeneration(const TimestampUtil::dtn_time_t & dtnTime) {
    m_timeOfSignalGeneration = dtnTime;
}
void CustodySignal::SetCustodyTransferStatusAndReason(bool custodyTransferSucceeded, BPV6_CUSTODY_SIGNAL_REASON_CODES_7BIT reasonCode7bit) {
    m_statusFlagsPlus7bitReasonCode = (static_cast<uint8_t>(reasonCode7bit)) | ((static_cast<uint8_t>(custodyTransferSucceeded)) << 7); //*cursor = csig->reasonCode | (csig->succeeded << 7);
}
bool CustodySignal::DidCustodyTransferSucceed() const {
    return ((m_statusFlagsPlus7bitReasonCode & 0x80) != 0);
}
BPV6_CUSTODY_SIGNAL_REASON_CODES_7BIT CustodySignal::GetReasonCode() const {
    return static_cast<BPV6_CUSTODY_SIGNAL_REASON_CODES_7BIT>(m_statusFlagsPlus7bitReasonCode & 0x7f);
}

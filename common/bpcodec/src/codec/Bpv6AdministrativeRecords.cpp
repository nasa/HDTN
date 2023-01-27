/**
 * @file Bpv6AdministrativeRecords.cpp
 * @author  Brian Tomko <brian.j.tomko@nasa.gov>
 *
 * @copyright Copyright © 2021 United States Government as represented by
 * the National Aeronautics and Space Administration.
 * No copyright is claimed in the United States under Title 17, U.S.Code.
 * All Other Rights Reserved.
 *
 * @section LICENSE
 * Released under the NASA Open Source Agreement (NOSA)
 * See LICENSE.md in the source root directory for more information.
 */

#include "codec/bpv6.h"
#include "Sdnv.h"
#include <boost/make_unique.hpp>

Bpv6AdministrativeRecord::Bpv6AdministrativeRecord() : Bpv6CanonicalBlock() { //a default constructor: X() //don't initialize anything for efficiency, use SetZero if required
    m_blockTypeCode = BPV6_BLOCK_TYPE_CODE::PAYLOAD;
    m_isFragment = false;
}
Bpv6AdministrativeRecord::~Bpv6AdministrativeRecord() { } //a destructor: ~X()
Bpv6AdministrativeRecord::Bpv6AdministrativeRecord(Bpv6AdministrativeRecord&& o) :
    Bpv6CanonicalBlock(std::move(o)),
    m_adminRecordTypeCode(o.m_adminRecordTypeCode),
    m_adminRecordContentPtr(std::move(o.m_adminRecordContentPtr)),
    m_isFragment(o.m_isFragment) { } //a move constructor: X(X&&)
Bpv6AdministrativeRecord& Bpv6AdministrativeRecord::operator=(Bpv6AdministrativeRecord && o) { //a move assignment: operator=(X&&)
    m_adminRecordTypeCode = o.m_adminRecordTypeCode;
    m_adminRecordContentPtr = std::move(o.m_adminRecordContentPtr);
    m_isFragment = o.m_isFragment;
    return static_cast<Bpv6AdministrativeRecord&>(Bpv6CanonicalBlock::operator=(std::move(o)));
}
bool Bpv6AdministrativeRecord::operator==(const Bpv6AdministrativeRecord & o) const {
    const bool initialValue = (m_adminRecordTypeCode == o.m_adminRecordTypeCode)
        && (m_isFragment == o.m_isFragment)
        && Bpv6CanonicalBlock::operator==(o);
    if (!initialValue) {
        return false;
    }
    if ((m_adminRecordContentPtr) && (o.m_adminRecordContentPtr)) { //both not null
        return m_adminRecordContentPtr->IsEqual(o.m_adminRecordContentPtr.get());
    }
    else if ((!m_adminRecordContentPtr) && (!o.m_adminRecordContentPtr)) { //both null
        return true;
    }
    else {
        return false;
    }
}
bool Bpv6AdministrativeRecord::operator!=(const Bpv6AdministrativeRecord & o) const {
    return !(*this == o);
}
void Bpv6AdministrativeRecord::SetZero() {
    Bpv6CanonicalBlock::SetZero();
    m_adminRecordTypeCode = BPV6_ADMINISTRATIVE_RECORD_TYPE_CODE::UNUSED_ZERO;
    m_blockTypeCode = BPV6_BLOCK_TYPE_CODE::PAYLOAD;
}
uint64_t Bpv6AdministrativeRecord::SerializeBpv6(uint8_t * serialization) {
    m_blockTypeSpecificDataPtr = NULL;
    m_blockTypeSpecificDataLength = GetCanonicalBlockTypeSpecificDataSerializationSize();
    const uint64_t serializationSizeCanonical = Bpv6CanonicalBlock::SerializeBpv6(serialization);
    uint64_t bufferSize = m_blockTypeSpecificDataLength;
    uint8_t * blockSpecificDataSerialization = m_blockTypeSpecificDataPtr;
    uint64_t thisSerializationSize;

    //Every administrative record consists of a four-bit record type code
    //followed by four bits of administrative record flags, followed by
    //record content in type-specific format.

    if (bufferSize == 0) {
        return 0;
    }
    *blockSpecificDataSerialization++ = ((static_cast<uint8_t>(m_adminRecordTypeCode)) << 4) | (static_cast<uint8_t>(m_isFragment)); // works because BUNDLE_IS_A_FRAGMENT == 1
    --bufferSize;

    if (m_adminRecordContentPtr) {
        thisSerializationSize = m_adminRecordContentPtr->SerializeBpv6(blockSpecificDataSerialization, bufferSize);
        blockSpecificDataSerialization += thisSerializationSize;
        //bufferSize -= thisSerializationSize; //not needed
    }

    return serializationSizeCanonical;
}

uint64_t Bpv6AdministrativeRecord::GetCanonicalBlockTypeSpecificDataSerializationSize() const {
    return 1 + //1 => m_adminRecordTypeCode with flags
        ((m_adminRecordContentPtr) ? m_adminRecordContentPtr->GetSerializationSize() : 0);
}


//serialization must be temporarily modifyable to zero crc and restore it
bool Bpv6AdministrativeRecord::Virtual_DeserializeExtensionBlockDataBpv6() {

    if (m_blockTypeSpecificDataPtr == NULL) {
        return false;
    }

    uint8_t * serialization = m_blockTypeSpecificDataPtr;
    uint64_t bufferSize = m_blockTypeSpecificDataLength;

    //Every administrative record consists of a four-bit record type code
    //followed by four bits of administrative record flags, followed by
    //record content in type-specific format.
    
    if (bufferSize < 1) { //for adminRecordType
        return false;
    }
    --bufferSize;

    m_adminRecordTypeCode = static_cast<BPV6_ADMINISTRATIVE_RECORD_TYPE_CODE>(*serialization >> 4);
    m_isFragment = (((*serialization) & 1) != 0); // works because BUNDLE_IS_A_FRAGMENT == 1
    ++serialization;

    if (m_adminRecordTypeCode == BPV6_ADMINISTRATIVE_RECORD_TYPE_CODE::BUNDLE_STATUS_REPORT) {
        std::unique_ptr<Bpv6AdministrativeRecordContentBundleStatusReport> bsr = boost::make_unique<Bpv6AdministrativeRecordContentBundleStatusReport>();
        bsr->m_isFragment = m_isFragment;
        m_adminRecordContentPtr = std::move(bsr);
    }
    else if (m_adminRecordTypeCode == BPV6_ADMINISTRATIVE_RECORD_TYPE_CODE::CUSTODY_SIGNAL) {
        std::unique_ptr<Bpv6AdministrativeRecordContentCustodySignal> cs = boost::make_unique<Bpv6AdministrativeRecordContentCustodySignal>();
        cs->m_isFragment = m_isFragment;
        m_adminRecordContentPtr = std::move(cs);
    }
    else if (m_adminRecordTypeCode == BPV6_ADMINISTRATIVE_RECORD_TYPE_CODE::AGGREGATE_CUSTODY_SIGNAL) {
        std::unique_ptr<Bpv6AdministrativeRecordContentAggregateCustodySignal> acs = boost::make_unique<Bpv6AdministrativeRecordContentAggregateCustodySignal>();
        m_adminRecordContentPtr = std::move(acs);
    }
    else {
        return false;
    }


    uint64_t tmpNumBytesTakenToDeserializeThisSpecificAdminRecordType;
    if (!m_adminRecordContentPtr->DeserializeBpv6(serialization, tmpNumBytesTakenToDeserializeThisSpecificAdminRecordType, bufferSize)) {
        return false;
    }
    //serialization += tmpNumBytesTakenToDeserializeThisSpecificAdminRecordType;
    bufferSize -= tmpNumBytesTakenToDeserializeThisSpecificAdminRecordType; //from value above
    return (bufferSize == 0);
}






Bpv6AdministrativeRecordContentBase::~Bpv6AdministrativeRecordContentBase() {}


Bpv6AdministrativeRecordContentBundleStatusReport::Bpv6AdministrativeRecordContentBundleStatusReport() :
    m_statusFlags(BPV6_BUNDLE_STATUS_REPORT_STATUS_FLAGS::NO_FLAGS_SET),
    m_reasonCode(BPV6_BUNDLE_STATUS_REPORT_REASON_CODES::NO_ADDITIONAL_INFORMATION),
    m_isFragment(false),
    m_fragmentOffsetIfPresent(0),
    m_fragmentLengthIfPresent(0)
    { } //a default constructor: X()
Bpv6AdministrativeRecordContentBundleStatusReport::~Bpv6AdministrativeRecordContentBundleStatusReport() { } //a destructor: ~X()
Bpv6AdministrativeRecordContentBundleStatusReport::Bpv6AdministrativeRecordContentBundleStatusReport(const Bpv6AdministrativeRecordContentBundleStatusReport& o) :
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
    m_copyOfBundleCreationTimestamp(o.m_copyOfBundleCreationTimestamp),
    m_bundleSourceEid(o.m_bundleSourceEid)
{ } //a copy constructor: X(const X&)
Bpv6AdministrativeRecordContentBundleStatusReport::Bpv6AdministrativeRecordContentBundleStatusReport(Bpv6AdministrativeRecordContentBundleStatusReport&& o) :
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
    m_copyOfBundleCreationTimestamp(std::move(o.m_copyOfBundleCreationTimestamp)),
    m_bundleSourceEid(std::move(o.m_bundleSourceEid)) { } //a move constructor: X(X&&)
Bpv6AdministrativeRecordContentBundleStatusReport& Bpv6AdministrativeRecordContentBundleStatusReport::operator=(const Bpv6AdministrativeRecordContentBundleStatusReport& o) { //a copy assignment: operator=(const X&)
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
    m_copyOfBundleCreationTimestamp = o.m_copyOfBundleCreationTimestamp;
    m_bundleSourceEid = o.m_bundleSourceEid;
    return *this;
}
Bpv6AdministrativeRecordContentBundleStatusReport& Bpv6AdministrativeRecordContentBundleStatusReport::operator=(Bpv6AdministrativeRecordContentBundleStatusReport && o) { //a move assignment: operator=(X&&)
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
    m_copyOfBundleCreationTimestamp = std::move(o.m_copyOfBundleCreationTimestamp);
    m_bundleSourceEid = std::move(o.m_bundleSourceEid);
    return *this;
}
bool Bpv6AdministrativeRecordContentBundleStatusReport::operator==(const Bpv6AdministrativeRecordContentBundleStatusReport & o) const {
    return
        (m_statusFlags == o.m_statusFlags) &&
        (m_reasonCode == o.m_reasonCode) &&
        (m_isFragment == o.m_isFragment) &&
        ((m_isFragment) ? (m_fragmentOffsetIfPresent == o.m_fragmentOffsetIfPresent) : true) &&
        ((m_isFragment) ? (m_fragmentLengthIfPresent == o.m_fragmentLengthIfPresent) : true) &&
        (((BPV6_BUNDLE_STATUS_REPORT_STATUS_FLAGS::REPORTING_NODE_RECEIVED_BUNDLE & m_statusFlags) != BPV6_BUNDLE_STATUS_REPORT_STATUS_FLAGS::NO_FLAGS_SET) ? (m_timeOfReceiptOfBundle == o.m_timeOfReceiptOfBundle) : true) &&
        (((BPV6_BUNDLE_STATUS_REPORT_STATUS_FLAGS::REPORTING_NODE_ACCEPTED_CUSTODY_OF_BUNDLE & m_statusFlags) != BPV6_BUNDLE_STATUS_REPORT_STATUS_FLAGS::NO_FLAGS_SET) ? (m_timeOfCustodyAcceptanceOfBundle == o.m_timeOfCustodyAcceptanceOfBundle) : true) &&
        (((BPV6_BUNDLE_STATUS_REPORT_STATUS_FLAGS::REPORTING_NODE_FORWARDED_BUNDLE & m_statusFlags) != BPV6_BUNDLE_STATUS_REPORT_STATUS_FLAGS::NO_FLAGS_SET) ? (m_timeOfForwardingOfBundle == o.m_timeOfForwardingOfBundle) : true) &&
        (((BPV6_BUNDLE_STATUS_REPORT_STATUS_FLAGS::REPORTING_NODE_DELIVERED_BUNDLE & m_statusFlags) != BPV6_BUNDLE_STATUS_REPORT_STATUS_FLAGS::NO_FLAGS_SET) ? (m_timeOfDeliveryOfBundle == o.m_timeOfDeliveryOfBundle) : true) &&
        (((BPV6_BUNDLE_STATUS_REPORT_STATUS_FLAGS::REPORTING_NODE_DELETED_BUNDLE & m_statusFlags) != BPV6_BUNDLE_STATUS_REPORT_STATUS_FLAGS::NO_FLAGS_SET) ? (m_timeOfDeletionOfBundle == o.m_timeOfDeletionOfBundle) : true) &&
        (m_copyOfBundleCreationTimestamp == o.m_copyOfBundleCreationTimestamp) &&
        (m_bundleSourceEid == o.m_bundleSourceEid);
}
bool Bpv6AdministrativeRecordContentBundleStatusReport::operator!=(const Bpv6AdministrativeRecordContentBundleStatusReport & o) const {
    return !(*this == o);
}
bool Bpv6AdministrativeRecordContentBundleStatusReport::IsEqual(const Bpv6AdministrativeRecordContentBase * otherPtr) const {
    if (const Bpv6AdministrativeRecordContentBundleStatusReport * asBsrPtr = dynamic_cast<const Bpv6AdministrativeRecordContentBundleStatusReport*>(otherPtr)) {
        return ((*asBsrPtr) == (*this));
    }
    else {
        return false;
    }
}
void Bpv6AdministrativeRecordContentBundleStatusReport::Reset() { //a copy assignment: operator=(const X&)
    m_statusFlags = BPV6_BUNDLE_STATUS_REPORT_STATUS_FLAGS::NO_FLAGS_SET;
    m_reasonCode = BPV6_BUNDLE_STATUS_REPORT_REASON_CODES::NO_ADDITIONAL_INFORMATION;
    m_isFragment = false;
    m_fragmentOffsetIfPresent = 0;
    m_fragmentLengthIfPresent = 0;
    
    m_timeOfReceiptOfBundle.SetZero();
    m_timeOfCustodyAcceptanceOfBundle.SetZero();
    m_timeOfForwardingOfBundle.SetZero();
    m_timeOfDeliveryOfBundle.SetZero();
    m_timeOfDeletionOfBundle.SetZero();
    m_copyOfBundleCreationTimestamp.SetZero();
    m_bundleSourceEid.clear();
}

uint64_t Bpv6AdministrativeRecordContentBundleStatusReport::SerializeBpv6(uint8_t * serialization, uint64_t bufferSize) const {
    const uint8_t * const serializationBase = serialization;

    uint64_t thisSerializationSize;

    /*
    +----------------+----------------+----------------+----------------+
    |  Status Flags  |  Reason code   |      Fragment offset (*) (if
    +----------------+----------------+----------------+----------------+
        present)     |      Fragment length (*) (if present)            |
    +----------------+----------------+----------------+----------------+
    |       Time of receipt of bundle X (a DTN time, if present)        |
    +----------------+----------------+----------------+----------------+
    |  Time of custody acceptance of bundle X (a DTN time, if present)  |
    +----------------+----------------+----------------+----------------+
    |     Time of forwarding of bundle X (a DTN time, if present)       |
    +----------------+----------------+----------------+----------------+
    |      Time of delivery of bundle X (a DTN time, if present)        |
    +----------------+----------------+----------------+----------------+
    |      Time of deletion of bundle X (a DTN time, if present)        |
    +----------------+----------------+----------------+----------------+
    |          Copy of bundle X's Creation Timestamp time (*)           |
    +----------------+----------------+----------------+----------------+
    |     Copy of bundle X's Creation Timestamp sequence number (*)     |
    +----------------+----------------+----------------+----------------+
    |      Length of X's source endpoint ID (*)        |   Source
    +----------------+---------------------------------+                +
                         endpoint ID of bundle X (variable)             |
    +----------------+----------------+----------------+----------------+
 */
    if (bufferSize < 2) {
        return 0;
    }
    bufferSize -= 2;
    *serialization++ = static_cast<uint8_t>(m_statusFlags);
    *serialization++ = static_cast<uint8_t>(m_reasonCode);

    if (m_isFragment) {
        thisSerializationSize = SdnvEncodeU64(serialization, m_fragmentOffsetIfPresent, bufferSize);
        serialization += thisSerializationSize;
        bufferSize -= thisSerializationSize;

        thisSerializationSize = SdnvEncodeU64(serialization, m_fragmentLengthIfPresent, bufferSize);
        serialization += thisSerializationSize;
        bufferSize -= thisSerializationSize;
    }
    if ((BPV6_BUNDLE_STATUS_REPORT_STATUS_FLAGS::REPORTING_NODE_RECEIVED_BUNDLE & m_statusFlags) != BPV6_BUNDLE_STATUS_REPORT_STATUS_FLAGS::NO_FLAGS_SET) {
        thisSerializationSize = m_timeOfReceiptOfBundle.SerializeBpv6(serialization, bufferSize);
        serialization += thisSerializationSize;
        bufferSize -= thisSerializationSize;
    }
    if ((BPV6_BUNDLE_STATUS_REPORT_STATUS_FLAGS::REPORTING_NODE_ACCEPTED_CUSTODY_OF_BUNDLE & m_statusFlags) != BPV6_BUNDLE_STATUS_REPORT_STATUS_FLAGS::NO_FLAGS_SET) {
        thisSerializationSize = m_timeOfCustodyAcceptanceOfBundle.SerializeBpv6(serialization, bufferSize);
        serialization += thisSerializationSize;
        bufferSize -= thisSerializationSize;
    }
    if ((BPV6_BUNDLE_STATUS_REPORT_STATUS_FLAGS::REPORTING_NODE_FORWARDED_BUNDLE & m_statusFlags) != BPV6_BUNDLE_STATUS_REPORT_STATUS_FLAGS::NO_FLAGS_SET) {
        thisSerializationSize = m_timeOfForwardingOfBundle.SerializeBpv6(serialization, bufferSize);
        serialization += thisSerializationSize;
        bufferSize -= thisSerializationSize;
    }
    if ((BPV6_BUNDLE_STATUS_REPORT_STATUS_FLAGS::REPORTING_NODE_DELIVERED_BUNDLE & m_statusFlags) != BPV6_BUNDLE_STATUS_REPORT_STATUS_FLAGS::NO_FLAGS_SET) {
        thisSerializationSize = m_timeOfDeliveryOfBundle.SerializeBpv6(serialization, bufferSize);
        serialization += thisSerializationSize;
        bufferSize -= thisSerializationSize;
    }
    if ((BPV6_BUNDLE_STATUS_REPORT_STATUS_FLAGS::REPORTING_NODE_DELETED_BUNDLE & m_statusFlags) != BPV6_BUNDLE_STATUS_REPORT_STATUS_FLAGS::NO_FLAGS_SET) {
        thisSerializationSize = m_timeOfDeletionOfBundle.SerializeBpv6(serialization, bufferSize);
        serialization += thisSerializationSize;
        bufferSize -= thisSerializationSize;
    }
    
    thisSerializationSize = m_copyOfBundleCreationTimestamp.SerializeBpv6(serialization, bufferSize);
    serialization += thisSerializationSize;
    bufferSize -= thisSerializationSize;

    const uint32_t lengthEidStr = static_cast<uint32_t>(m_bundleSourceEid.length());
    if (lengthEidStr <= 127) {
        if (bufferSize == 0) {
            return 0;
        }
        *serialization++ = static_cast<uint8_t>(lengthEidStr);
        --bufferSize;
    }
    else {
        thisSerializationSize = SdnvEncodeU32(serialization, lengthEidStr, bufferSize); //work with both dtn and ipn scheme
        serialization += thisSerializationSize;
        bufferSize -= thisSerializationSize;
    }
    //*buffer++ = static_cast<uint8_t>(lengthEidStr); //will always be a 1 byte sdnv
    if (bufferSize < lengthEidStr) {
        return 0;
    }
    memcpy(serialization, m_bundleSourceEid.data(), lengthEidStr);
    serialization += lengthEidStr;
    return serialization - serializationBase;
}
uint64_t Bpv6AdministrativeRecordContentBundleStatusReport::GetSerializationSize() const {
    uint64_t size = 2; //2 statusFlags + reasonCode
    if (m_isFragment) {
        size += SdnvGetNumBytesRequiredToEncode(m_fragmentOffsetIfPresent);
        size += SdnvGetNumBytesRequiredToEncode(m_fragmentLengthIfPresent);
    }
    if ((BPV6_BUNDLE_STATUS_REPORT_STATUS_FLAGS::REPORTING_NODE_RECEIVED_BUNDLE & m_statusFlags) != BPV6_BUNDLE_STATUS_REPORT_STATUS_FLAGS::NO_FLAGS_SET) {
        size += m_timeOfReceiptOfBundle.GetSerializationSizeBpv6();
    }
    if ((BPV6_BUNDLE_STATUS_REPORT_STATUS_FLAGS::REPORTING_NODE_ACCEPTED_CUSTODY_OF_BUNDLE & m_statusFlags) != BPV6_BUNDLE_STATUS_REPORT_STATUS_FLAGS::NO_FLAGS_SET) {
        size += m_timeOfCustodyAcceptanceOfBundle.GetSerializationSizeBpv6();
    }
    if ((BPV6_BUNDLE_STATUS_REPORT_STATUS_FLAGS::REPORTING_NODE_FORWARDED_BUNDLE & m_statusFlags) != BPV6_BUNDLE_STATUS_REPORT_STATUS_FLAGS::NO_FLAGS_SET) {
        size += m_timeOfForwardingOfBundle.GetSerializationSizeBpv6();
    }
    if ((BPV6_BUNDLE_STATUS_REPORT_STATUS_FLAGS::REPORTING_NODE_DELIVERED_BUNDLE & m_statusFlags) != BPV6_BUNDLE_STATUS_REPORT_STATUS_FLAGS::NO_FLAGS_SET) {
        size += m_timeOfDeliveryOfBundle.GetSerializationSizeBpv6();
    }
    if ((BPV6_BUNDLE_STATUS_REPORT_STATUS_FLAGS::REPORTING_NODE_DELETED_BUNDLE & m_statusFlags) != BPV6_BUNDLE_STATUS_REPORT_STATUS_FLAGS::NO_FLAGS_SET) {
        size += m_timeOfDeletionOfBundle.GetSerializationSizeBpv6();
    }
    size += m_copyOfBundleCreationTimestamp.GetSerializationSizeBpv6();
    size += SdnvGetNumBytesRequiredToEncode(m_bundleSourceEid.length());
    size += m_bundleSourceEid.length();
    
    return size;
}
//m_isFragment must be set before calling
bool Bpv6AdministrativeRecordContentBundleStatusReport::DeserializeBpv6(const uint8_t * serialization, uint64_t & numBytesTakenToDecode, uint64_t bufferSize) {
    uint8_t sdnvSize;
    const uint8_t * const serializationBase = serialization;
    //Reset(); //don't clear m_isFragment

    if (bufferSize < 2) { //for statusFlags, and reasonCode
        return false;
    }
    bufferSize -= 2;

    m_statusFlags = static_cast<BPV6_BUNDLE_STATUS_REPORT_STATUS_FLAGS>(*serialization++);
    m_reasonCode = static_cast<BPV6_BUNDLE_STATUS_REPORT_REASON_CODES>(*serialization++);
    if (m_isFragment) {
        m_fragmentOffsetIfPresent = SdnvDecodeU64(serialization, &sdnvSize, bufferSize);
        if (sdnvSize == 0) {
            return false; //failure
        }
        serialization += sdnvSize;
        bufferSize -= sdnvSize;

        m_fragmentLengthIfPresent = SdnvDecodeU64(serialization, &sdnvSize, bufferSize);
        if (sdnvSize == 0) {
            return false; //failure
        }
        serialization += sdnvSize;
        bufferSize -= sdnvSize;
    }
    
    if ((BPV6_BUNDLE_STATUS_REPORT_STATUS_FLAGS::REPORTING_NODE_RECEIVED_BUNDLE & m_statusFlags) != BPV6_BUNDLE_STATUS_REPORT_STATUS_FLAGS::NO_FLAGS_SET) {
        if (!m_timeOfReceiptOfBundle.DeserializeBpv6(serialization, &sdnvSize, bufferSize)) {
            return false;
        }
        serialization += sdnvSize;
        bufferSize -= sdnvSize;
    }
    if ((BPV6_BUNDLE_STATUS_REPORT_STATUS_FLAGS::REPORTING_NODE_ACCEPTED_CUSTODY_OF_BUNDLE & m_statusFlags) != BPV6_BUNDLE_STATUS_REPORT_STATUS_FLAGS::NO_FLAGS_SET) {
        if (!m_timeOfCustodyAcceptanceOfBundle.DeserializeBpv6(serialization, &sdnvSize, bufferSize)) {
            return false;
        }
        serialization += sdnvSize;
        bufferSize -= sdnvSize;
    }
    if ((BPV6_BUNDLE_STATUS_REPORT_STATUS_FLAGS::REPORTING_NODE_FORWARDED_BUNDLE & m_statusFlags) != BPV6_BUNDLE_STATUS_REPORT_STATUS_FLAGS::NO_FLAGS_SET) {
        if (!m_timeOfForwardingOfBundle.DeserializeBpv6(serialization, &sdnvSize, bufferSize)) {
            return false;
        }
        serialization += sdnvSize;
        bufferSize -= sdnvSize;
    }
    if ((BPV6_BUNDLE_STATUS_REPORT_STATUS_FLAGS::REPORTING_NODE_DELIVERED_BUNDLE & m_statusFlags) != BPV6_BUNDLE_STATUS_REPORT_STATUS_FLAGS::NO_FLAGS_SET) {
        if (!m_timeOfDeliveryOfBundle.DeserializeBpv6(serialization, &sdnvSize, bufferSize)) {
            return false;
        }
        serialization += sdnvSize;
        bufferSize -= sdnvSize;
    }
    if ((BPV6_BUNDLE_STATUS_REPORT_STATUS_FLAGS::REPORTING_NODE_DELETED_BUNDLE & m_statusFlags) != BPV6_BUNDLE_STATUS_REPORT_STATUS_FLAGS::NO_FLAGS_SET) {
        if (!m_timeOfDeletionOfBundle.DeserializeBpv6(serialization, &sdnvSize, bufferSize)) {
            return false;
        }
        serialization += sdnvSize;
        bufferSize -= sdnvSize;
    }

    if (!m_copyOfBundleCreationTimestamp.DeserializeBpv6(serialization, &sdnvSize, bufferSize)) {
        return false;
    }
    serialization += sdnvSize;
    bufferSize -= sdnvSize;


    const uint32_t lengthEidStr = SdnvDecodeU32(serialization, &sdnvSize, bufferSize);
    if (sdnvSize == 0) {
        return false; //failure
    }
    if ((lengthEidStr > UINT16_MAX) || (lengthEidStr < 5)) {
        return false; //failure
    }
    serialization += sdnvSize;
    bufferSize -= sdnvSize;

    if (bufferSize < lengthEidStr) {
        return false;
    }

    m_bundleSourceEid.assign((const char *)serialization, lengthEidStr);
    serialization += lengthEidStr;
    //bufferSize -= lengthEidStr; //not needed

    numBytesTakenToDecode = (serialization - serializationBase);
    return true;
}

void Bpv6AdministrativeRecordContentBundleStatusReport::SetTimeOfReceiptOfBundleAndStatusFlag(const TimestampUtil::dtn_time_t & dtnTime) {
    m_timeOfReceiptOfBundle = dtnTime;
    m_statusFlags |= BPV6_BUNDLE_STATUS_REPORT_STATUS_FLAGS::REPORTING_NODE_RECEIVED_BUNDLE;
}
void Bpv6AdministrativeRecordContentBundleStatusReport::SetTimeOfCustodyAcceptanceOfBundleAndStatusFlag(const TimestampUtil::dtn_time_t & dtnTime) {
    m_timeOfCustodyAcceptanceOfBundle = dtnTime;
    m_statusFlags |= BPV6_BUNDLE_STATUS_REPORT_STATUS_FLAGS::REPORTING_NODE_ACCEPTED_CUSTODY_OF_BUNDLE;
}
void Bpv6AdministrativeRecordContentBundleStatusReport::SetTimeOfForwardingOfBundleAndStatusFlag(const TimestampUtil::dtn_time_t & dtnTime) {
    m_timeOfForwardingOfBundle = dtnTime;
    m_statusFlags |= BPV6_BUNDLE_STATUS_REPORT_STATUS_FLAGS::REPORTING_NODE_FORWARDED_BUNDLE;
}
void Bpv6AdministrativeRecordContentBundleStatusReport::SetTimeOfDeliveryOfBundleAndStatusFlag(const TimestampUtil::dtn_time_t & dtnTime) {
    m_timeOfDeliveryOfBundle = dtnTime;
    m_statusFlags |= BPV6_BUNDLE_STATUS_REPORT_STATUS_FLAGS::REPORTING_NODE_DELIVERED_BUNDLE;
}
void Bpv6AdministrativeRecordContentBundleStatusReport::SetTimeOfDeletionOfBundleAndStatusFlag(const TimestampUtil::dtn_time_t & dtnTime) {
    m_timeOfDeletionOfBundle = dtnTime;
    m_statusFlags |= BPV6_BUNDLE_STATUS_REPORT_STATUS_FLAGS::REPORTING_NODE_DELETED_BUNDLE;
}
bool Bpv6AdministrativeRecordContentBundleStatusReport::HasBundleStatusReportStatusFlagSet(const BPV6_BUNDLE_STATUS_REPORT_STATUS_FLAGS & flag) const {
    return ((m_statusFlags & flag) != BPV6_BUNDLE_STATUS_REPORT_STATUS_FLAGS::NO_FLAGS_SET);
}



Bpv6AdministrativeRecordContentCustodySignal::Bpv6AdministrativeRecordContentCustodySignal() :
    m_statusFlagsPlus7bitReasonCode(0),
    m_isFragment(false),
    m_fragmentOffsetIfPresent(0),
    m_fragmentLengthIfPresent(0)
{ } //a default constructor: X()
Bpv6AdministrativeRecordContentCustodySignal::~Bpv6AdministrativeRecordContentCustodySignal() { } //a destructor: ~X()
Bpv6AdministrativeRecordContentCustodySignal::Bpv6AdministrativeRecordContentCustodySignal(const Bpv6AdministrativeRecordContentCustodySignal& o) :
    m_statusFlagsPlus7bitReasonCode(o.m_statusFlagsPlus7bitReasonCode),
    m_isFragment(o.m_isFragment),
    m_fragmentOffsetIfPresent(o.m_fragmentOffsetIfPresent),
    m_fragmentLengthIfPresent(o.m_fragmentLengthIfPresent),
    m_timeOfSignalGeneration(o.m_timeOfSignalGeneration),
    m_copyOfBundleCreationTimestamp(o.m_copyOfBundleCreationTimestamp),
    m_bundleSourceEid(o.m_bundleSourceEid)
{ } //a copy constructor: X(const X&)
Bpv6AdministrativeRecordContentCustodySignal::Bpv6AdministrativeRecordContentCustodySignal(Bpv6AdministrativeRecordContentCustodySignal&& o) :
    m_statusFlagsPlus7bitReasonCode(o.m_statusFlagsPlus7bitReasonCode),
    m_isFragment(o.m_isFragment),
    m_fragmentOffsetIfPresent(o.m_fragmentOffsetIfPresent),
    m_fragmentLengthIfPresent(o.m_fragmentLengthIfPresent),
    m_timeOfSignalGeneration(std::move(o.m_timeOfSignalGeneration)),
    m_copyOfBundleCreationTimestamp(std::move(o.m_copyOfBundleCreationTimestamp)),
    m_bundleSourceEid(std::move(o.m_bundleSourceEid)) { } //a move constructor: X(X&&)
Bpv6AdministrativeRecordContentCustodySignal& Bpv6AdministrativeRecordContentCustodySignal::operator=(const Bpv6AdministrativeRecordContentCustodySignal& o) { //a copy assignment: operator=(const X&)
    m_statusFlagsPlus7bitReasonCode = o.m_statusFlagsPlus7bitReasonCode;
    m_isFragment = o.m_isFragment;
    m_fragmentOffsetIfPresent = o.m_fragmentOffsetIfPresent;
    m_fragmentLengthIfPresent = o.m_fragmentLengthIfPresent;
    m_timeOfSignalGeneration = o.m_timeOfSignalGeneration;
    m_copyOfBundleCreationTimestamp = o.m_copyOfBundleCreationTimestamp;
    m_bundleSourceEid = o.m_bundleSourceEid;
    return *this;
}
Bpv6AdministrativeRecordContentCustodySignal& Bpv6AdministrativeRecordContentCustodySignal::operator=(Bpv6AdministrativeRecordContentCustodySignal && o) { //a move assignment: operator=(X&&)
    m_statusFlagsPlus7bitReasonCode = o.m_statusFlagsPlus7bitReasonCode;
    m_isFragment = o.m_isFragment;
    m_fragmentOffsetIfPresent = o.m_fragmentOffsetIfPresent;
    m_fragmentLengthIfPresent = o.m_fragmentLengthIfPresent;
    m_timeOfSignalGeneration = std::move(o.m_timeOfSignalGeneration);
    m_copyOfBundleCreationTimestamp = std::move(o.m_copyOfBundleCreationTimestamp);
    m_bundleSourceEid = std::move(o.m_bundleSourceEid);
    return *this;
}
bool Bpv6AdministrativeRecordContentCustodySignal::operator==(const Bpv6AdministrativeRecordContentCustodySignal & o) const {
    return
        (m_statusFlagsPlus7bitReasonCode == o.m_statusFlagsPlus7bitReasonCode) &&
        (m_isFragment == o.m_isFragment) &&
        ((m_isFragment) ? (m_fragmentOffsetIfPresent == o.m_fragmentOffsetIfPresent) : true) &&
        ((m_isFragment) ? (m_fragmentLengthIfPresent == o.m_fragmentLengthIfPresent) : true) &&
        (m_timeOfSignalGeneration == o.m_timeOfSignalGeneration) &&
        (m_copyOfBundleCreationTimestamp == o.m_copyOfBundleCreationTimestamp) &&
        (m_bundleSourceEid == o.m_bundleSourceEid);
}
bool Bpv6AdministrativeRecordContentCustodySignal::operator!=(const Bpv6AdministrativeRecordContentCustodySignal & o) const {
    return !(*this == o);
}
bool Bpv6AdministrativeRecordContentCustodySignal::IsEqual(const Bpv6AdministrativeRecordContentBase * otherPtr) const {
    if (const Bpv6AdministrativeRecordContentCustodySignal * asCsPtr = dynamic_cast<const Bpv6AdministrativeRecordContentCustodySignal*>(otherPtr)) {
        return ((*asCsPtr) == (*this));
    }
    else {
        return false;
    }
}
void Bpv6AdministrativeRecordContentCustodySignal::Reset() { //a copy assignment: operator=(const X&)
    m_statusFlagsPlus7bitReasonCode = 0;
    m_isFragment = false;
    m_fragmentOffsetIfPresent = 0;
    m_fragmentLengthIfPresent = 0;

    m_timeOfSignalGeneration.SetZero();
    m_copyOfBundleCreationTimestamp.SetZero();
    m_bundleSourceEid.clear();
}

uint64_t Bpv6AdministrativeRecordContentCustodySignal::SerializeBpv6(uint8_t * serialization, uint64_t bufferSize) const {
    const uint8_t * const serializationBase = serialization;

    uint64_t thisSerializationSize;

    /*
   +----------------+----------------+----------------+----------------+
   |     Status     |      Fragment offset (*) (if present)            |
   +----------------+----------------+----------------+----------------+
   |                   Fragment length (*) (if present)                |
   +----------------+----------------+----------------+----------------+
   |                   Time of signal (a DTN time)                     |
   +----------------+----------------+----------------+----------------+
   |          Copy of bundle X's Creation Timestamp time (*)           |
   +----------------+----------------+----------------+----------------+
   |     Copy of bundle X's Creation Timestamp sequence number (*)     |
   +----------------+----------------+----------------+----------------+
   |      Length of X's source endpoint ID (*)        |   Source
   +----------------+---------------------------------+                +
                        endpoint ID of bundle X (variable)             |
   +----------------+----------------+----------------+----------------+
    */

    if (bufferSize < 1) {
        return 0;
    }
    bufferSize -= 1;
    *serialization++ = m_statusFlagsPlus7bitReasonCode;

    if (m_isFragment) {
        thisSerializationSize = SdnvEncodeU64(serialization, m_fragmentOffsetIfPresent, bufferSize);
        serialization += thisSerializationSize;
        bufferSize -= thisSerializationSize;

        thisSerializationSize = SdnvEncodeU64(serialization, m_fragmentLengthIfPresent, bufferSize);
        serialization += thisSerializationSize;
        bufferSize -= thisSerializationSize;
    }
    
    thisSerializationSize = m_timeOfSignalGeneration.SerializeBpv6(serialization, bufferSize);
    serialization += thisSerializationSize;
    bufferSize -= thisSerializationSize;
    

    thisSerializationSize = m_copyOfBundleCreationTimestamp.SerializeBpv6(serialization, bufferSize);
    serialization += thisSerializationSize;
    bufferSize -= thisSerializationSize;

    const uint32_t lengthEidStr = static_cast<uint32_t>(m_bundleSourceEid.length());
    if (lengthEidStr <= 127) {
        if (bufferSize == 0) {
            return 0;
        }
        *serialization++ = static_cast<uint8_t>(lengthEidStr);
        --bufferSize;
    }
    else {
        thisSerializationSize = SdnvEncodeU32(serialization, lengthEidStr, bufferSize); //work with both dtn and ipn scheme
        serialization += thisSerializationSize;
        bufferSize -= thisSerializationSize;
    }
    //*buffer++ = static_cast<uint8_t>(lengthEidStr); //will always be a 1 byte sdnv
    if (bufferSize < lengthEidStr) {
        return 0;
    }
    memcpy(serialization, m_bundleSourceEid.data(), lengthEidStr);
    serialization += lengthEidStr;
    return serialization - serializationBase;
}
uint64_t Bpv6AdministrativeRecordContentCustodySignal::GetSerializationSize() const {
    uint64_t size = 1; //m_statusFlagsPlus7bitReasonCode
    if (m_isFragment) {
        size += SdnvGetNumBytesRequiredToEncode(m_fragmentOffsetIfPresent);
        size += SdnvGetNumBytesRequiredToEncode(m_fragmentLengthIfPresent);
    }
    size += m_timeOfSignalGeneration.GetSerializationSizeBpv6();
    size += m_copyOfBundleCreationTimestamp.GetSerializationSizeBpv6();
    size += SdnvGetNumBytesRequiredToEncode(m_bundleSourceEid.length());
    size += m_bundleSourceEid.length();

    return size;
}
//m_isFragment must be set before calling
bool Bpv6AdministrativeRecordContentCustodySignal::DeserializeBpv6(const uint8_t * serialization, uint64_t & numBytesTakenToDecode, uint64_t bufferSize) {
    uint8_t sdnvSize;
    const uint8_t * const serializationBase = serialization;
    //Reset(); //don't clear m_isFragment

    if (bufferSize < 1) { //for statusFlags, and reasonCode
        return false;
    }
    bufferSize -= 1;
    m_statusFlagsPlus7bitReasonCode = *serialization++;

    if (m_isFragment) {
        m_fragmentOffsetIfPresent = SdnvDecodeU64(serialization, &sdnvSize, bufferSize);
        if (sdnvSize == 0) {
            return false; //failure
        }
        serialization += sdnvSize;
        bufferSize -= sdnvSize;

        m_fragmentLengthIfPresent = SdnvDecodeU64(serialization, &sdnvSize, bufferSize);
        if (sdnvSize == 0) {
            return false; //failure
        }
        serialization += sdnvSize;
        bufferSize -= sdnvSize;
    }


    if (!m_timeOfSignalGeneration.DeserializeBpv6(serialization, &sdnvSize, bufferSize)) {
        return false;
    }
    serialization += sdnvSize;
    bufferSize -= sdnvSize;

    if (!m_copyOfBundleCreationTimestamp.DeserializeBpv6(serialization, &sdnvSize, bufferSize)) {
        return false;
    }
    serialization += sdnvSize;
    bufferSize -= sdnvSize;

    const uint32_t lengthEidStr = SdnvDecodeU32(serialization, &sdnvSize, bufferSize);
    if (sdnvSize == 0) {
        return false; //failure
    }
    if ((lengthEidStr > UINT16_MAX) || (lengthEidStr < 5)) {
        return false; //failure
    }
    serialization += sdnvSize;
    bufferSize -= sdnvSize;

    if (bufferSize < lengthEidStr) {
        return false;
    }

    m_bundleSourceEid.assign((const char *)serialization, lengthEidStr);
    serialization += lengthEidStr;
    //bufferSize -= lengthEidStr; //not needed

    numBytesTakenToDecode = (serialization - serializationBase);
    return true;
}

void Bpv6AdministrativeRecordContentCustodySignal::SetTimeOfSignalGeneration(const TimestampUtil::dtn_time_t & dtnTime) {
    m_timeOfSignalGeneration = dtnTime;
}
void Bpv6AdministrativeRecordContentCustodySignal::SetCustodyTransferStatusAndReason(bool custodyTransferSucceeded, BPV6_CUSTODY_SIGNAL_REASON_CODES_7BIT reasonCode7bit) {
    m_statusFlagsPlus7bitReasonCode = (static_cast<uint8_t>(reasonCode7bit)) | ((static_cast<uint8_t>(custodyTransferSucceeded)) << 7); //*cursor = csig->reasonCode | (csig->succeeded << 7);
}
bool Bpv6AdministrativeRecordContentCustodySignal::DidCustodyTransferSucceed() const {
    //Status: A 1-byte field containing a 1-bit "custody transfer
    //succeeded" flag followed by a 7-bit reason code explaining the
    //value of that flag.
    return ((m_statusFlagsPlus7bitReasonCode & 0x80) != 0);
}
BPV6_CUSTODY_SIGNAL_REASON_CODES_7BIT Bpv6AdministrativeRecordContentCustodySignal::GetReasonCode() const {
    return static_cast<BPV6_CUSTODY_SIGNAL_REASON_CODES_7BIT>(m_statusFlagsPlus7bitReasonCode & 0x7f);
}





Bpv6AdministrativeRecordContentAggregateCustodySignal::Bpv6AdministrativeRecordContentAggregateCustodySignal() :
    m_statusFlagsPlus7bitReasonCode(0) { } //a default constructor: X()
Bpv6AdministrativeRecordContentAggregateCustodySignal::~Bpv6AdministrativeRecordContentAggregateCustodySignal() { } //a destructor: ~X()
Bpv6AdministrativeRecordContentAggregateCustodySignal::Bpv6AdministrativeRecordContentAggregateCustodySignal(const Bpv6AdministrativeRecordContentAggregateCustodySignal& o) :
    m_statusFlagsPlus7bitReasonCode(o.m_statusFlagsPlus7bitReasonCode),
    m_custodyIdFills(o.m_custodyIdFills)
{ } //a copy constructor: X(const X&)
Bpv6AdministrativeRecordContentAggregateCustodySignal::Bpv6AdministrativeRecordContentAggregateCustodySignal(Bpv6AdministrativeRecordContentAggregateCustodySignal&& o) :
    m_statusFlagsPlus7bitReasonCode(o.m_statusFlagsPlus7bitReasonCode),
    m_custodyIdFills(std::move(o.m_custodyIdFills)) { } //a move constructor: X(X&&)
Bpv6AdministrativeRecordContentAggregateCustodySignal& Bpv6AdministrativeRecordContentAggregateCustodySignal::operator=(const Bpv6AdministrativeRecordContentAggregateCustodySignal& o) { //a copy assignment: operator=(const X&)
    m_statusFlagsPlus7bitReasonCode = o.m_statusFlagsPlus7bitReasonCode;
    m_custodyIdFills = o.m_custodyIdFills;
    return *this;
}
Bpv6AdministrativeRecordContentAggregateCustodySignal& Bpv6AdministrativeRecordContentAggregateCustodySignal::operator=(Bpv6AdministrativeRecordContentAggregateCustodySignal && o) { //a move assignment: operator=(X&&)
    m_statusFlagsPlus7bitReasonCode = o.m_statusFlagsPlus7bitReasonCode;
    m_custodyIdFills = std::move(o.m_custodyIdFills);
    return *this;
}
bool Bpv6AdministrativeRecordContentAggregateCustodySignal::operator==(const Bpv6AdministrativeRecordContentAggregateCustodySignal & o) const {
    return
        (m_statusFlagsPlus7bitReasonCode == o.m_statusFlagsPlus7bitReasonCode) &&
        (m_custodyIdFills == o.m_custodyIdFills);
}
bool Bpv6AdministrativeRecordContentAggregateCustodySignal::operator!=(const Bpv6AdministrativeRecordContentAggregateCustodySignal & o) const {
    return !(*this == o);
}
bool Bpv6AdministrativeRecordContentAggregateCustodySignal::IsEqual(const Bpv6AdministrativeRecordContentBase * otherPtr) const {
    if (const Bpv6AdministrativeRecordContentAggregateCustodySignal * asAcsPtr = dynamic_cast<const Bpv6AdministrativeRecordContentAggregateCustodySignal*>(otherPtr)) {
        return ((*asAcsPtr) == (*this));
    }
    else {
        return false;
    }
}
void Bpv6AdministrativeRecordContentAggregateCustodySignal::Reset() { //a copy assignment: operator=(const X&)
    //m_statusFlagsPlus7bitReasonCode = 0;
    m_custodyIdFills.clear();
}

uint64_t Bpv6AdministrativeRecordContentAggregateCustodySignal::SerializeBpv6(uint8_t * serialization, uint64_t bufferSize) const {
    const uint8_t * const serializationBase = serialization;

    /* https://public.ccsds.org/Pubs/734x2b1.pdf

   +----------------+----------------+----------------+----------------+
   |     Admin record 0x04     |      Status                           |
   +----------------+----------------+----------------+----------------+
   |    Left edge of first fill*   |  Length of first fill*            |
   +----------------+----------------+----------------+----------------+
   |  Difference between right edge of first |  Length of second fill* |
   |  fill and left edge of second fill*     |                         |
   +----------------+----------------+----------------+----------------+
   |                                ...                                |
   +----------------+----------------+----------------+----------------+
   |  Difference between right edge first |  Length of fill N*         |
   |  N-1 and left edge of fill N*        |                            |
   +----------------+----------------+----------------+----------------+
       * Field is an SDNV
  */

    if (bufferSize < 1) {
        return 0;
    }
    bufferSize -= 1;
    *serialization++ = m_statusFlagsPlus7bitReasonCode;

    serialization += SerializeFills(serialization, bufferSize);
    
    return serialization - serializationBase;
}
uint64_t Bpv6AdministrativeRecordContentAggregateCustodySignal::GetSerializationSize() const {
    return 1 + GetFillSerializedSize(); //1 => m_statusFlagsPlus7bitReasonCode
}
//m_isFragment must be set before calling
bool Bpv6AdministrativeRecordContentAggregateCustodySignal::DeserializeBpv6(const uint8_t * serialization, uint64_t & numBytesTakenToDecode, uint64_t bufferSize) {
    //Reset(); //don't clear m_isFragment

    if (bufferSize < 1) { //for statusFlags, and reasonCode
        return false;
    }
    bufferSize -= 1;
    m_statusFlagsPlus7bitReasonCode = *serialization++;

    uint64_t numBytesTakenToDecodeFills;
    if (!DeserializeFills(serialization, numBytesTakenToDecodeFills, bufferSize)) {
        return false;
    }
    
    numBytesTakenToDecode = (1 + numBytesTakenToDecodeFills);
    return true;
}

void Bpv6AdministrativeRecordContentAggregateCustodySignal::SetCustodyTransferStatusAndReason(bool custodyTransferSucceeded, BPV6_CUSTODY_SIGNAL_REASON_CODES_7BIT reasonCode7bit) {
    m_statusFlagsPlus7bitReasonCode = (static_cast<uint8_t>(reasonCode7bit)) | ((static_cast<uint8_t>(custodyTransferSucceeded)) << 7); //*cursor = csig->reasonCode | (csig->succeeded << 7);
}
bool Bpv6AdministrativeRecordContentAggregateCustodySignal::DidCustodyTransferSucceed() const {
    return ((m_statusFlagsPlus7bitReasonCode & 0x80) != 0);
}
BPV6_CUSTODY_SIGNAL_REASON_CODES_7BIT Bpv6AdministrativeRecordContentAggregateCustodySignal::GetReasonCode() const {
    return static_cast<BPV6_CUSTODY_SIGNAL_REASON_CODES_7BIT>(m_statusFlagsPlus7bitReasonCode & 0x7f);
}
//return number of fills
uint64_t Bpv6AdministrativeRecordContentAggregateCustodySignal::AddCustodyIdToFill(const uint64_t custodyId) {
    FragmentSet::InsertFragment(m_custodyIdFills, FragmentSet::data_fragment_t(custodyId, custodyId));
    return m_custodyIdFills.size();
}
//return number of fills
uint64_t Bpv6AdministrativeRecordContentAggregateCustodySignal::AddContiguousCustodyIdsToFill(const uint64_t firstCustodyId, const uint64_t lastCustodyId) {
    FragmentSet::InsertFragment(m_custodyIdFills, FragmentSet::data_fragment_t(firstCustodyId, lastCustodyId));
    return m_custodyIdFills.size();
}
uint64_t Bpv6AdministrativeRecordContentAggregateCustodySignal::SerializeFills(uint8_t * serialization, uint64_t bufferSize) const {
    uint8_t * const serializationBase = serialization;
    uint64_t rightEdgePrevious = 0; // ion code before loop: lastFill = 0;
    uint64_t thisSerializationSize;
    for (FragmentSet::data_fragment_set_t::const_iterator it = m_custodyIdFills.cbegin(); it != m_custodyIdFills.cend(); ++it) {
        thisSerializationSize = SdnvEncodeU64(serialization, it->beginIndex - rightEdgePrevious, bufferSize); //ion code in loop: encode startDelta = fill.start - lastFill
        serialization += thisSerializationSize;
        bufferSize -= thisSerializationSize;

        thisSerializationSize = SdnvEncodeU64(serialization, (it->endIndex + 1) - it->beginIndex, bufferSize); //ion code in loop: encode fill.length
        serialization += thisSerializationSize;
        bufferSize -= thisSerializationSize;

        rightEdgePrevious = it->endIndex; //ion code in loop: lastFill = fill.start + fill.length - 1;
    }
    return serialization - serializationBase; //returns length of sdnv encoded, if zero-length => (failure.. minimum 1 fill)
}
uint64_t Bpv6AdministrativeRecordContentAggregateCustodySignal::GetFillSerializedSize() const {
    uint64_t rightEdgePrevious = 0; // ion code before loop: lastFill = 0;
    uint64_t size = 0;
    for (FragmentSet::data_fragment_set_t::const_iterator it = m_custodyIdFills.cbegin(); it != m_custodyIdFills.cend(); ++it) {
        size += SdnvGetNumBytesRequiredToEncode(it->beginIndex - rightEdgePrevious); //ion code in loop: encode startDelta = fill.start - lastFill
        size += SdnvGetNumBytesRequiredToEncode((it->endIndex + 1) - it->beginIndex); //ion code in loop: encode fill.length
        rightEdgePrevious = it->endIndex; //ion code in loop: lastFill = fill.start + fill.length - 1;
    }
    return size;
}
bool Bpv6AdministrativeRecordContentAggregateCustodySignal::DeserializeFills(const uint8_t * serialization, uint64_t & numBytesTakenToDecode, uint64_t bufferSize) {
    const uint8_t * const serializationBase = serialization;
    uint8_t sdnvSize;
    int64_t bufferSizeSigned = static_cast<int64_t>(bufferSize);
    m_custodyIdFills.clear();
    uint64_t rightEdgePrevious = 0;
    while (true) {
        const uint64_t startDelta = SdnvDecodeU64(serialization, &sdnvSize, static_cast<uint64_t>(bufferSizeSigned));
        if (sdnvSize == 0) {
            return false; //failure
        }
        serialization += sdnvSize;
        bufferSizeSigned -= sdnvSize;
        if (bufferSizeSigned <= 0) {
            return false; //failure
        }
        const uint64_t leftEdge = startDelta + rightEdgePrevious;

        const uint64_t fillLength = SdnvDecodeU64(serialization, &sdnvSize, static_cast<uint64_t>(bufferSizeSigned));
        if (sdnvSize == 0) {
            return false; //failure
        }
        rightEdgePrevious = (leftEdge + fillLength) - 1; //using rightEdgePrevious as current rightEdge
        AddContiguousCustodyIdsToFill(leftEdge, rightEdgePrevious);
        serialization += sdnvSize;
        bufferSizeSigned -= sdnvSize;
        if (bufferSizeSigned <= 0) {
            numBytesTakenToDecode = (serialization - serializationBase);
            return (bufferSizeSigned == 0); //done and success
        }
    }
}

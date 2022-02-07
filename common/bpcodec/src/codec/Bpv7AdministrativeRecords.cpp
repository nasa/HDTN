/***************************************************************************
 * NASA Glenn Research Center, Cleveland, OH
 * Released under the NASA Open Source Agreement (NOSA)
 * May  2021
 *
 ****************************************************************************
 */
#include "codec/bpv7.h"
#include "CborUint.h"
#include <boost/format.hpp>
#include <boost/make_unique.hpp>

Bpv7AdministrativeRecord::Bpv7AdministrativeRecord() : Bpv7CanonicalBlock() { //a default constructor: X() //don't initialize anything for efficiency, use SetZero if required
    m_blockTypeCode = BPV7_BLOCK_TYPE_CODE::PAYLOAD;
    m_blockNumber = 1;
}
Bpv7AdministrativeRecord::~Bpv7AdministrativeRecord() { } //a destructor: ~X()
Bpv7AdministrativeRecord::Bpv7AdministrativeRecord(Bpv7AdministrativeRecord&& o) :
    Bpv7CanonicalBlock(std::move(o)),
    m_adminRecordTypeCode(o.m_adminRecordTypeCode),
    m_adminRecordContentPtr(std::move(o.m_adminRecordContentPtr)) { } //a move constructor: X(X&&)
Bpv7AdministrativeRecord& Bpv7AdministrativeRecord::operator=(Bpv7AdministrativeRecord && o) { //a move assignment: operator=(X&&)
    m_adminRecordTypeCode = o.m_adminRecordTypeCode;
    m_adminRecordContentPtr = std::move(o.m_adminRecordContentPtr);
    return static_cast<Bpv7AdministrativeRecord&>(Bpv7CanonicalBlock::operator=(std::move(o)));
}
bool Bpv7AdministrativeRecord::operator==(const Bpv7AdministrativeRecord & o) const {
    const bool initialValue = (m_adminRecordTypeCode == o.m_adminRecordTypeCode)
        && Bpv7CanonicalBlock::operator==(o);
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
bool Bpv7AdministrativeRecord::operator!=(const Bpv7AdministrativeRecord & o) const {
    return !(*this == o);
}
void Bpv7AdministrativeRecord::SetZero() {
    Bpv7CanonicalBlock::SetZero();
    m_adminRecordTypeCode = BPV7_ADMINISTRATIVE_RECORD_TYPE_CODE::UNUSED_ZERO;
    m_blockTypeCode = BPV7_BLOCK_TYPE_CODE::PAYLOAD;
    m_blockNumber = 1;
}
uint64_t Bpv7AdministrativeRecord::SerializeBpv7(uint8_t * serialization) {
    m_dataPtr = NULL;
    m_dataLength = GetCanonicalBlockTypeSpecificDataSerializationSize();
    const uint64_t serializationSizeCanonical = Bpv7CanonicalBlock::SerializeBpv7(serialization);
    uint64_t bufferSize = m_dataLength;
    uint8_t * blockSpecificDataSerialization = m_dataPtr;
    uint64_t thisSerializationSize;

    // admin-record-structure = [
    //  record-type-code: uint,
    //  record-content: any
    // ]
    //Administrative records are standard application data units that are
    //used in providing some of the features of the Bundle Protocol. One
    //type of administrative record has been defined to date: bundle
    //status reports.  Note that additional types of administrative
    //records may be defined by supplementary DTN protocol specification
    //documents.
    //
    //Every administrative record consists of:
    //
    //    - Record type code (an unsigned integer for which valid values
    //          are as defined below).
    //    - Record content in type-specific format.
    //
    //Each BP administrative record SHALL be represented as a CBOR array
    //comprising two items.
    if (bufferSize == 0) {
        return 0;
    }
    *blockSpecificDataSerialization++ = (4U << 5) | 2; //major type 4, additional information 2 (CBOR array size 2)
    --bufferSize;
    
    //The first item of the array SHALL be a record type code, which SHALL
    //be represented as a CBOR unsigned integer.
    thisSerializationSize = CborEncodeU64(blockSpecificDataSerialization, static_cast<uint64_t>(m_adminRecordTypeCode), bufferSize);
    blockSpecificDataSerialization += thisSerializationSize;
    bufferSize -= thisSerializationSize;

    //The second element of this array SHALL be the applicable CBOR
    //representation of the content of the record.  Details of the CBOR
    //representation of administrative record type 1 are provided below.
    //Details of the CBOR representation of other types of administrative
    //record type are included in the specifications defining those
    //records.
    if (m_adminRecordContentPtr) {
        thisSerializationSize = m_adminRecordContentPtr->SerializeBpv7(blockSpecificDataSerialization, bufferSize);
        blockSpecificDataSerialization += thisSerializationSize;
        bufferSize -= thisSerializationSize;
    }
    

    RecomputeCrcAfterDataModification(serialization, serializationSizeCanonical);
    return serializationSizeCanonical;
}

uint64_t Bpv7AdministrativeRecord::GetSerializationSize() const {
    return Bpv7CanonicalBlock::GetSerializationSize(
        GetCanonicalBlockTypeSpecificDataSerializationSize()
    ); 
}
uint64_t Bpv7AdministrativeRecord::GetCanonicalBlockTypeSpecificDataSerializationSize() const {
    return 1 + //1 => cbor byte (major type 4, additional information 2)
        CborGetEncodingSizeU64(static_cast<uint64_t>(m_adminRecordTypeCode)) +
        ((m_adminRecordContentPtr) ? m_adminRecordContentPtr->GetSerializationSize() : 0);
}


//serialization must be temporarily modifyable to zero crc and restore it
bool Bpv7AdministrativeRecord::Virtual_DeserializeExtensionBlockDataBpv7() {

    if (m_dataPtr == NULL) {
        return false;
    }
    
    uint8_t * serialization = m_dataPtr;
    uint64_t bufferSize = m_dataLength;
    uint8_t cborUintSizeDecoded;
    //uint64_t tmpNumBytesTakenToDecode64;

    if (bufferSize < 1) {
        return false;
    }
    //Each BP administrative record SHALL be represented as a CBOR array
    //comprising two items.
    const uint8_t initialCborByte = *serialization++;
    --bufferSize;
    if ((initialCborByte != ((4U << 5) | 2U)) && //major type 4, additional information 2 (array of length 2)
        (initialCborByte != ((4U << 5) | 31U))) { //major type 4, additional information 31 (Indefinite-Length Array)
        return false;
    }

    //The first item of the array SHALL be a record type code, which SHALL
    //be represented as a CBOR unsigned integer.
    m_adminRecordTypeCode = static_cast<BPV7_ADMINISTRATIVE_RECORD_TYPE_CODE>(CborDecodeU64(serialization, &cborUintSizeDecoded, bufferSize));
    if ((cborUintSizeDecoded == 0)) {
        return false; //failure
    }
    serialization += cborUintSizeDecoded;
    bufferSize -= cborUintSizeDecoded;
    switch (m_adminRecordTypeCode) {
        case BPV7_ADMINISTRATIVE_RECORD_TYPE_CODE::BUNDLE_STATUS_REPORT:
            m_adminRecordContentPtr = boost::make_unique<Bpv7AdministrativeRecordContentBundleStatusReport>();
            break;
        case BPV7_ADMINISTRATIVE_RECORD_TYPE_CODE::BIBE_PDU:
            m_adminRecordContentPtr = boost::make_unique<Bpv7AdministrativeRecordContentBibePduMessage>();
            break;
        //case BPV7_ADMINISTRATIVE_RECORD_TYPE_CODE::CUSTODY_SIGNAL:
        //     m_adminRecordContentPtr = boost::make_unique<Bpv7AdministrativeRecordContentBibeCustodySignal>();
        //     break;
        default:
            return false;
    }

    

    //The second element of this array SHALL be the applicable CBOR
    //representation of the content of the record.  Details of the CBOR
    //representation of administrative record type 1 are provided below.
    //Details of the CBOR representation of other types of administrative
    //record type are included in the specifications defining those
    //records.
    uint64_t tmpNumBytesTakenToDeserializeThisSpecificAdminRecordType;
    if (!m_adminRecordContentPtr->DeserializeBpv7(serialization, tmpNumBytesTakenToDeserializeThisSpecificAdminRecordType, bufferSize)) {
        return false;
    }
    serialization += tmpNumBytesTakenToDeserializeThisSpecificAdminRecordType;
    bufferSize -= tmpNumBytesTakenToDeserializeThisSpecificAdminRecordType; //from value above
    
    //An implementation of the Bundle Protocol MAY accept a sequence of
    //bytes that does not conform to the Bundle Protocol specification
    //(e.g., one that represents data elements in fixed-length arrays
    //rather than indefinite-length arrays) and transform it into
    //conformant BP structure before processing it.
    if (initialCborByte == ((4U << 5) | 31U)) { //major type 4, additional information 31 (Indefinite-Length Array)
        
        if (bufferSize == 0) {
            return false;
        }
        const uint8_t breakStopCode = *serialization++;
        --bufferSize;
        if (breakStopCode != 0xff) {
            return false;
        }
    }

    return (bufferSize == 0);
}


Bpv7AdministrativeRecordContentBase::~Bpv7AdministrativeRecordContentBase() {}


Bpv7AdministrativeRecordContentBundleStatusReport::~Bpv7AdministrativeRecordContentBundleStatusReport() {}
uint64_t Bpv7AdministrativeRecordContentBundleStatusReport::SerializeBpv7(uint8_t * serialization, uint64_t bufferSize) {
    uint8_t * const serializationBase = serialization;
    
    uint64_t thisSerializationSize;
    //6.1.1. Bundle Status Reports
    //The transmission of "bundle status reports" under specified
    //conditions is an option that can be invoked when transmission of a
    //bundle is requested. These reports are intended to provide
    //information about how bundles are progressing through the system,
    //including notices of receipt, forwarding, final delivery, and
    //deletion. They are transmitted to the Report-to endpoints of
    //bundles.
    //Each bundle status report SHALL be represented as a CBOR array.  The
    //number of elements in the array SHALL be either 6 (if the subject
    //bundle is a fragment) or 4 (otherwise).
    //
    // status-record-content = [
    //  bundle-status-information,
    //  status-report-reason-code: uint,
    //  source-node-eid: eid,
    //  subject-creation-timestamp: creation-timestamp,
    //  ? (
    //    subject-payload-offset: uint,
    //    subject-payload-length: uint
    //  )
    // ]
    if (bufferSize < 2) {
        return 0;
    }
    bufferSize -= 2;
    const uint8_t cborArraySizeBsr = 4 + ((static_cast<uint8_t>(m_subjectBundleIsFragment)) << 1);
    *serialization++ = (4U << 5) | cborArraySizeBsr; //major type 4, additional information [4, 6]
    
    //The first item of the bundle status report array SHALL be bundle
    //status information represented as a CBOR array of at least 4
    //elements.  The first four items of the bundle status information
    //array shall provide information on the following four status
    //assertions, in this order:
    //  - Reporting node received bundle.
    //  - Reporting node forwarded the bundle.
    //  - Reporting node delivered the bundle.
    //  - Reporting node deleted the bundle.
    //
    // bundle-status-information = [
    //  reporting-node-received-bundle: status-info-content,
    //  reporting-node-forwarded-bundle: status-info-content,
    //  reporting-node-delivered-bundle: status-info-content,
    //  reporting-node-deleted-bundle: status-info-content
    // ]
    *serialization++ = (4U << 5) | 4; //major type 4, additional information 4
    for (unsigned int bundleStatusInfoArrayIndex = 0; bundleStatusInfoArrayIndex < 4; ++bundleStatusInfoArrayIndex) {
        const status_info_content_t & statusInfoContent = m_bundleStatusInfo[bundleStatusInfoArrayIndex];
        //Each item of the bundle status information array SHALL be a bundle
        //status item represented as a CBOR array; the number of elements in
        //each such array SHALL be either 2 (if the value of the first item of
        //this bundle status item is 1 AND the "Report status time" flag was
        //set to 1 in the bundle processing flags of the bundle whose status
        //is being reported) or 1 (otherwise).   
        //
        // status-info-content = [
        //   status-indicator: bool,
        //   ? timestamp: dtn-time
        // ]
        if (bufferSize < 2) {
            return 0;
        }
        bufferSize -= 2;
        const bool doEncodeTimeStamp = (m_reportStatusTimeFlagWasSet && statusInfoContent.first);
        const uint8_t cborArraySizeStatusInfoContent = 1 + doEncodeTimeStamp;
        *serialization++ = (4U << 5) | cborArraySizeStatusInfoContent; //major type 4, additional information [1, 2]

        //The first item of the bundle
        //status item array SHALL be a status indicator, a Boolean value
        //indicating whether or not the corresponding bundle status is
        //asserted, represented as a CBOR Boolean value. 
        //False (major type 7, additional information 20) becomes a JSON false.
        //True (major type 7, additional information 21) becomes a JSON true.
        *serialization++ = ((7U << 5) | 20) + statusInfoContent.first;
        
        //The second item of
        //the bundle status item array, if present, SHALL indicate the time
        //(as reported by the local system clock, an implementation matter) at
        //which the indicated status was asserted for this bundle, represented
        //as a DTN time as described in Section 4.2.6. above.
        if (doEncodeTimeStamp) {
            thisSerializationSize = CborEncodeU64(serialization, statusInfoContent.second, bufferSize);
            serialization += thisSerializationSize;
            bufferSize -= thisSerializationSize;
        }
    }

    //The second item of the bundle status report array SHALL be the
    //bundle status report reason code explaining the value of the status
    //indicator, represented as a CBOR unsigned integer.
    thisSerializationSize = CborEncodeU64(serialization, static_cast<uint64_t>(m_statusReportReasonCode), bufferSize);
    serialization += thisSerializationSize;
    bufferSize -= thisSerializationSize;

    //The third item of the bundle status report array SHALL be the source
    //node ID identifying the source of the bundle whose status is being
    //reported, represented as described in Section 4.2.5.1.1. above.
    thisSerializationSize = m_sourceNodeEid.SerializeBpv7(serialization, bufferSize);
    serialization += thisSerializationSize;
    bufferSize -= thisSerializationSize;

    //The fourth item of the bundle status report array SHALL be the
    //creation timestamp of the bundle whose status is being reported,
    //represented as described in Section 4.2.7. above.
    thisSerializationSize = m_creationTimestamp.SerializeBpv7(serialization, bufferSize);
    serialization += thisSerializationSize;

    if (m_subjectBundleIsFragment) {
        bufferSize -= thisSerializationSize;
        //The fifth item of the bundle status report array SHALL be present if
        //and only if the bundle whose status is being reported contained a
        //fragment offset.  If present, it SHALL be the subject bundle's
        //fragment offset represented as a CBOR unsigned integer item.
        thisSerializationSize = CborEncodeU64(serialization, m_optionalSubjectPayloadFragmentOffset, bufferSize);
        serialization += thisSerializationSize;
        bufferSize -= thisSerializationSize;

        //The sixth item of the bundle status report array SHALL be present if
        //and only if the bundle whose status is being reported contained a
        //fragment offset.  If present, it SHALL be the length of the subject
        //bundle's payload represented as a CBOR unsigned integer item.
        thisSerializationSize = CborEncodeU64(serialization, m_optionalSubjectPayloadFragmentLength, bufferSize);
        serialization += thisSerializationSize;
    }

    return serialization - serializationBase;
}
uint64_t Bpv7AdministrativeRecordContentBundleStatusReport::GetSerializationSize() const {
    uint64_t size = 2 + 4 + 4; //2 cbor array start bytes + 4 cborStatusInfoContent start bytes + 4 booleans
    if (m_reportStatusTimeFlagWasSet) {
        for (unsigned int bundleStatusInfoArrayIndex = 0; bundleStatusInfoArrayIndex < 4; ++bundleStatusInfoArrayIndex) {
            const status_info_content_t & statusInfoContent = m_bundleStatusInfo[bundleStatusInfoArrayIndex];
            if (statusInfoContent.first) {
                size += CborGetEncodingSizeU64(statusInfoContent.second);
            }
        }
    }
    size += CborGetEncodingSizeU64(static_cast<uint64_t>(m_statusReportReasonCode));
    size += m_sourceNodeEid.GetSerializationSizeBpv7();
    size += m_creationTimestamp.GetSerializationSize();
    if (m_subjectBundleIsFragment) {
        size += CborGetEncodingSizeU64(m_optionalSubjectPayloadFragmentOffset);
        size += CborGetEncodingSizeU64(m_optionalSubjectPayloadFragmentLength);
    }
    return size;
}
bool Bpv7AdministrativeRecordContentBundleStatusReport::DeserializeBpv7(uint8_t * serialization, uint64_t & numBytesTakenToDecode, uint64_t bufferSize) {
    const uint8_t * const serializationBase = serialization;
    //6.1.1. Bundle Status Reports
    //The transmission of "bundle status reports" under specified
    //conditions is an option that can be invoked when transmission of a
    //bundle is requested. These reports are intended to provide
    //information about how bundles are progressing through the system,
    //including notices of receipt, forwarding, final delivery, and
    //deletion. They are transmitted to the Report-to endpoints of
    //bundles.
    //Each bundle status report SHALL be represented as a CBOR array.  The
    //number of elements in the array SHALL be either 6 (if the subject
    //bundle is a fragment) or 4 (otherwise).
    //
    // status-record-content = [
    //  bundle-status-information,
    //  status-report-reason-code: uint,
    //  source-node-eid: eid,
    //  subject-creation-timestamp: creation-timestamp,
    //  ? (
    //    subject-payload-offset: uint,
    //    subject-payload-length: uint
    //  )
    // ]

    if (bufferSize < 2) { //for 2 initial array headers
        return false;
    }
    bufferSize -= 2;
    uint8_t cborUintSizeDecoded;
    const uint8_t initialCborByteBsr = *serialization++;
    const uint8_t cborMajorTypeBsr = initialCborByteBsr >> 5;
    const uint8_t cborArraySizeBsr = initialCborByteBsr & 0x1f;
    if (cborMajorTypeBsr != 4U) {
        return false;
    }
    if ((cborArraySizeBsr != 4U) && (cborArraySizeBsr != 6U) && (cborArraySizeBsr != 31)) { //31 => indefinite length
        return false;
    }
    
    //The first item of the bundle status report array SHALL be bundle
    //status information represented as a CBOR array of at least 4
    //elements.  The first four items of the bundle status information
    //array shall provide information on the following four status
    //assertions, in this order:
    //  - Reporting node received bundle.
    //  - Reporting node forwarded the bundle.
    //  - Reporting node delivered the bundle.
    //  - Reporting node deleted the bundle.
    //
    // bundle-status-information = [
    //  reporting-node-received-bundle: status-info-content,
    //  reporting-node-forwarded-bundle: status-info-content,
    //  reporting-node-delivered-bundle: status-info-content,
    //  reporting-node-deleted-bundle: status-info-content
    // ]
    const uint8_t initialCborByteBsInfo = *serialization++; 
    if ((initialCborByteBsInfo != ((4U << 5) | 4)) && (initialCborByteBsInfo != ((4U << 5) | 31))) { //31 => indefinite length
        return false;
    }
    bool detectedIfReportStatusTimeFlagWasSet = false;
    for (unsigned int bundleStatusInfoArrayIndex = 0; bundleStatusInfoArrayIndex < 4; ++bundleStatusInfoArrayIndex) {
        status_info_content_t & statusInfoContent = m_bundleStatusInfo[bundleStatusInfoArrayIndex];
        //Each item of the bundle status information array SHALL be a bundle
        //status item represented as a CBOR array; the number of elements in
        //each such array SHALL be either 2 (if the value of the first item of
        //this bundle status item is 1 AND the "Report status time" flag was
        //set to 1 in the bundle processing flags of the bundle whose status
        //is being reported) or 1 (otherwise).   
        //
        // status-info-content = [
        //   status-indicator: bool,
        //   ? timestamp: dtn-time
        // ]
        if (bufferSize < 2) { //for 2 initial array header + boolean
            return false;
        }
        bufferSize -= 2;
        const uint8_t initialCborByteStatusInfoContent = *serialization++;
        const uint8_t cborMajorTypeStatusInfoContent = initialCborByteStatusInfoContent >> 5;
        const uint8_t cborArraySizeStatusInfoContent = initialCborByteStatusInfoContent & 0x1f;
        if (cborMajorTypeStatusInfoContent != 4U) {
            return false;
        }
        if ((cborArraySizeStatusInfoContent != 1U) && (cborArraySizeStatusInfoContent != 2U) && (cborArraySizeStatusInfoContent != 31)) { //31 => indefinite length
            return false;
        }
        //The first item of the bundle
        //status item array SHALL be a status indicator, a Boolean value
        //indicating whether or not the corresponding bundle status is
        //asserted, represented as a CBOR Boolean value. 
        //False (major type 7, additional information 20) becomes a JSON false.
        //True (major type 7, additional information 21) becomes a JSON true.
        const uint8_t cborBoolean = *serialization++;
        if (cborBoolean == ((7U << 5) | 20)) {
            statusInfoContent.first = false;
        }
        else if (cborBoolean == ((7U << 5) | 21)) {
            statusInfoContent.first = true;
        }
        else {
            return false;
        }
        
        if (statusInfoContent.first) { //this status was asserted
            if (!detectedIfReportStatusTimeFlagWasSet) {
                if (bufferSize < 2) {
                    return false;
                }
                //begin failed attempts
                //if the next element is a cbor boolean then m_reportStatusTimeFlagWasSet is false
                //const uint8_t peekAtNextPotentialCborBoolean = serialization[1]; //
                //m_reportStatusTimeFlagWasSet = ((peekAtNextPotentialCborBoolean != ((7U << 5) | 20)) && (peekAtNextPotentialCborBoolean != ((7U << 5) | 21)));
                //m_reportStatusTimeFlagWasSet = ((nextCborByte >> 5) == 0); //time is set if next byte is cbor uint (major type 0) (also not cbor break stop code)
                //end failed attempts
                if (cborArraySizeStatusInfoContent == 31) {
                    const uint8_t nextCborByte = serialization[0];
                    m_reportStatusTimeFlagWasSet = (nextCborByte != 0xff);
                }
                else {
                    m_reportStatusTimeFlagWasSet = (cborArraySizeStatusInfoContent == 2);
                }
                detectedIfReportStatusTimeFlagWasSet = true;
            }

            if ((!m_reportStatusTimeFlagWasSet) && (cborArraySizeStatusInfoContent == 2U)) {
                return false;
            }
            if ((m_reportStatusTimeFlagWasSet) && (cborArraySizeStatusInfoContent == 1U)) {
                return false;
            }
            //The second item of
            //the bundle status item array, if present, SHALL indicate the time
            //(as reported by the local system clock, an implementation matter) at
            //which the indicated status was asserted for this bundle, represented
            //as a DTN time as described in Section 4.2.6. above.
            if (m_reportStatusTimeFlagWasSet) {
                statusInfoContent.second = CborDecodeU64(serialization, &cborUintSizeDecoded, bufferSize);
                if ((cborUintSizeDecoded == 0)) {
                    return false; //failure
                }
                serialization += cborUintSizeDecoded;
                bufferSize -= cborUintSizeDecoded;
            }
        }

        //An implementation of the Bundle Protocol MAY accept a sequence of
        //bytes that does not conform to the Bundle Protocol specification
        //(e.g., one that represents data elements in fixed-length arrays
        //rather than indefinite-length arrays) and transform it into
        //conformant BP structure before processing it.
        if (cborArraySizeStatusInfoContent == 31U) { //additional information 31 (Indefinite-Length Array)
            if (bufferSize == 0) {
                return false;
            }
            const uint8_t breakStopCode = *serialization++;
            if (breakStopCode != 0xff) {
                return false;
            }
            --bufferSize;
        }
    }
    if (!detectedIfReportStatusTimeFlagWasSet) {
        return false; //none of the 4 items asserted
    }
    //The second item of the bundle status report array SHALL be the
    //bundle status report reason code explaining the value of the status
    //indicator, represented as a CBOR unsigned integer.
    m_statusReportReasonCode = static_cast<BPV7_STATUS_REPORT_REASON_CODE>(CborDecodeU64(serialization, &cborUintSizeDecoded, bufferSize));
    if ((cborUintSizeDecoded == 0)) {
        return false; //failure
    }
    serialization += cborUintSizeDecoded;
    bufferSize -= cborUintSizeDecoded;

    //The third item of the bundle status report array SHALL be the source
    //node ID identifying the source of the bundle whose status is being
    //reported, represented as described in Section 4.2.5.1.1. above.
    uint8_t cborSizeDecoded;
    if (!m_sourceNodeEid.DeserializeBpv7(serialization, &cborSizeDecoded, bufferSize)) { // cborSizeDecoded will never be 0 if function returns true
        return false; //failure
    }
    serialization += cborSizeDecoded;
    bufferSize -= cborSizeDecoded;

    //The fourth item of the bundle status report array SHALL be the
    //creation timestamp of the bundle whose status is being reported,
    //represented as described in Section 4.2.7. above.
    if (!m_creationTimestamp.DeserializeBpv7(serialization, &cborSizeDecoded, bufferSize)) { // cborSizeDecoded will never be 0 if function returns true
        return false; //failure
    }
    serialization += cborSizeDecoded;
    bufferSize -= cborSizeDecoded;

    if (cborArraySizeBsr == 4U) {
        m_subjectBundleIsFragment = false;
    }
    else if (cborArraySizeBsr == 6U) {
        m_subjectBundleIsFragment = true;
    }
    else { //(cborArraySizeBsr == 31))
        if (bufferSize == 0) {
            return false;
        }
        //if the next element is a cbor break character then the array size was 4
        const uint8_t peekAtNextPotentialCborBreakStopCode = *serialization;
        m_subjectBundleIsFragment = (peekAtNextPotentialCborBreakStopCode != 0xff);
    }

    if (m_subjectBundleIsFragment) {
        //The fifth item of the bundle status report array SHALL be present if
        //and only if the bundle whose status is being reported contained a
        //fragment offset.  If present, it SHALL be the subject bundle's
        //fragment offset represented as a CBOR unsigned integer item.
        m_optionalSubjectPayloadFragmentOffset = CborDecodeU64(serialization, &cborUintSizeDecoded, bufferSize);
        if ((cborUintSizeDecoded == 0)) {
            return false; //failure
        }
        serialization += cborUintSizeDecoded;
        bufferSize -= cborUintSizeDecoded;

        //The sixth item of the bundle status report array SHALL be present if
        //and only if the bundle whose status is being reported contained a
        //fragment offset.  If present, it SHALL be the length of the subject
        //bundle's payload represented as a CBOR unsigned integer item.
        m_optionalSubjectPayloadFragmentLength = CborDecodeU64(serialization, &cborUintSizeDecoded, bufferSize);
        if ((cborUintSizeDecoded == 0)) {
            return false; //failure
        }
        serialization += cborUintSizeDecoded;
        bufferSize -= cborUintSizeDecoded;
    }

    //An implementation of the Bundle Protocol MAY accept a sequence of
    //bytes that does not conform to the Bundle Protocol specification
    //(e.g., one that represents data elements in fixed-length arrays
    //rather than indefinite-length arrays) and transform it into
    //conformant BP structure before processing it.
    if (cborArraySizeBsr == 31) { //additional information 31 (Indefinite-Length Array)
        if (bufferSize == 0) {
            return false;
        }
        const uint8_t breakStopCode = *serialization++;
        if (breakStopCode != 0xff) {
            return false;
        }
    }
    numBytesTakenToDecode = (serialization - serializationBase);
    return true;
}
bool Bpv7AdministrativeRecordContentBundleStatusReport::IsEqual(const Bpv7AdministrativeRecordContentBase * otherPtr) const {
    if (const Bpv7AdministrativeRecordContentBundleStatusReport * asBsrPtr = dynamic_cast<const Bpv7AdministrativeRecordContentBundleStatusReport*>(otherPtr)) {
        const bool initialCompare = (asBsrPtr->m_statusReportReasonCode == m_statusReportReasonCode)
            && (asBsrPtr->m_sourceNodeEid == m_sourceNodeEid)
            && (asBsrPtr->m_creationTimestamp == m_creationTimestamp)
            && (asBsrPtr->m_subjectBundleIsFragment == m_subjectBundleIsFragment)
            && (asBsrPtr->m_reportStatusTimeFlagWasSet == m_reportStatusTimeFlagWasSet);
        if (!initialCompare) {
            return false;
        }
        if (m_subjectBundleIsFragment) {
            const bool fragmentCompare = (asBsrPtr->m_optionalSubjectPayloadFragmentOffset == m_optionalSubjectPayloadFragmentOffset)
                && (asBsrPtr->m_optionalSubjectPayloadFragmentLength == m_optionalSubjectPayloadFragmentLength);
            if (!fragmentCompare) {
                return false;
            }
        }

        for (unsigned int bundleStatusInfoArrayIndex = 0; bundleStatusInfoArrayIndex < 4; ++bundleStatusInfoArrayIndex) {
            const status_info_content_t & sic1 = m_bundleStatusInfo[bundleStatusInfoArrayIndex];
            const status_info_content_t & sic2 = asBsrPtr->m_bundleStatusInfo[bundleStatusInfoArrayIndex];
            
            if (sic1.first != sic2.first) {
                return false;
            }

            if (m_reportStatusTimeFlagWasSet) {
                if (sic1.first != sic2.first) {
                    return false;
                }
            }
        }
    }
    else {
        return false;
    }
    return true;
}




Bpv7AdministrativeRecordContentBibePduMessage::~Bpv7AdministrativeRecordContentBibePduMessage() {}
uint64_t Bpv7AdministrativeRecordContentBibePduMessage::SerializeBpv7(uint8_t * serialization, uint64_t bufferSize) {
    uint8_t * const serializationBase = serialization;

    uint64_t thisSerializationSize;
    //A BIBE protocol data unit is a Bundle Protocol administrative record
    //whose record type code is 3 (i.e., bit pattern 0011) and whose
    //representation conforms to the Bundle Protocol specification for
    //administrative record representation.  The content of the record
    //SHALL be a BPDU message represented as follows.
    //
    // admin-record-choice /= BIBE-PDU
    // BIBE-PDU = [3, [transmission-ID: uint,
    //                     retransmission-time: uint,
    //                     encapsulated-bundle: bytes,
    //                     admin-common]]
    //

    //Each BPDU message SHALL be represented as a CBOR array. The number
    //of elements in the array SHALL be 3.
    if (bufferSize == 0) {
        return 0;
    }
    --bufferSize;
    *serialization++ = (4U << 5) | 3; //major type 4, additional information 3


    //The first item of the BPDU array SHALL be the "transmission ID" for
    //the BPDU, represented as a CBOR unsigned integer.  The transmission
    //ID for a BPDU for which custody transfer is NOT requested SHALL be
    //zero.  The transmission ID for a BPDU for which custody transfer IS
    //requested SHALL be the current value of the local node's custodial
    //transmission count, plus 1.
    thisSerializationSize = CborEncodeU64(serialization, m_transmissionId, bufferSize);
    serialization += thisSerializationSize;
    bufferSize -= thisSerializationSize;

    //The second item of the BPDU array SHALL be the BPDU's retransmission
    //time (i.e., the time by which custody disposition for this BPDU is
    //expected), represented as a CBOR unsigned integer.  Retransmission
    //time for a BPDU for which custody transfer is NOT requested SHALL be
    //zero.  Retransmission time for a BPDU for which custody transfer IS
    //requested SHALL take the form of a "DTN Time" as defined in the
    //Bundle Protocol specification; determination of the value of
    //retransmission time is an implementation matter that is beyond the
    //scope of this specification and may be dynamically responsive to
    //changes in connectivity.
    thisSerializationSize = CborEncodeU64(serialization, m_custodyRetransmissionTime, bufferSize);
    serialization += thisSerializationSize;
    bufferSize -= thisSerializationSize;

    //The third item of the BPDU array SHALL be a single BP bundle, termed
    //the "encapsulated bundle", represented as a CBOR byte string of
    //definite length.
    uint8_t * const byteStringHeaderStartPtr = serialization;
    thisSerializationSize = CborEncodeU64(serialization, m_encapsulatedBundleLength, bufferSize);
    serialization += thisSerializationSize;
    bufferSize -= thisSerializationSize;
    *byteStringHeaderStartPtr |= (2U << 5); //change from major type 0 (unsigned integer) to major type 2 (byte string)
    uint8_t * const byteStringAllocatedDataStartPtr = serialization;
    serialization += m_encapsulatedBundleLength;

    //bool doCrcComputation = false;
    if (m_encapsulatedBundlePtr) { //if not NULL, copy data and compute crc
        memcpy(byteStringAllocatedDataStartPtr, m_encapsulatedBundlePtr, m_encapsulatedBundleLength);
        //doCrcComputation = true;
    }
    if (!m_temporaryEncapsulatedBundleStorage.empty()) { //delete this now that everything has been copied to the main bundle
        m_temporaryEncapsulatedBundleStorage.resize(0);
        m_temporaryEncapsulatedBundleStorage.shrink_to_fit();
    }

    return serialization - serializationBase;
}
uint64_t Bpv7AdministrativeRecordContentBibePduMessage::GetSerializationSize() const {
    uint64_t size = 1; //cbor array size 3 header byte
    size += CborGetEncodingSizeU64(m_transmissionId);
    size += CborGetEncodingSizeU64(m_custodyRetransmissionTime);
    size += CborGetEncodingSizeU64(m_encapsulatedBundleLength);
    size += m_encapsulatedBundleLength;
    return size;
}
bool Bpv7AdministrativeRecordContentBibePduMessage::DeserializeBpv7(uint8_t * serialization, uint64_t & numBytesTakenToDecode, uint64_t bufferSize) {
    const uint8_t * const serializationBase = serialization;
    //A BIBE protocol data unit is a Bundle Protocol administrative record
    //whose record type code is 3 (i.e., bit pattern 0011) and whose
    //representation conforms to the Bundle Protocol specification for
    //administrative record representation.  The content of the record
    //SHALL be a BPDU message represented as follows.
    //
    // admin-record-choice /= BIBE-PDU
    // BIBE-PDU = [3, [transmission-ID: uint,
    //                     retransmission-time: uint,
    //                     encapsulated-bundle: bytes,
    //                     admin-common]]
    //

    //Each BPDU message SHALL be represented as a CBOR array. The number
    //of elements in the array SHALL be 3.

    if (bufferSize < 1) { //for 1 initial array header
        return false;
    }
    --bufferSize;
    uint8_t cborUintSizeDecoded;
    const uint8_t initialCborByte = *serialization++;
    if ((initialCborByte != ((4U << 5) | 3U)) && //major type 4, additional information 3 (array of length 3)
        (initialCborByte != ((4U << 5) | 31U))) { //major type 4, additional information 31 (Indefinite-Length Array)
        return false;
    }

    //The first item of the BPDU array SHALL be the "transmission ID" for
    //the BPDU, represented as a CBOR unsigned integer.  The transmission
    //ID for a BPDU for which custody transfer is NOT requested SHALL be
    //zero.  The transmission ID for a BPDU for which custody transfer IS
    //requested SHALL be the current value of the local node's custodial
    //transmission count, plus 1.
    m_transmissionId = CborDecodeU64(serialization, &cborUintSizeDecoded, bufferSize);
    if ((cborUintSizeDecoded == 0)) {
        return false; //failure
    }
    serialization += cborUintSizeDecoded;
    bufferSize -= cborUintSizeDecoded;

    //The second item of the BPDU array SHALL be the BPDU's retransmission
    //time (i.e., the time by which custody disposition for this BPDU is
    //expected), represented as a CBOR unsigned integer.  Retransmission
    //time for a BPDU for which custody transfer is NOT requested SHALL be
    //zero.  Retransmission time for a BPDU for which custody transfer IS
    //requested SHALL take the form of a "DTN Time" as defined in the
    //Bundle Protocol specification; determination of the value of
    //retransmission time is an implementation matter that is beyond the
    //scope of this specification and may be dynamically responsive to
    //changes in connectivity.
    m_custodyRetransmissionTime = CborDecodeU64(serialization, &cborUintSizeDecoded, bufferSize);
    if ((cborUintSizeDecoded == 0)) {
        return false; //failure
    }
    serialization += cborUintSizeDecoded;
    bufferSize -= cborUintSizeDecoded;

    //The third item of the BPDU array SHALL be a single BP bundle, termed
    //the "encapsulated bundle", represented as a CBOR byte string of
    //definite length.
    if (bufferSize < 1) { //for 1 initial byte string header or tag24
        return false;
    }
    const uint8_t potentialTag24 = *serialization;
    if (potentialTag24 == ((6U << 5) | 24U)) { //major type 6, additional information 24 (Tag number 24 (CBOR data item))
        ++serialization;
        --bufferSize;
        if (bufferSize < 1) { //forbyteStringHeader
            return false;
        }
    }
    uint8_t * const byteStringHeaderStartPtr = serialization; //buffer size verified above
    const uint8_t cborMajorTypeByteString = (*byteStringHeaderStartPtr) >> 5;
    if (cborMajorTypeByteString != 2) {
        return false; //failure
    }
    *byteStringHeaderStartPtr &= 0x1f; //temporarily zero out major type to 0 to make it unsigned integer
    m_encapsulatedBundleLength = CborDecodeU64(byteStringHeaderStartPtr, &cborUintSizeDecoded, bufferSize);
    *byteStringHeaderStartPtr |= (2U << 5); // restore to major type to 2 (change from major type 0 (unsigned integer) to major type 2 (byte string))
    if (cborUintSizeDecoded == 0) {
        return false; //failure
    }
    serialization += cborUintSizeDecoded;
    bufferSize -= cborUintSizeDecoded;
    if (m_encapsulatedBundleLength > bufferSize) {
        return false;
    }
    m_encapsulatedBundlePtr = serialization;
    serialization += m_encapsulatedBundleLength;


    //An implementation of the Bundle Protocol MAY accept a sequence of
    //bytes that does not conform to the Bundle Protocol specification
    //(e.g., one that represents data elements in fixed-length arrays
    //rather than indefinite-length arrays) and transform it into
    //conformant BP structure before processing it.
    if (initialCborByte == ((4U << 5) | 31U)) { //additional information 31 (Indefinite-Length Array)
        bufferSize -= m_encapsulatedBundleLength;
        if (bufferSize == 0) {
            return false;
        }
        const uint8_t breakStopCode = *serialization++;
        if (breakStopCode != 0xff) {
            return false;
        }
    }
    numBytesTakenToDecode = (serialization - serializationBase);
    return true;
}
bool Bpv7AdministrativeRecordContentBibePduMessage::IsEqual(const Bpv7AdministrativeRecordContentBase * otherPtr) const {
    if (const Bpv7AdministrativeRecordContentBibePduMessage * asBibePduMessagePtr = dynamic_cast<const Bpv7AdministrativeRecordContentBibePduMessage*>(otherPtr)) {
        const bool initialCompare = (asBibePduMessagePtr->m_transmissionId == m_transmissionId)
            && (asBibePduMessagePtr->m_custodyRetransmissionTime == m_custodyRetransmissionTime)
            && (asBibePduMessagePtr->m_encapsulatedBundleLength == m_encapsulatedBundleLength);
        if (!initialCompare) {
            return false;
        }
        if ((m_encapsulatedBundlePtr == NULL) && (asBibePduMessagePtr->m_encapsulatedBundlePtr == NULL)) {
            return true;
        }
        else if ((m_encapsulatedBundlePtr != NULL) && (asBibePduMessagePtr->m_encapsulatedBundlePtr != NULL)) {
            return (memcmp(m_encapsulatedBundlePtr, asBibePduMessagePtr->m_encapsulatedBundlePtr, m_encapsulatedBundleLength) == 0);
        }
        else {
            return false;
        }
    }
    else {
        return false;
    }
}

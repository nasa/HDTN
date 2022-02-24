/***************************************************************************
 * NASA Glenn Research Center, Cleveland, OH
 * Released under the NASA Open Source Agreement (NOSA)
 * May  2021
 *
 ****************************************************************************
 */
#include "codec/bpv6.h"
#include "Sdnv.h"
#include <boost/make_unique.hpp>
#include "Uri.h"

Bpv6MetadataCanonicalBlock::Bpv6MetadataCanonicalBlock() : Bpv6CanonicalBlock() { //a default constructor: X() //don't initialize anything for efficiency, use SetZero if required
    m_blockTypeCode = BPV6_BLOCK_TYPE_CODE::METADATA_EXTENSION;
}
Bpv6MetadataCanonicalBlock::~Bpv6MetadataCanonicalBlock() { } //a destructor: ~X()
Bpv6MetadataCanonicalBlock::Bpv6MetadataCanonicalBlock(Bpv6MetadataCanonicalBlock&& o) :
    Bpv6CanonicalBlock(std::move(o)),
    m_metadataTypeCode(o.m_metadataTypeCode),
    m_metadataContentPtr(std::move(o.m_metadataContentPtr)) { } //a move constructor: X(X&&)
Bpv6MetadataCanonicalBlock& Bpv6MetadataCanonicalBlock::operator=(Bpv6MetadataCanonicalBlock && o) { //a move assignment: operator=(X&&)
    m_metadataTypeCode = o.m_metadataTypeCode;
    m_metadataContentPtr = std::move(o.m_metadataContentPtr);
    return static_cast<Bpv6MetadataCanonicalBlock&>(Bpv6CanonicalBlock::operator=(std::move(o)));
}
bool Bpv6MetadataCanonicalBlock::operator==(const Bpv6MetadataCanonicalBlock & o) const {
    const bool initialValue = (m_metadataTypeCode == o.m_metadataTypeCode)
        && Bpv6CanonicalBlock::operator==(o);
    if (!initialValue) {
        return false;
    }
    if ((m_metadataContentPtr) && (o.m_metadataContentPtr)) { //both not null
        return m_metadataContentPtr->IsEqual(o.m_metadataContentPtr.get());
    }
    else if ((!m_metadataContentPtr) && (!o.m_metadataContentPtr)) { //both null
        return true;
    }
    else {
        return false;
    }
}
bool Bpv6MetadataCanonicalBlock::operator!=(const Bpv6MetadataCanonicalBlock & o) const {
    return !(*this == o);
}
void Bpv6MetadataCanonicalBlock::SetZero() {
    Bpv6CanonicalBlock::SetZero();
    m_metadataTypeCode = BPV6_METADATA_TYPE_CODE::UNDEFINED_ZERO;
    m_blockTypeCode = BPV6_BLOCK_TYPE_CODE::PAYLOAD;
}
uint64_t Bpv6MetadataCanonicalBlock::SerializeBpv6(uint8_t * serialization) {
    m_blockTypeSpecificDataPtr = NULL;
    m_blockTypeSpecificDataLength = GetCanonicalBlockTypeSpecificDataSerializationSize();
    const uint64_t serializationSizeCanonical = Bpv6CanonicalBlock::SerializeBpv6(serialization);
    uint64_t bufferSize = m_blockTypeSpecificDataLength;
    uint8_t * blockSpecificDataSerialization = m_blockTypeSpecificDataPtr;
    uint64_t thisSerializationSize;

    //The structure of a metadata block is as follows:
    //
    //Metadata Block Format:
    // +-----+------+--------------------+------+----------+----------|
    // |Type |Flags |EID-Reference count |Len   | Metadata | Metadata |
    // |     |(SDNV)|  and list (opt)    |(SDNV)|   Type   |          |
    // +-----+------+--------------------+------+----------+----------+

    
    //Block-type-specific data fields as follows:

    //     - Metadata Type field (SDNV) - indicates which metadata type is
    //     to be used to interpret both the metadata in the metadata field
    //     and the EID-references in the optional Block EID-reference
    //     count and EID-references field (if present).  One metadata type
    //     is defined in this document.  Other metadata types may be
    //     defined in separate documents.
    thisSerializationSize = SdnvEncodeU64(blockSpecificDataSerialization, static_cast<uint64_t>(m_metadataTypeCode), bufferSize);
    blockSpecificDataSerialization += thisSerializationSize;
    bufferSize -= thisSerializationSize;
    
    //     - Metadata field - contains the metadata itself, formatted
    //     according to the metadata type that has been specified for this
    //     block.  One metadata type is defined in Section 4.1.  Other
    //     metadata types may be defined elsewhere, as discussed in
    //     Section 4.
    if (m_metadataContentPtr) {
        thisSerializationSize = m_metadataContentPtr->SerializeBpv6(blockSpecificDataSerialization, bufferSize);
        //blockSpecificDataSerialization += thisSerializationSize;
        //bufferSize -= thisSerializationSize; //not needed
    }

    return serializationSizeCanonical;
}

uint64_t Bpv6MetadataCanonicalBlock::GetCanonicalBlockTypeSpecificDataSerializationSize() const {
    return SdnvGetNumBytesRequiredToEncode(static_cast<uint64_t>(m_metadataTypeCode)) +
        ((m_metadataContentPtr) ? m_metadataContentPtr->GetSerializationSize() : 0);
}


bool Bpv6MetadataCanonicalBlock::Virtual_DeserializeExtensionBlockDataBpv6() {

    if (m_blockTypeSpecificDataPtr == NULL) {
        return false;
    }

    uint8_t * serialization = m_blockTypeSpecificDataPtr;
    uint64_t bufferSize = m_blockTypeSpecificDataLength;
    uint8_t sdnvSize;

    //Block-type-specific data fields as follows:

    //     - Metadata Type field (SDNV) - indicates which metadata type is
    //     to be used to interpret both the metadata in the metadata field
    //     and the EID-references in the optional Block EID-reference
    //     count and EID-references field (if present).  One metadata type
    //     is defined in this document.  Other metadata types may be
    //     defined in separate documents.
    m_metadataTypeCode = static_cast<BPV6_METADATA_TYPE_CODE>(SdnvDecodeU64(serialization, &sdnvSize, bufferSize));
    if (sdnvSize == 0) {
        return false; //failure
    }
    serialization += sdnvSize;
    bufferSize -= sdnvSize;

    if (m_metadataTypeCode == BPV6_METADATA_TYPE_CODE::URI) {
        std::unique_ptr<Bpv6MetadataContentUriList> uriList = boost::make_unique<Bpv6MetadataContentUriList>();
        m_metadataContentPtr = std::move(uriList);
    }
    else {
        std::unique_ptr<Bpv6MetadataContentGeneric> genericMeta = boost::make_unique<Bpv6MetadataContentGeneric>();
        m_metadataContentPtr = std::move(genericMeta);
    }


    uint64_t tmpNumBytesTakenToDeserializeThisSpecificMetadataType;
    if (!m_metadataContentPtr->DeserializeBpv6(serialization, tmpNumBytesTakenToDeserializeThisSpecificMetadataType, bufferSize)) {
        return false;
    }
    //serialization += tmpNumBytesTakenToDeserializeThisSpecificAdminRecordType;
    bufferSize -= tmpNumBytesTakenToDeserializeThisSpecificMetadataType; //from value above
    return (bufferSize == 0);
}






Bpv6MetadataContentBase::~Bpv6MetadataContentBase() {}


Bpv6MetadataContentUriList::Bpv6MetadataContentUriList() { } //a default constructor: X()
Bpv6MetadataContentUriList::~Bpv6MetadataContentUriList() { } //a destructor: ~X()
Bpv6MetadataContentUriList::Bpv6MetadataContentUriList(const Bpv6MetadataContentUriList& o) : m_uriArray(o.m_uriArray) { } //a copy constructor: X(const X&)
Bpv6MetadataContentUriList::Bpv6MetadataContentUriList(Bpv6MetadataContentUriList&& o) : m_uriArray(std::move(o.m_uriArray)) { } //a move constructor: X(X&&)
Bpv6MetadataContentUriList& Bpv6MetadataContentUriList::operator=(const Bpv6MetadataContentUriList& o) { //a copy assignment: operator=(const X&)
    m_uriArray = o.m_uriArray;
    return *this;
}
Bpv6MetadataContentUriList& Bpv6MetadataContentUriList::operator=(Bpv6MetadataContentUriList && o) { //a move assignment: operator=(X&&)
    m_uriArray = std::move(o.m_uriArray);
    return *this;
}
bool Bpv6MetadataContentUriList::operator==(const Bpv6MetadataContentUriList & o) const {
    return (m_uriArray == o.m_uriArray);
}
bool Bpv6MetadataContentUriList::operator!=(const Bpv6MetadataContentUriList & o) const {
    return !(*this == o);
}
bool Bpv6MetadataContentUriList::IsEqual(const Bpv6MetadataContentBase * otherPtr) const {
    if (const Bpv6MetadataContentUriList * asUriListPtr = dynamic_cast<const Bpv6MetadataContentUriList*>(otherPtr)) {
        return ((*asUriListPtr) == (*this));
    }
    else {
        return false;
    }
}
void Bpv6MetadataContentUriList::Reset() {
    m_uriArray.clear();
}

uint64_t Bpv6MetadataContentUriList::SerializeBpv6(uint8_t * serialization, uint64_t bufferSize) const {
    const uint8_t * const serializationBase = serialization;

    uint64_t thisSerializationSize;

    //4.1.  URI Metadata Type
    //
    //It is believed that use of URIs will, in many cases, be adequate for
    //encoding metadata, although it is recognized that use of URIs may not
    //be the most efficient method for such encoding.  Because of the
    //expected utility of using URI encoding for metadata, the metadata
    //type value of 0x01 is defined to indicate a metadata type of URI.
    //Metadata type values other than 0x01 will be used to indicate
    //alternative metadata types.
    //
    //The Metadata field for metadata of metadata type URI (0x01) consists
    //of an array of bytes formed by concatenating one or more null-
    //terminated URIs.  Unless determined by local policy, the specific
    //processing steps that must be performed on bundles with metadata
    //blocks containing metadata of type URI are expected to be indicated
    //as part of the URI encoding of the metadata.  It is envisioned that
    //users might define URI schemes for this purpose.  Metadata blocks
    //containing metadata of type URI MUST NOT include a Block EID-
    //reference count and EID-references field.  The absence of this field
    //MUST be indicated by a value of 0 for the "Block contains an EID-
    //reference field" flag in the block processing control flags.  Support
    //for the URI metadata type is OPTIONAL.
    for (std::size_t i = 0; i < m_uriArray.size(); ++i) {
        const cbhe_eid_t & eid = m_uriArray[i];
        thisSerializationSize = Uri::WriteIpnUriCstring(eid.nodeId, eid.serviceId, (char *)serialization, bufferSize);
        serialization += thisSerializationSize;
        bufferSize -= thisSerializationSize;
    }

    return serialization - serializationBase;
}
uint64_t Bpv6MetadataContentUriList::GetSerializationSize() const {
    uint64_t size = 0;
    for (std::size_t i = 0; i < m_uriArray.size(); ++i) {
        const cbhe_eid_t & eid = m_uriArray[i];
        size += Uri::GetIpnUriCstringLengthRequiredIncludingNullTerminator(eid.nodeId, eid.serviceId);
    }
    return size;
}
//m_isFragment must be set before calling
bool Bpv6MetadataContentUriList::DeserializeBpv6(const uint8_t * serialization, uint64_t & numBytesTakenToDecode, uint64_t bufferSize) {
    const uint8_t * const serializationBase = serialization;
    m_uriArray.resize(0);
    m_uriArray.reserve(10); //todo
    while (true) {
        m_uriArray.emplace_back();
        cbhe_eid_t & eid = m_uriArray.back();
        uint64_t bytesDecodedIncludingNullChar;
        if (!Uri::ParseIpnUriCstring((const char *)serialization, bufferSize, bytesDecodedIncludingNullChar, eid.nodeId, eid.serviceId)) {
            return false;
        }
        serialization += bytesDecodedIncludingNullChar;
        bufferSize -= bytesDecodedIncludingNullChar;
        if (bufferSize == 0) {
            numBytesTakenToDecode = (serialization - serializationBase);
            return true;
        }
    }
}




Bpv6MetadataContentGeneric::Bpv6MetadataContentGeneric() { } //a default constructor: X()
Bpv6MetadataContentGeneric::~Bpv6MetadataContentGeneric() { } //a destructor: ~X()
Bpv6MetadataContentGeneric::Bpv6MetadataContentGeneric(const Bpv6MetadataContentGeneric& o) : m_genericRawMetadata(o.m_genericRawMetadata) { } //a copy constructor: X(const X&)
Bpv6MetadataContentGeneric::Bpv6MetadataContentGeneric(Bpv6MetadataContentGeneric&& o) : m_genericRawMetadata(std::move(o.m_genericRawMetadata)) { } //a move constructor: X(X&&)
Bpv6MetadataContentGeneric& Bpv6MetadataContentGeneric::operator=(const Bpv6MetadataContentGeneric& o) { //a copy assignment: operator=(const X&)
    m_genericRawMetadata = o.m_genericRawMetadata;
    return *this;
}
Bpv6MetadataContentGeneric& Bpv6MetadataContentGeneric::operator=(Bpv6MetadataContentGeneric && o) { //a move assignment: operator=(X&&)
    m_genericRawMetadata = std::move(o.m_genericRawMetadata);
    return *this;
}
bool Bpv6MetadataContentGeneric::operator==(const Bpv6MetadataContentGeneric & o) const {
    return (m_genericRawMetadata == o.m_genericRawMetadata);
}
bool Bpv6MetadataContentGeneric::operator!=(const Bpv6MetadataContentGeneric & o) const {
    return !(*this == o);
}
bool Bpv6MetadataContentGeneric::IsEqual(const Bpv6MetadataContentBase * otherPtr) const {
    if (const Bpv6MetadataContentGeneric * asUriListPtr = dynamic_cast<const Bpv6MetadataContentGeneric*>(otherPtr)) {
        return ((*asUriListPtr) == (*this));
    }
    else {
        return false;
    }
}
void Bpv6MetadataContentGeneric::Reset() {
    m_genericRawMetadata.clear();
}

uint64_t Bpv6MetadataContentGeneric::SerializeBpv6(uint8_t * serialization, uint64_t bufferSize) const {
    if (bufferSize < m_genericRawMetadata.size()) {
        return 0;
    }
    memcpy(serialization, m_genericRawMetadata.data(), m_genericRawMetadata.size());
    return m_genericRawMetadata.size();
}
uint64_t Bpv6MetadataContentGeneric::GetSerializationSize() const {
    return m_genericRawMetadata.size();
}
bool Bpv6MetadataContentGeneric::DeserializeBpv6(const uint8_t * serialization, uint64_t & numBytesTakenToDecode, uint64_t bufferSize) {
    m_genericRawMetadata.assign(serialization, serialization + bufferSize);
    numBytesTakenToDecode = bufferSize;
    return true;
}

/***************************************************************************
 * NASA Glenn Research Center, Cleveland, OH
 * Released under the NASA Open Source Agreement (NOSA)
 * May  2021
 *
 ****************************************************************************
 */
#include "codec/AggregateCustodySignal.h"
#include "Sdnv.h"




AggregateCustodySignal::AggregateCustodySignal() :
    m_statusFlagsPlus7bitReasonCode(0){ } //a default constructor: X()
AggregateCustodySignal::~AggregateCustodySignal() { } //a destructor: ~X()
AggregateCustodySignal::AggregateCustodySignal(const AggregateCustodySignal& o) :
    m_statusFlagsPlus7bitReasonCode(o.m_statusFlagsPlus7bitReasonCode),
    m_custodyIdFills(o.m_custodyIdFills){ } //a copy constructor: X(const X&)
AggregateCustodySignal::AggregateCustodySignal(AggregateCustodySignal&& o) :
    m_statusFlagsPlus7bitReasonCode(o.m_statusFlagsPlus7bitReasonCode),
    m_custodyIdFills(std::move(o.m_custodyIdFills)) { } //a move constructor: X(X&&)
AggregateCustodySignal& AggregateCustodySignal::operator=(const AggregateCustodySignal& o) { //a copy assignment: operator=(const X&)
    m_statusFlagsPlus7bitReasonCode = o.m_statusFlagsPlus7bitReasonCode;
    m_custodyIdFills = o.m_custodyIdFills;
    return *this;
}
AggregateCustodySignal& AggregateCustodySignal::operator=(AggregateCustodySignal && o) { //a move assignment: operator=(X&&)
    m_statusFlagsPlus7bitReasonCode = o.m_statusFlagsPlus7bitReasonCode;
    m_custodyIdFills = std::move(o.m_custodyIdFills);
    return *this;
}
bool AggregateCustodySignal::operator==(const AggregateCustodySignal & o) const {
    return
        (m_statusFlagsPlus7bitReasonCode == o.m_statusFlagsPlus7bitReasonCode) &&
        (m_custodyIdFills == o.m_custodyIdFills);
}
bool AggregateCustodySignal::operator!=(const AggregateCustodySignal & o) const {
    return !(*this == o);
}
void AggregateCustodySignal::Reset() { //a copy assignment: operator=(const X&)
    //m_statusFlagsPlus7bitReasonCode = 0;
    m_custodyIdFills.clear();
}

uint64_t AggregateCustodySignal::Serialize(uint8_t * buffer) const { //use MAX_SERIALIZATION_SIZE sized buffer
    uint8_t * const serializationBase = buffer;

    //An Aggregate Custody Signal is an administrative record that shall have:
    // a) an administrative record type 4 for ‘Aggregate Custody Signal’;
    // b) Administrative Record Flag ‘record is for a fragment’ cleared.
    *buffer++ = (static_cast<uint8_t>(BPV6_ADMINISTRATIVE_RECORD_TYPES::AGGREGATE_CUSTODY_SIGNAL)) << 4; // | (static_cast<uint8_t>(m_isFragment)); fragment flag must be cleared
    
    //'Status' byte encoded in the same way as the status byte
    //for administrative records in RFC 5050, using the same reason codes.
    *buffer++ = m_statusFlagsPlus7bitReasonCode;

    buffer += SerializeFills(buffer);
        
    return buffer - serializationBase;
}

bool AggregateCustodySignal::Deserialize(const uint8_t * serialization, const uint64_t blockSizeStartingWithAdminRecordType) {

    if (blockSizeStartingWithAdminRecordType < 4) {
        return false; //failure.. minimum size is 4
    }

    const BPV6_ADMINISTRATIVE_RECORD_TYPES adminRecordType = static_cast<BPV6_ADMINISTRATIVE_RECORD_TYPES>(*serialization++ >> 4);
    if (adminRecordType != BPV6_ADMINISTRATIVE_RECORD_TYPES::AGGREGATE_CUSTODY_SIGNAL) {
        return false; //failure
    }

    m_statusFlagsPlus7bitReasonCode = *serialization++;

    return DeserializeFills(serialization, blockSizeStartingWithAdminRecordType - 2);
}

void AggregateCustodySignal::SetCustodyTransferStatusAndReason(bool custodyTransferSucceeded, BPV6_CUSTODY_SIGNAL_REASON_CODES_7BIT reasonCode7bit) {
    m_statusFlagsPlus7bitReasonCode = (static_cast<uint8_t>(reasonCode7bit)) | ((static_cast<uint8_t>(custodyTransferSucceeded)) << 7); //*cursor = csig->reasonCode | (csig->succeeded << 7);
}
bool AggregateCustodySignal::DidCustodyTransferSucceed() const {
    return ((m_statusFlagsPlus7bitReasonCode & 0x80) != 0);
}
BPV6_CUSTODY_SIGNAL_REASON_CODES_7BIT AggregateCustodySignal::GetReasonCode() const {
    return static_cast<BPV6_CUSTODY_SIGNAL_REASON_CODES_7BIT>(m_statusFlagsPlus7bitReasonCode & 0x7f);
}
//return number of fills
uint64_t AggregateCustodySignal::AddCustodyIdToFill(const uint64_t custodyId) {
    FragmentSet::InsertFragment(m_custodyIdFills, FragmentSet::data_fragment_t(custodyId, custodyId));
    return m_custodyIdFills.size();
}
//return number of fills
uint64_t AggregateCustodySignal::AddContiguousCustodyIdsToFill(const uint64_t firstCustodyId, const uint64_t lastCustodyId) {
    FragmentSet::InsertFragment(m_custodyIdFills, FragmentSet::data_fragment_t(firstCustodyId, lastCustodyId));
    return m_custodyIdFills.size();
}
uint64_t AggregateCustodySignal::SerializeFills(uint8_t * buffer) const {
    uint8_t * const serializationBase = buffer;
    uint64_t rightEdgePrevious = 0; // ion code before loop: lastFill = 0;
    for (std::set<FragmentSet::data_fragment_t>::const_iterator it = m_custodyIdFills.cbegin(); it != m_custodyIdFills.cend(); ++it) {
        buffer += SdnvEncodeU64(buffer, it->beginIndex - rightEdgePrevious); //ion code in loop: encode startDelta = fill.start - lastFill
        buffer += SdnvEncodeU64(buffer, (it->endIndex + 1) - it->beginIndex); //ion code in loop: encode fill.length
        rightEdgePrevious = it->endIndex; //ion code in loop: lastFill = fill.start + fill.length - 1;
    }
    return buffer - serializationBase; //returns length of sdnv encoded, if zero-length => (failure.. minimum 1 fill)
}
bool AggregateCustodySignal::DeserializeFills(const uint8_t * serialization, const uint64_t serializationSizeBytes) {
    uint8_t sdnvSize;
    uint64_t byteCounter = 0;
    m_custodyIdFills.clear();
    uint64_t rightEdgePrevious = 0;
    while (true) {
        const uint64_t startDelta = SdnvDecodeU64(serialization, &sdnvSize);
        if (sdnvSize == 0) {
            return false; //failure
        }
        serialization += sdnvSize;
        byteCounter += sdnvSize;
        if (byteCounter >= serializationSizeBytes) {
            return false; //failure
        }
        const uint64_t leftEdge = startDelta + rightEdgePrevious;

        const uint64_t fillLength = SdnvDecodeU64(serialization, &sdnvSize);
        if (sdnvSize == 0) {
            return false; //failure
        }
        rightEdgePrevious = (leftEdge + fillLength) - 1; //using rightEdgePrevious as current rightEdge
        AddContiguousCustodyIdsToFill(leftEdge, rightEdgePrevious);
        serialization += sdnvSize;
        byteCounter += sdnvSize;
        if (byteCounter >= serializationSizeBytes) {
            return (byteCounter == serializationSizeBytes); //done and success
        }
    }
}



#ifndef AGGREGATE_CUSTODY_SIGNAL_H
#define AGGREGATE_CUSTODY_SIGNAL_H

#include <stdlib.h>
#include <stdint.h>
#include <string>
#include "Bpv6AdministrativeRecords.h"
#include "FragmentSet.h"

class AggregateCustodySignal {
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

public:

private:
    //The second field shall be a ‘Status’ byte encoded in the same way as the status byte
    //for administrative records in RFC 5050, using the same reason codes
    uint8_t m_statusFlagsPlus7bitReasonCode;
public:
    std::set<FragmentSet::data_fragment_t> m_custodyIdFills;

public:
    AggregateCustodySignal(); //a default constructor: X()
    ~AggregateCustodySignal(); //a destructor: ~X()
    AggregateCustodySignal(const AggregateCustodySignal& o); //a copy constructor: X(const X&)
    AggregateCustodySignal(AggregateCustodySignal&& o); //a move constructor: X(X&&)
    AggregateCustodySignal& operator=(const AggregateCustodySignal& o); //a copy assignment: operator=(const X&)
    AggregateCustodySignal& operator=(AggregateCustodySignal&& o); //a move assignment: operator=(X&&)
    bool operator==(const AggregateCustodySignal & o) const;
    bool operator!=(const AggregateCustodySignal & o) const;
    uint64_t Serialize(uint8_t * buffer) const; //use MAX_SERIALIZATION_SIZE sized buffer
    bool Deserialize(const uint8_t * serialization, const uint64_t blockSizeStartingWithAdminRecordType);
    void Reset();

    void SetCustodyTransferStatusAndReason(bool custodyTransferSucceeded, BPV6_CUSTODY_SIGNAL_REASON_CODES_7BIT reasonCode7bit);
    bool DidCustodyTransferSucceed() const;
    BPV6_CUSTODY_SIGNAL_REASON_CODES_7BIT GetReasonCode() const;
    //return number of fills
    uint64_t AddCustodyIdToFill(const uint64_t custodyId);
    //return number of fills
    uint64_t AddContiguousCustodyIdsToFill(const uint64_t firstCustodyId, const uint64_t lastCustodyId);
public: //only public for unit testing
    uint64_t SerializeFills(uint8_t * buffer) const;
    bool DeserializeFills(const uint8_t * serialization, const uint64_t serializationSizeBytes);
};

#endif //AGGREGATE_CUSTODY_SIGNAL_H

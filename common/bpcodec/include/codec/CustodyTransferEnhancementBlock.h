

#ifndef CUSTODY_TRANSFER_ENHANCEMENT_BLOCK_H
#define CUSTODY_TRANSFER_ENHANCEMENT_BLOCK_H

#include <stdlib.h>
#include <stdint.h>
#include <string>
#include "bpv6.h"

class CustodyTransferEnhancementBlock {
    /* https://public.ccsds.org/Pubs/734x2b1.pdf
     
   +----------------+----------------+----------------+----------------+
   |     Canonical block type 0x0a     |  Block Flags* | Block Length* |
   +----------------+----------------+----------------+----------------+
   |   Custody ID* | CTEB creator custodian EID (variable len string)  |
   +----------------+----------------+----------------+----------------+
       * Field is an SDNV
  */


public:
    static constexpr unsigned int CBHE_MAX_SERIALIZATION_SIZE =
        1 + //block type
        10 + //block flags sdnv +
        1 + //block length (1-byte-min-sized-sdnv) +
        10 + //custody id sdnv +
        45; //length of "ipn:18446744073709551615.18446744073709551615" (note 45 > 32 so sdnv hardware acceleration overwrite is satisfied)

    BPV6_BLOCKFLAG m_blockProcessingControlFlags;
    uint64_t m_custodyId;
    std::string m_ctebCreatorCustodianEidString;

public:
    CustodyTransferEnhancementBlock(); //a default constructor: X()
    ~CustodyTransferEnhancementBlock(); //a destructor: ~X()
    CustodyTransferEnhancementBlock(const CustodyTransferEnhancementBlock& o); //a copy constructor: X(const X&)
    CustodyTransferEnhancementBlock(CustodyTransferEnhancementBlock&& o); //a move constructor: X(X&&)
    CustodyTransferEnhancementBlock& operator=(const CustodyTransferEnhancementBlock& o); //a copy assignment: operator=(const X&)
    CustodyTransferEnhancementBlock& operator=(CustodyTransferEnhancementBlock&& o); //a move assignment: operator=(X&&)
    bool operator==(const CustodyTransferEnhancementBlock & o) const;
    bool operator!=(const CustodyTransferEnhancementBlock & o) const;
    uint64_t SerializeCtebCanonicalBlock(uint8_t * buffer) const; //use MAX_SERIALIZATION_SIZE sized buffer
    static uint64_t StaticSerializeCtebCanonicalBlock(uint8_t * buffer, const BPV6_BLOCKFLAG blockProcessingControlFlags,
        const uint64_t custodyId, const std::string & ctebCreatorCustodianEidString, Bpv6CanonicalBlock & returnedCanonicalBlock);
    static uint64_t StaticSerializeCtebCanonicalBlockBody(uint8_t * buffer,
        const uint64_t custodyId, const std::string & ctebCreatorCustodianEidString, Bpv6CanonicalBlock & returnedCanonicalBlock);
    uint32_t DeserializeCtebCanonicalBlock(const uint8_t * serialization);
    void Reset();

    void AddCanonicalBlockProcessingControlFlag(BPV6_BLOCKFLAG flag);
    bool HasCanonicalBlockProcessingControlFlagSet(BPV6_BLOCKFLAG flag) const;
    
};

#endif //CUSTODY_TRANSFER_ENHANCEMENT_BLOCK_H

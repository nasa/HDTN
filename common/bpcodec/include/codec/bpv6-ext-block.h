/**
 * BPv6 Extension Blocks for GSFC's bplib and ION.
 *
 * @author Rachel Dudukovich (GRC-LSS0)
 * NOTICE: This code should be considered experimental, and should thus not be used in critical applications / contexts.
 */
#ifndef BPV6_EXT_BLOCK_H
#define BPV6_EXT_BLOCK_H

#include <stdlib.h>
#include <stdint.h>
#include "codec/bpv6.h"

/**
* Structure that contains information necessary for a Previous Hop extension block
*/
struct bpv6_prev_hop_ext_block : Bpv6CanonicalBlock {
    char scheme[4];//we would assume "ipn"or "dtn"
    char scheme_specific_eid[41];//using ipn scheme the max number of ascii characters in an eid would be 4+20+1+20 = "ipn:"+2^64+"." + 2^64

    /**
     * Decode a previous hop extension block. Tested with ION.
     * @param block structure into which values should be decoded
     * @param buffer target from which values should be read
     * @param offset offset into target from which values should be read
     * @param bufsz maximum size of the buffer
     * @return currently just returns zero
     */
    uint8_t bpv6_prev_hop_decode(const char* buffer, const size_t block_start, const size_t offset, const size_t bufsz);

    /**
     * Dumps an ION previous hop block stdout in a human-readable fashion
     *
     * @param block previous hop block  which should be displayed
     */
    void bpv6_prev_hop_print() const;
};

/**
 * Structure that contains information necessary for a Custody Transfer Extension Block (CTEB)
 */
struct bpv6_cust_transfer_ext_block : Bpv6CanonicalBlock {
    uint64_t cust_id;//cteb custody id
    uint64_t cteb_creator_node;//cteb creator EID node number (ipn scheme)
    uint64_t cteb_creator_service;//cteb creator EID service number (ipn scheme)

    /**
    * Decode an Aggregate Custody Signal (ACS) Custody Transfer Extension Block (CTEB). Tested with bplib.
    * There should be one CTEB per bundle using ACS. If a bundle agent does not support ACS,
    * the CTEB can be ignored.
    * @param block structure into which values should be decoded
    * @param buffer target from which values should be read
    * @param offset offset into target from which values should be read
    * @param bufsz maximum size of the buffer
    * @return currently just returns zero
    */
    uint8_t bpv6_cteb_decode(const char* buffer, const size_t block_start, const size_t offset, const size_t bufsz);

    /**
     * Dumps an ACS Custody Transfer Extension Block (CTEB) to stdout in a human-readable fashion
     *
     * @param block Custody Transfer Extension Block which should be displayed
     */
    void bpv6_cteb_print() const;
};

/**
 * Structure that contains information necessary for a Bundle Integrity Block (BIB)
 */
struct bpv6_bplib_bib_block : Bpv6CanonicalBlock {
    uint64_t num_targets;//number of security targets for this block, always 1 for bplib
    uint64_t target_type;//type of security target for this block, always 1 for bplib
    uint64_t target_sequence;//target sequence used by bplib
    uint64_t cipher_suite_id;//cipher suit id
    uint64_t cipher_suite_flags;//cipher suit flags
    uint64_t num_security_results;//number of security results, always 1 for bplib
    uint64_t security_result_type;//security result type
    uint64_t security_result_len;//security result length
    uint16_t security_result;//security result

    /**
    * Decode Streamlined Bundle Security Protocol (SBSP) Bundle Integrity Block (BIB). Tested with bplib.
    * Currently implemented for bplib BIB with CRC16 and only one security target.
    * The block type is 13 rather than 3 as specified in the CCSDS SBSP.
    * @param block structure into which values should be decoded
    * @param buffer target from which values should be read
    * @param offset offset into target from which values should be read
    * @param bufsz maximum size of the buffer
    * @return currently just returns zero
    */
    uint8_t bpv6_bib_decode(const char* buffer, const size_t offset, const size_t bufsz);

    /**
     * Dumps a bplib Bundle Integrity Block (BIB) to stdout in a human-readable fashion
     *
     * @param block Bundle Integrity Block (BIB) which should be displayed
     */
    void bpv6_bib_print() const;
};

/**
 * Structure that contains information necessary for a Bundle Age extension block
 */
struct bpv6_bundle_age_ext_block : Bpv6CanonicalBlock {
    uint64_t bundle_age;//bundle age from sdnv

    /**
     * Decode bundle age extension block. Tested with ION.
     * @param block structure into which values should be decoded
     * @param buffer target from which values should be read
     * @param offset offset into target from which values should be read
     * @param bufsz maximum size of the buffer
     * @return currently just returns zero
     */
    uint8_t bpv6_bundle_age_decode(const char* buffer, const size_t offset, const size_t bufsz);


    /**
     * Dumps a bundle age extension block to stdout in a human-readable fashion
     *
     * @param block Bundle age extension (BAE) block which should be displayed
     */
    void bpv6_bundle_age_print() const;
};


/**
 * Structure for bpv6 IPN EIDs,  2^64 is the maximum value for node and service numbers
*/
struct bpv6_eid {
    uint64_t node;
    uint64_t service;

    /** Convert from char* to bpv6_eid
     *  @param bpv6_eid structure to contain eid node and service number
     *  @param buffer char buffer containing the eid string
     *  @param offset into the buffer where the eid id located
     *  @param eid_len the expected length of the eid
     *  @param  bufsz maximum size of the buffer
     *  @return 0
     */
    uint8_t char_to_bpv6_eid(const char* buffer, const size_t offset, const size_t eid_len, const size_t bufsz);
};



#endif //BPV6_EXT_BLOCK_H

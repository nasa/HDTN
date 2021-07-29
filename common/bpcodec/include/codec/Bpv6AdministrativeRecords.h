/**
 * Simple encoder / decoder for RFC 5050 bundles.
 *
 * @author
 * NOTICE: This code should be considered experimental, and should thus not be used in critical applications / contexts.
 */

#ifndef BPV6_ADMINISTRATIVE_RECORDS_H
#define BPV6_ADMINISTRATIVE_RECORDS_H

#include <stdlib.h>
#include <stdint.h>
#include <string>
#include "TimestampUtil.h"



//Administrative record types
enum class BPV6_ADMINISTRATIVE_RECORD_TYPES : uint8_t {
    STATUS_REPORT = 1,
    CUSTODY_SIGNAL = 2,
    AGGREGATE_CUSTODY_SIGNAL = 4,
    ENCAPSULATED_BUNDLE = 7,
    SAGA_MESSAGE = 42
};

//Administrative record flags
enum class BPV6_ADMINISTRATIVE_RECORD_FLAGS : uint8_t {
    BUNDLE_IS_A_FRAGMENT = 1 //00000001
};

/*
+----------+--------------------------------------------+
|  Value   |                  Meaning                   |
+==========+============================================+
| 00000001 |  Reporting node received bundle.           |
+----------+--------------------------------------------+
| 00000010 |  Reporting node accepted custody of bundle.|
+----------+--------------------------------------------+
| 00000100 |  Reporting node forwarded the bundle.      |
+----------+--------------------------------------------+
| 00001000 |  Reporting node delivered the bundle.      |
+----------+--------------------------------------------+
| 00010000 |  Reporting node deleted the bundle.        |
+----------+--------------------------------------------+
| 00100000 |  Unused.                                   |
+----------+--------------------------------------------+
| 01000000 |  Unused.                                   |
+----------+--------------------------------------------+
| 10000000 |  Unused.                                   |
+----------+--------------------------------------------+
Status Flags for Bundle Status Reports
*/
enum class BPV6_BUNDLE_STATUS_REPORT_STATUS_FLAGS : uint8_t {
    NONE = 0,
    REPORTING_NODE_RECEIVED_BUNDLE = (1 << 0),
    REPORTING_NODE_ACCEPTED_CUSTODY_OF_BUNDLE = (1 << 1),
    REPORTING_NODE_FORWARDED_BUNDLE = (1 << 2),
    REPORTING_NODE_DELIVERED_BUNDLE = (1 << 3),
    REPORTING_NODE_DELETED_BUNDLE = (1 << 4),
};

/*
+---------+--------------------------------------------+
|  Value  |                  Meaning                   |
+=========+============================================+
|  0x00   |  No additional information.                |
+---------+--------------------------------------------+
|  0x01   |  Lifetime expired.                         |
+---------+--------------------------------------------+
|  0x02   |  Forwarded over unidirectional link.       |
+---------+--------------------------------------------+
|  0x03   |  Transmission canceled.                    |
+---------+--------------------------------------------+
|  0x04   |  Depleted storage.                         |
+---------+--------------------------------------------+
|  0x05   |  Destination endpoint ID unintelligible.   |
+---------+--------------------------------------------+
|  0x06   |  No known route to destination from here.  |
+---------+--------------------------------------------+
|  0x07   |  No timely contact with next node on route.|
+---------+--------------------------------------------+
|  0x08   |  Block unintelligible.                     |
+---------+--------------------------------------------+
| (other) |  Reserved for future use.                  |
+---------+--------------------------------------------+
Status Report Reason Codes
*/
enum class BPV6_BUNDLE_STATUS_REPORT_REASON_CODES : uint8_t {
    NO_ADDITIONAL_INFORMATION = 0,
    LIFETIME_EXPIRED = 1,
    FORWARDED_OVER_UNIDIRECTIONAL_LINK = 2,
    TRANSMISSION_CANCELLED = 3,
    DEPLETED_STORAGE = 4,
    DESTINATION_ENDPOINT_ID_UNINTELLIGIBLE = 5,
    NO_KNOWN_ROUTE_TO_DESTINATION_FROM_HERE = 6,
    NO_TIMELY_CONTACT_WITH_NEXT_NODE_ON_ROUTE = 7,
    BLOCK_UNINTELLIGIBLE = 8
};

class BundleStatusReport {
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

public:
    static constexpr unsigned int CBHE_MAX_SERIALIZATION_SIZE =
        3 + //admin flags + status flags + reason code
        10 + //fragmentOffsetSdnv.length +
        10 + //fragmentLengthSdnv.length +
        10 + //receiptTimeSecondsSdnv.length +
        5 + //receiptTimeNanosecSdnv.length +
        10 + //custodyTimeSecondsSdnv.length +
        5 + //custodyTimeNanosecSdnv.length +
        10 + //forwardTimeSecondsSdnv.length +
        5 + //forwardTimeNanosecSdnv.length +
        10 + //deliveryTimeSecondsSdnv.length +
        5 + //deliveryTimeNanosecSdnv.length +
        10 + //deletionTimeSecondsSdnv.length +
        5 + //deletionTimeNanosecSdnv.length +
        10 + //creationTimeSecondsSdnv.length +
        10 + //creationTimeCountSdnv.length +
        1 + //eidLengthSdnv.length +
        45; //length of "ipn:18446744073709551615.18446744073709551615" (note 45 > 32 so sdnv hardware acceleration overwrite is satisfied)
    BPV6_BUNDLE_STATUS_REPORT_STATUS_FLAGS m_statusFlags;
    BPV6_BUNDLE_STATUS_REPORT_REASON_CODES m_reasonCode;
    bool m_isFragment;

    uint64_t m_fragmentOffsetIfPresent;
    uint64_t m_fragmentLengthIfPresent;

    TimestampUtil::dtn_time_t m_timeOfReceiptOfBundle;
    TimestampUtil::dtn_time_t m_timeOfCustodyAcceptanceOfBundle;
    TimestampUtil::dtn_time_t m_timeOfForwardingOfBundle;
    TimestampUtil::dtn_time_t m_timeOfDeliveryOfBundle;
    TimestampUtil::dtn_time_t m_timeOfDeletionOfBundle;

    //from primary block of subject bundle
    uint64_t m_copyOfBundleCreationTimestampTimeSeconds;
    uint64_t m_copyOfBundleCreationTimestampSequenceNumber;

    std::string m_bundleSourceEid;

public:
    BundleStatusReport(); //a default constructor: X()
    ~BundleStatusReport(); //a destructor: ~X()
    BundleStatusReport(const BundleStatusReport& o); //a copy constructor: X(const X&)
    BundleStatusReport(BundleStatusReport&& o); //a move constructor: X(X&&)
    BundleStatusReport& operator=(const BundleStatusReport& o); //a copy assignment: operator=(const X&)
    BundleStatusReport& operator=(BundleStatusReport&& o); //a move assignment: operator=(X&&)
    bool operator==(const BundleStatusReport & o) const;
    bool operator!=(const BundleStatusReport & o) const;
    uint64_t Serialize(uint8_t * buffer) const; //use MAX_SERIALIZATION_SIZE sized buffer
    uint16_t Deserialize(const uint8_t * serialization);
    void Reset();

    void SetTimeOfReceiptOfBundleAndStatusFlag(const TimestampUtil::dtn_time_t & dtnTime);
    void SetTimeOfCustodyAcceptanceOfBundleAndStatusFlag(const TimestampUtil::dtn_time_t & dtnTime);
    void SetTimeOfForwardingOfBundleAndStatusFlag(const TimestampUtil::dtn_time_t & dtnTime);
    void SetTimeOfDeliveryOfBundleAndStatusFlag(const TimestampUtil::dtn_time_t & dtnTime);
    void SetTimeOfDeletionOfBundleAndStatusFlag(const TimestampUtil::dtn_time_t & dtnTime);
};

/*
 +---------+--------------------------------------------+
|  Value  |                  Meaning                   |
+=========+============================================+
|  0x00   |  No additional information.                |
+---------+--------------------------------------------+
|  0x01   |  Reserved for future use.                  |
+---------+--------------------------------------------+
|  0x02   |  Reserved for future use.                  |
+---------+--------------------------------------------+
|  0x03   |  Redundant reception (reception by a node  |
|         |  that is a custodial node for this bundle).|
+---------+--------------------------------------------+
|  0x04   |  Depleted storage.                         |
+---------+--------------------------------------------+
|  0x05   |  Destination endpoint ID unintelligible.   |
+---------+--------------------------------------------+
|  0x06   |  No known route to destination from here.  |
+---------+--------------------------------------------+
|  0x07   |  No timely contact with next node on route.|
+---------+--------------------------------------------+
|  0x08   |  Block unintelligible.                     |
+---------+--------------------------------------------+
| (other) |  Reserved for future use.                  |
+---------+--------------------------------------------+
Custody Signal Reason Codes
*/
enum class BPV6_CUSTODY_SIGNAL_REASON_CODES_7BIT : uint8_t {
    NO_ADDITIONAL_INFORMATION = 0,
    REDUNDANT_RECEPTION = 3,
    DEPLETED_STORAGE = 4,
    DESTINATION_ENDPOINT_ID_UNINTELLIGIBLE = 5,
    NO_KNOWN_ROUTE_TO_DESTINATION_FROM_HERE = 6,
    NO_TIMELY_CONTACT_WITH_NEXT_NODE_ON_ROUTE = 7,
    BLOCK_UNINTELLIGIBLE = 8
};

class CustodySignal {
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

public:
    static constexpr unsigned int CBHE_MAX_SERIALIZATION_SIZE =
        2 + //admin flags + (bit7 status flags |  bit 6..0 reason code)
        10 + //fragmentOffsetSdnv.length +
        10 + //fragmentLengthSdnv.length +
        10 + //signalTimeSecondsSdnv.length +
        5 + //signalTimeNanosecSdnv.length +
        10 + //creationTimeSecondsSdnv.length +
        10 + //creationTimeCountSdnv.length +
        1 + //eidLengthSdnv.length +
        45; //length of "ipn:18446744073709551615.18446744073709551615" (note 45 > 32 so sdnv hardware acceleration overwrite is satisfied)
private:
    uint8_t m_statusFlagsPlus7bitReasonCode;
public:
    bool m_isFragment;

    uint64_t m_fragmentOffsetIfPresent;
    uint64_t m_fragmentLengthIfPresent;

    TimestampUtil::dtn_time_t m_timeOfSignalGeneration;

    //from primary block of subject bundle
    uint64_t m_copyOfBundleCreationTimestampTimeSeconds;
    uint64_t m_copyOfBundleCreationTimestampSequenceNumber;

    std::string m_bundleSourceEid;

public:
    CustodySignal(); //a default constructor: X()
    ~CustodySignal(); //a destructor: ~X()
    CustodySignal(const CustodySignal& o); //a copy constructor: X(const X&)
    CustodySignal(CustodySignal&& o); //a move constructor: X(X&&)
    CustodySignal& operator=(const CustodySignal& o); //a copy assignment: operator=(const X&)
    CustodySignal& operator=(CustodySignal&& o); //a move assignment: operator=(X&&)
    bool operator==(const CustodySignal & o) const;
    bool operator!=(const CustodySignal & o) const;
    uint64_t Serialize(uint8_t * buffer) const; //use MAX_SERIALIZATION_SIZE sized buffer
    uint16_t Deserialize(const uint8_t * serialization);
    void Reset();

    void SetTimeOfSignalGeneration(const TimestampUtil::dtn_time_t & dtnTime);
    void SetCustodyTransferStatusAndReason(bool custodyTransferSucceeded, BPV6_CUSTODY_SIGNAL_REASON_CODES_7BIT reasonCode7bit);
    bool DidCustodyTransferSucceed() const;
    BPV6_CUSTODY_SIGNAL_REASON_CODES_7BIT GetReasonCode() const;
};

#endif //BPV6_ADMINISTRATIVE_RECORDS_H

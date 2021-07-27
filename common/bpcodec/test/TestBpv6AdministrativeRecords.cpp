#include <boost/test/unit_test.hpp>
#include <boost/thread.hpp>
#include "codec/Bpv6AdministrativeRecords.h"
#include <iostream>
#include <string>
#include <inttypes.h>
#include <vector>

BOOST_AUTO_TEST_CASE(Bpv6AdministrativeRecordsTestCase)
{
    
    //BundleStatusReport
    {
        TimestampUtil::dtn_time_t t1(1000, 65537); //2+3=5 serialization length
        const std::string eidStr = "ipn:2.3";
        std::vector<uint8_t> serialization(BundleStatusReport::CBHE_MAX_SERIALIZATION_SIZE);
        BundleStatusReport rpt;
        rpt.m_copyOfBundleCreationTimestampTimeSeconds = 150; //size 2 sdnv
        rpt.m_copyOfBundleCreationTimestampSequenceNumber = 65538; //size 3 sdnv
        rpt.m_bundleSourceEid = eidStr;
        rpt.SetTimeOfCustodyAcceptanceOfBundleAndStatusFlag(t1);//add custody
        rpt.m_reasonCode = BPV6_BUNDLE_STATUS_REPORT_REASON_CODES::DEPLETED_STORAGE;
        uint64_t sizeSerialized = rpt.Serialize(&serialization[0]);
        uint16_t expectedSerializationSize = 4 + 5 + 5 + static_cast<uint16_t>(eidStr.length()); // 4=>admin flags + status flags + reason code + eidLenSdnv
        BOOST_REQUIRE_EQUAL(sizeSerialized, expectedSerializationSize);
        BundleStatusReport rpt2;
        uint16_t numBytesTakenToDecode;
        BOOST_REQUIRE(rpt2.Deserialize(serialization.data(), &numBytesTakenToDecode));
        BOOST_REQUIRE_EQUAL(numBytesTakenToDecode, expectedSerializationSize);
        BOOST_REQUIRE(rpt == rpt2);

        //add fragment
        rpt.m_isFragment = true;
        rpt.m_fragmentOffsetIfPresent = 65539; //3 bytes
        rpt.m_fragmentLengthIfPresent = 65540; //3 bytes
        expectedSerializationSize += 6;
        //re-serialize/deserialize
        sizeSerialized = rpt.Serialize(&serialization[0]);
        BOOST_REQUIRE_EQUAL(sizeSerialized, expectedSerializationSize);
        BOOST_REQUIRE(rpt2.Deserialize(serialization.data(), &numBytesTakenToDecode));
        BOOST_REQUIRE_EQUAL(numBytesTakenToDecode, expectedSerializationSize);
        BOOST_REQUIRE(rpt == rpt2);

        //add receipt
        rpt.SetTimeOfReceiptOfBundleAndStatusFlag(t1);
        expectedSerializationSize += 5;
        //re-serialize/deserialize
        sizeSerialized = rpt.Serialize(&serialization[0]);
        BOOST_REQUIRE_EQUAL(sizeSerialized, expectedSerializationSize);
        BOOST_REQUIRE(rpt2.Deserialize(serialization.data(), &numBytesTakenToDecode));
        BOOST_REQUIRE_EQUAL(numBytesTakenToDecode, expectedSerializationSize);
        BOOST_REQUIRE(rpt == rpt2);

        //add deletion
        rpt.SetTimeOfDeletionOfBundleAndStatusFlag(t1);
        expectedSerializationSize += 5;
        //re-serialize/deserialize
        sizeSerialized = rpt.Serialize(&serialization[0]);
        BOOST_REQUIRE_EQUAL(sizeSerialized, expectedSerializationSize);
        BOOST_REQUIRE(rpt2.Deserialize(serialization.data(), &numBytesTakenToDecode));
        BOOST_REQUIRE_EQUAL(numBytesTakenToDecode, expectedSerializationSize);
        BOOST_REQUIRE(rpt == rpt2);

        //add delivery
        rpt.SetTimeOfDeliveryOfBundleAndStatusFlag(t1);
        expectedSerializationSize += 5;
        //re-serialize/deserialize
        sizeSerialized = rpt.Serialize(&serialization[0]);
        BOOST_REQUIRE_EQUAL(sizeSerialized, expectedSerializationSize);
        BOOST_REQUIRE(rpt2.Deserialize(serialization.data(), &numBytesTakenToDecode));
        BOOST_REQUIRE_EQUAL(numBytesTakenToDecode, expectedSerializationSize);
        BOOST_REQUIRE(rpt == rpt2);

        //add forwarding
        rpt.SetTimeOfForwardingOfBundleAndStatusFlag(t1);
        expectedSerializationSize += 5;
        //re-serialize/deserialize
        sizeSerialized = rpt.Serialize(&serialization[0]);
        BOOST_REQUIRE_EQUAL(sizeSerialized, expectedSerializationSize);
        BOOST_REQUIRE(rpt2.Deserialize(serialization.data(), &numBytesTakenToDecode));
        BOOST_REQUIRE_EQUAL(numBytesTakenToDecode, expectedSerializationSize);
        BOOST_REQUIRE(rpt == rpt2);

        //misc
        BOOST_REQUIRE(!(rpt != rpt2));
        BundleStatusReport rptCopy = rpt;
        BundleStatusReport rptCopy2(rpt);
        BundleStatusReport rpt2Moved = std::move(rpt2);
        BOOST_REQUIRE(rpt != rpt2); //rpt2 moved
        BOOST_REQUIRE(rpt == rpt2Moved);
        BOOST_REQUIRE(rpt == rptCopy);
        BOOST_REQUIRE(rpt == rptCopy2);
        BundleStatusReport rpt2Moved2(std::move(rpt2Moved));
        BOOST_REQUIRE(rpt != rpt2Moved); //rpt2 moved
        BOOST_REQUIRE(rpt == rpt2Moved2);
    }



    //Custody Signal
    {
        TimestampUtil::dtn_time_t t1(1000, 65537); //2+3=5 serialization length
        const std::string eidStr = "ipn:2.3";
        std::vector<uint8_t> serialization(CustodySignal::CBHE_MAX_SERIALIZATION_SIZE);
        CustodySignal sig;
        sig.m_copyOfBundleCreationTimestampTimeSeconds = 150; //size 2 sdnv
        sig.m_copyOfBundleCreationTimestampSequenceNumber = 65538; //size 3 sdnv
        sig.m_bundleSourceEid = eidStr;
        sig.SetTimeOfSignalGeneration(t1);//add custody
        BOOST_REQUIRE(!sig.DidCustodyTransferSucceed());
        BOOST_REQUIRE(sig.GetReasonCode() == BPV6_CUSTODY_SIGNAL_REASON_CODES_7BIT::NO_ADDITIONAL_INFORMATION);
        sig.SetCustodyTransferStatusAndReason(true, BPV6_CUSTODY_SIGNAL_REASON_CODES_7BIT::DESTINATION_ENDPOINT_ID_UNINTELLIGIBLE);
        BOOST_REQUIRE(sig.DidCustodyTransferSucceed());
        BOOST_REQUIRE(sig.GetReasonCode() == BPV6_CUSTODY_SIGNAL_REASON_CODES_7BIT::DESTINATION_ENDPOINT_ID_UNINTELLIGIBLE);
        uint64_t sizeSerialized = sig.Serialize(&serialization[0]);
        uint16_t expectedSerializationSize = 3 + 5 + 5 + static_cast<uint16_t>(eidStr.length()); // 3=>admin flags + (status | reason) + eidLenSdnv
        BOOST_REQUIRE_EQUAL(sizeSerialized, expectedSerializationSize);
        CustodySignal sig2;
        uint16_t numBytesTakenToDecode;
        BOOST_REQUIRE(sig2.Deserialize(serialization.data(), &numBytesTakenToDecode));
        BOOST_REQUIRE_EQUAL(numBytesTakenToDecode, expectedSerializationSize);
        BOOST_REQUIRE(sig == sig2);

        //misc
        BOOST_REQUIRE(!(sig != sig2));
        CustodySignal sigCopy = sig;
        CustodySignal sigCopy2(sig);
        CustodySignal sig2Moved = std::move(sig2);
        BOOST_REQUIRE(sig != sig2); //rpt2 moved
        BOOST_REQUIRE(sig == sig2Moved);
        BOOST_REQUIRE(sig == sigCopy);
        BOOST_REQUIRE(sig == sigCopy2);
        CustodySignal sig2Moved2(std::move(sig2Moved));
        BOOST_REQUIRE(sig != sig2Moved); //sig2 moved
        BOOST_REQUIRE(sig == sig2Moved2);
    }
}


#include <boost/test/unit_test.hpp>
#include <boost/thread.hpp>
#include "codec/CustodyTransferEnhancementBlock.h"
#include "codec/CustodyTransferManager.h"
#include <iostream>
#include <string>
#include <inttypes.h>
#include <vector>
#include "Uri.h"

static const uint64_t PRIMARY_SRC_NODE = 100;
static const uint64_t PRIMARY_SRC_SVC = 1;
static const std::string PRIMARY_SRC_URI = "ipn:100.1";
static const uint64_t PRIMARY_HDTN_NODE = 200;
static const uint64_t PRIMARY_HDTN_SVC = 2;
static const std::string PRIMARY_HDTN_URI = "ipn:200.2";
static const uint64_t PRIMARY_DEST_NODE = 300;
static const uint64_t PRIMARY_DEST_SVC = 3;
static const std::string PRIMARY_DEST_URI = "ipn:300.3";
static const uint64_t PRIMARY_TIME = 1000;
static const uint64_t PRIMARY_LIFETIME = 2000;
static const uint64_t PRIMARY_SEQ = 1;
static const uint64_t INVALID_CTEB_SRC_NODE = 400;
static const uint64_t INVALID_CTEB_SRC_SVC = 4;
static const std::string INVALID_CTEB_SRC_URI = "ipn:400.4";
#define BP_MSG_BUFSZ 2000 //don't care, not used

static uint64_t GenerateBundleWithCteb(uint64_t primaryCustodianNode, uint64_t primaryCustodianService,
    uint64_t ctebCustodianNode, uint64_t ctebCustodianService, uint64_t ctebCustodyId,
    const std::string & bundleDataStr, uint8_t * buffer)
{
    uint8_t * const serializationBase = buffer;

    Bpv6CbhePrimaryBlock primary;
    primary.SetZero();
    bpv6_canonical_block block;
    block.SetZero();

    primary.flags = bpv6_bundle_set_priority(BPV6_PRIORITY_EXPEDITED) |
        bpv6_bundle_set_gflags(BPV6_BUNDLEFLAG_SINGLETON | BPV6_BUNDLEFLAG_NOFRAGMENT | BPV6_BUNDLEFLAG_CUSTODY);
    primary.m_sourceNodeId.Set(PRIMARY_SRC_NODE, PRIMARY_SRC_SVC);
    primary.m_destinationEid.Set(PRIMARY_DEST_NODE, PRIMARY_DEST_SVC);
    primary.m_custodianEid.Set(primaryCustodianNode, primaryCustodianService);
    primary.creation = PRIMARY_TIME; //(uint64_t)bpv6_unix_to_5050(curr_time);
    primary.lifetime = PRIMARY_LIFETIME;
    primary.sequence = PRIMARY_SEQ;
    uint64_t retVal;
    retVal = primary.SerializeBpv6(buffer);
    if (retVal == 0) {
        return 0;
    }
    buffer += retVal;

    block.type = BPV6_BLOCKTYPE_PAYLOAD;
    block.flags = 0;// BPV6_BLOCKFLAG_LAST_BLOCK;
    block.length = bundleDataStr.length();

    retVal = block.bpv6_canonical_block_encode((char *)buffer, 0, BP_MSG_BUFSZ);
    if (retVal == 0) {
        return 0;
    }
    buffer += retVal;

    memcpy(buffer, bundleDataStr.data(), bundleDataStr.length());
    buffer += bundleDataStr.length();

    CustodyTransferEnhancementBlock cteb;
    cteb.m_custodyId = ctebCustodyId;
    cteb.m_ctebCreatorCustodianEidString = Uri::GetIpnUriString(ctebCustodianNode, ctebCustodianService);
    cteb.AddCanonicalBlockProcessingControlFlag(BLOCK_PROCESSING_CONTROL_FLAGS::LAST_BLOCK);

    retVal = cteb.SerializeCtebCanonicalBlock(buffer);
    if (retVal == 0) {
        return 0;
    }
    buffer += retVal;

    return buffer - serializationBase;
}

static uint64_t GenerateBundleWithoutCteb(uint64_t primaryCustodianNode, uint64_t primaryCustodianService,
    const std::string & bundleDataStr, uint8_t * buffer)
{
    uint8_t * const serializationBase = buffer;

    Bpv6CbhePrimaryBlock primary;
    primary.SetZero();
    bpv6_canonical_block block;
    block.SetZero();

    primary.flags = bpv6_bundle_set_priority(BPV6_PRIORITY_EXPEDITED) |
        bpv6_bundle_set_gflags(BPV6_BUNDLEFLAG_SINGLETON | BPV6_BUNDLEFLAG_NOFRAGMENT | BPV6_BUNDLEFLAG_CUSTODY);
    primary.m_sourceNodeId.Set(PRIMARY_SRC_NODE, PRIMARY_SRC_SVC);
    primary.m_destinationEid.Set(PRIMARY_DEST_NODE, PRIMARY_DEST_SVC);
    primary.m_custodianEid.Set(primaryCustodianNode, primaryCustodianService);
    primary.creation = PRIMARY_TIME; //(uint64_t)bpv6_unix_to_5050(curr_time);
    primary.lifetime = PRIMARY_LIFETIME;
    primary.sequence = PRIMARY_SEQ;
    uint64_t retVal;
    retVal = primary.SerializeBpv6(buffer);
    if (retVal == 0) {
        return 0;
    }
    buffer += retVal;

    block.type = BPV6_BLOCKTYPE_PAYLOAD;
    block.flags = BPV6_BLOCKFLAG_LAST_BLOCK;
    block.length = bundleDataStr.length();

    retVal = block.bpv6_canonical_block_encode((char *)buffer, 0, BP_MSG_BUFSZ);
    if (retVal == 0) {
        return 0;
    }
    buffer += retVal;

    memcpy(buffer, bundleDataStr.data(), bundleDataStr.length());
    buffer += bundleDataStr.length();

    return buffer - serializationBase;
}

BOOST_AUTO_TEST_CASE(CustodyTransferTestCase)
{
    //create bundle that accepts custody from bundle originator which will be sent to hdtn
    const uint64_t srcCtebCustodyId = 10;
    const uint64_t newHdtnCtebCustodyId = 11;
    const std::string bundleDataStr = "bundle data!!!";
    std::vector<uint8_t> validCtebBundle(2000);
    //make valid cteb (primary custodian matches cteb custodian) from bundle originator
    const cbhe_eid_t custodianOriginator(PRIMARY_SRC_NODE, PRIMARY_SRC_SVC);
    uint64_t len = GenerateBundleWithCteb(
        PRIMARY_SRC_NODE, PRIMARY_SRC_SVC, //primary custodian
        PRIMARY_SRC_NODE, PRIMARY_SRC_SVC, srcCtebCustodyId, //cteb custodian
        bundleDataStr, &validCtebBundle[0]);
    BOOST_REQUIRE_GT(len, 0);
    validCtebBundle.resize(len);

    //create invalid cteb bundle where cteb doesnt match
    std::vector<uint8_t> invalidCtebBundle(2000);
    //make valid cteb (primary custodian matches cteb custodian) from bundle originator
    len = GenerateBundleWithCteb(
        PRIMARY_SRC_NODE, PRIMARY_SRC_SVC, //primary custodian
        INVALID_CTEB_SRC_NODE, INVALID_CTEB_SRC_SVC, srcCtebCustodyId, //cteb custodian
        bundleDataStr, &invalidCtebBundle[0]);
    BOOST_REQUIRE_GT(len, 0);
    invalidCtebBundle.resize(len);

    //create no cteb bundle where cteb missing
    std::vector<uint8_t> missingCtebBundle(2000);
    //make valid cteb (primary custodian matches cteb custodian) from bundle originator
    len = GenerateBundleWithoutCteb(
        PRIMARY_SRC_NODE, PRIMARY_SRC_SVC, //primary custodian
        bundleDataStr, &missingCtebBundle[0]);
    BOOST_REQUIRE_GT(len, 0);
    missingCtebBundle.resize(len);

    

    //bundle with custody bit set, hdtn acs enabled and accepts custody, valid cteb
    {
        std::vector<uint8_t> bundleData(validCtebBundle);
        BundleViewV6 bv;
        //std::cout << "sz " << bundleSerializedCopy.size() << std::endl;
        Bpv6CbhePrimaryBlock originalPrimaryFromOriginator;
        BOOST_REQUIRE(bv.SwapInAndLoadBundle(bundleData));
        { //check primary
            Bpv6CbhePrimaryBlock & primary = bv.m_primaryBlockView.header;
            originalPrimaryFromOriginator = primary;
            const uint64_t requiredPrimaryFlags = BPV6_BUNDLEFLAG_SINGLETON | BPV6_BUNDLEFLAG_NOFRAGMENT | BPV6_BUNDLEFLAG_CUSTODY;
            BOOST_REQUIRE((primary.flags & requiredPrimaryFlags) == requiredPrimaryFlags);
            BOOST_REQUIRE_EQUAL(primary.m_custodianEid, cbhe_eid_t(PRIMARY_SRC_NODE, PRIMARY_SRC_SVC));
        }
        { //check cteb
            BOOST_REQUIRE_EQUAL(bv.GetNumCanonicalBlocks(), 2); //payload + cteb
            std::vector<BundleViewV6::Bpv6CanonicalBlockView*> blocks;
            bv.GetCanonicalBlocksByType(BPV6_BLOCKTYPE_CUST_TRANSFER_EXT, blocks);
            BOOST_REQUIRE_EQUAL(blocks.size(), 1);
            CustodyTransferEnhancementBlock cteb;
            BOOST_REQUIRE_EQUAL(cteb.DeserializeCtebCanonicalBlock((const uint8_t*)blocks[0]->actualSerializedHeaderAndBodyPtr.data()),
                blocks[0]->actualSerializedHeaderAndBodyPtr.size());
            BOOST_REQUIRE_EQUAL(cteb.m_ctebCreatorCustodianEidString, PRIMARY_SRC_URI); //cteb matches primary custodian
        }

        //hdtn node accept custody with acs
        const bool isAcsAware = true;
        CustodyTransferManager ctmHdtn(isAcsAware, PRIMARY_HDTN_NODE, PRIMARY_HDTN_SVC);
        BOOST_REQUIRE_EQUAL(ctmHdtn.GetAcsConstRef(custodianOriginator, BPV6_ACS_STATUS_REASON_INDICES::SUCCESS__NO_ADDITIONAL_INFORMATION).m_custodyIdFills.size(), 0);
        std::vector<uint8_t> custodySignalRfc5050SerializedBundle;
        Bpv6CbhePrimaryBlock custodySignalRfc5050Primary;
        BOOST_REQUIRE(ctmHdtn.ProcessCustodyOfBundle(bv, true, newHdtnCtebCustodyId, BPV6_ACS_STATUS_REASON_INDICES::SUCCESS__NO_ADDITIONAL_INFORMATION,
            custodySignalRfc5050SerializedBundle, custodySignalRfc5050Primary));
        BOOST_REQUIRE_EQUAL(ctmHdtn.GetAcsConstRef(custodianOriginator, BPV6_ACS_STATUS_REASON_INDICES::SUCCESS__NO_ADDITIONAL_INFORMATION).m_custodyIdFills.size(), 1);
        BOOST_REQUIRE_EQUAL(custodySignalRfc5050SerializedBundle.size(), 0);
        //pretend it was time to send acs
        std::pair<Bpv6CbhePrimaryBlock, std::vector<uint8_t> > primaryPlusSerializedBundle;
        std::vector<uint8_t> & serializedAcsBundleFromHdtn = primaryPlusSerializedBundle.second;
        //originalPrimaryFromOriginator.bpv6_primary_block_print();
        BOOST_REQUIRE(ctmHdtn.GenerateAcsBundle(primaryPlusSerializedBundle,
            originalPrimaryFromOriginator.m_custodianEid,
            BPV6_ACS_STATUS_REASON_INDICES::SUCCESS__NO_ADDITIONAL_INFORMATION));
        //test with generate all
        std::list<std::pair<Bpv6CbhePrimaryBlock, std::vector<uint8_t> > > serializedPrimariesAndBundlesList;
        BOOST_REQUIRE_EQUAL(ctmHdtn.GetLargestNumberOfFills(), 1);
        BOOST_REQUIRE(ctmHdtn.GenerateAllAcsBundlesAndClear(serializedPrimariesAndBundlesList));
        BOOST_REQUIRE_EQUAL(ctmHdtn.GetLargestNumberOfFills(), 0);
        BOOST_REQUIRE_EQUAL(serializedPrimariesAndBundlesList.size(), 1);
        //all cleared at this point
        BOOST_REQUIRE(ctmHdtn.GenerateAllAcsBundlesAndClear(serializedPrimariesAndBundlesList));
        BOOST_REQUIRE_EQUAL(serializedPrimariesAndBundlesList.size(), 0);
        //hdtn modifies bundle for next hop
        BOOST_REQUIRE(bv.Render(2000));
        bundleData.swap(bv.m_frontBuffer); //bundleData is now hdtn's modified bundle for next hop
        BOOST_REQUIRE(bv.SwapInAndLoadBundle(bundleData)); //bv is now hdtn's
        { //check new primary
            Bpv6CbhePrimaryBlock & primary = bv.m_primaryBlockView.header;
            const uint64_t requiredPrimaryFlags = BPV6_BUNDLEFLAG_SINGLETON | BPV6_BUNDLEFLAG_NOFRAGMENT | BPV6_BUNDLEFLAG_CUSTODY;
            BOOST_REQUIRE((primary.flags & requiredPrimaryFlags) == requiredPrimaryFlags);
            BOOST_REQUIRE_EQUAL(primary.m_custodianEid, cbhe_eid_t(PRIMARY_HDTN_NODE, PRIMARY_HDTN_SVC)); //hdtn is new custodian
        }
        { //check new cteb
            BOOST_REQUIRE_EQUAL(bv.GetNumCanonicalBlocks(), 2); //payload + cteb
            std::vector<BundleViewV6::Bpv6CanonicalBlockView*> blocks;
            bv.GetCanonicalBlocksByType(BPV6_BLOCKTYPE_CUST_TRANSFER_EXT, blocks);
            BOOST_REQUIRE_EQUAL(blocks.size(), 1);
            CustodyTransferEnhancementBlock cteb;
            BOOST_REQUIRE_EQUAL(cteb.DeserializeCtebCanonicalBlock((const uint8_t*)blocks[0]->actualSerializedHeaderAndBodyPtr.data()),
                blocks[0]->actualSerializedHeaderAndBodyPtr.size());
            BOOST_REQUIRE_EQUAL(cteb.m_ctebCreatorCustodianEidString, PRIMARY_HDTN_URI); //cteb matches new hdtn custodian
            BOOST_REQUIRE_EQUAL(cteb.m_custodyId, newHdtnCtebCustodyId);
        }

        //source node receives acs
        {
            BundleViewV6 bvSrc;
            //std::cout << "sz " << bundleSerializedCopy.size() << std::endl;
            BOOST_REQUIRE(bvSrc.SwapInAndLoadBundle(serializedAcsBundleFromHdtn));
            { //check primary
                Bpv6CbhePrimaryBlock & primary = bvSrc.m_primaryBlockView.header;
                const uint64_t requiredPrimaryFlags = BPV6_BUNDLEFLAG_SINGLETON | BPV6_BUNDLEFLAG_NOFRAGMENT | BPV6_BUNDLEFLAG_ADMIN_RECORD;
                BOOST_REQUIRE((primary.flags & requiredPrimaryFlags) == requiredPrimaryFlags);
                BOOST_REQUIRE_EQUAL(primary.m_custodianEid, cbhe_eid_t(0, 0));
                BOOST_REQUIRE_EQUAL(primary.m_sourceNodeId, cbhe_eid_t(PRIMARY_HDTN_NODE, PRIMARY_HDTN_SVC));
                BOOST_REQUIRE_EQUAL(primary.m_destinationEid, cbhe_eid_t(PRIMARY_SRC_NODE, PRIMARY_SRC_SVC));
            }
            { //check acs
                BOOST_REQUIRE_EQUAL(bvSrc.GetNumCanonicalBlocks(), 0); //admin record is not canonical
                BOOST_REQUIRE(bvSrc.m_applicationDataUnitStartPtr != NULL); //admin record is not canonical
                const uint8_t adminRecordType = (*bvSrc.m_applicationDataUnitStartPtr >> 4);
                BOOST_REQUIRE(adminRecordType == static_cast<uint8_t>(BPV6_ADMINISTRATIVE_RECORD_TYPES::AGGREGATE_CUSTODY_SIGNAL));
                AggregateCustodySignal acs;
                BOOST_REQUIRE(acs.Deserialize(bvSrc.m_applicationDataUnitStartPtr, bvSrc.m_frontBuffer.size() - bvSrc.m_primaryBlockView.actualSerializedPrimaryBlockPtr.size()));
                BOOST_REQUIRE(acs.DidCustodyTransferSucceed());
                BOOST_REQUIRE(acs.GetReasonCode() == BPV6_CUSTODY_SIGNAL_REASON_CODES_7BIT::NO_ADDITIONAL_INFORMATION);
                BOOST_REQUIRE_EQUAL(acs.m_custodyIdFills.size(), 1);
                BOOST_REQUIRE_EQUAL(acs.m_custodyIdFills.cbegin()->beginIndex, srcCtebCustodyId);
                BOOST_REQUIRE_EQUAL(acs.m_custodyIdFills.cbegin()->endIndex, srcCtebCustodyId);
            }
        }
    }


    //bundle with custody bit set, hdtn acs enabled and accepts custody, invalid cteb
    {
        std::vector<uint8_t> bundleData(invalidCtebBundle);
        BundleViewV6 bv;
        //std::cout << "sz " << bundleSerializedCopy.size() << std::endl;
        Bpv6CbhePrimaryBlock originalPrimaryFromOriginator;
        BOOST_REQUIRE(bv.SwapInAndLoadBundle(bundleData));
        { //check primary
            Bpv6CbhePrimaryBlock & primary = bv.m_primaryBlockView.header;
            originalPrimaryFromOriginator = primary;
            const uint64_t requiredPrimaryFlags = BPV6_BUNDLEFLAG_SINGLETON | BPV6_BUNDLEFLAG_NOFRAGMENT | BPV6_BUNDLEFLAG_CUSTODY;
            BOOST_REQUIRE((primary.flags & requiredPrimaryFlags) == requiredPrimaryFlags);
            BOOST_REQUIRE_EQUAL(primary.m_custodianEid, cbhe_eid_t(PRIMARY_SRC_NODE, PRIMARY_SRC_SVC));
        }
        { //check cteb
            BOOST_REQUIRE_EQUAL(bv.GetNumCanonicalBlocks(), 2); //payload + cteb
            std::vector<BundleViewV6::Bpv6CanonicalBlockView*> blocks;
            bv.GetCanonicalBlocksByType(BPV6_BLOCKTYPE_CUST_TRANSFER_EXT, blocks);
            BOOST_REQUIRE_EQUAL(blocks.size(), 1);
            CustodyTransferEnhancementBlock cteb;
            BOOST_REQUIRE_EQUAL(cteb.DeserializeCtebCanonicalBlock((const uint8_t*)blocks[0]->actualSerializedHeaderAndBodyPtr.data()),
                blocks[0]->actualSerializedHeaderAndBodyPtr.size());
            BOOST_REQUIRE_EQUAL(cteb.m_ctebCreatorCustodianEidString, INVALID_CTEB_SRC_URI); //cteb invalid
        }

        //hdtn node accept custody with acs
        const bool isAcsAware = true;
        CustodyTransferManager ctmHdtn(isAcsAware, PRIMARY_HDTN_NODE, PRIMARY_HDTN_SVC);
        BOOST_REQUIRE_EQUAL(ctmHdtn.GetAcsConstRef(custodianOriginator, BPV6_ACS_STATUS_REASON_INDICES::SUCCESS__NO_ADDITIONAL_INFORMATION).m_custodyIdFills.size(), 0);
        std::vector<uint8_t> custodySignalRfc5050SerializedBundle;
        Bpv6CbhePrimaryBlock custodySignalRfc5050Primary;
        BOOST_REQUIRE(ctmHdtn.ProcessCustodyOfBundle(bv, true, newHdtnCtebCustodyId, BPV6_ACS_STATUS_REASON_INDICES::SUCCESS__NO_ADDITIONAL_INFORMATION,
            custodySignalRfc5050SerializedBundle, custodySignalRfc5050Primary));
        BOOST_REQUIRE_EQUAL(ctmHdtn.GetAcsConstRef(custodianOriginator, BPV6_ACS_STATUS_REASON_INDICES::SUCCESS__NO_ADDITIONAL_INFORMATION).m_custodyIdFills.size(), 0); //acs not used due to invalid cteb
        BOOST_REQUIRE_GT(custodySignalRfc5050SerializedBundle.size(), 0); //using 5050 custody signal
        
        //hdtn modifies bundle for next hop
        BOOST_REQUIRE(bv.Render(2000));
        bundleData.swap(bv.m_frontBuffer); //bundleData is now hdtn's modified bundle for next hop
        BOOST_REQUIRE(bv.SwapInAndLoadBundle(bundleData)); //bv is now hdtn's
        { //check new primary
            Bpv6CbhePrimaryBlock & primary = bv.m_primaryBlockView.header;
            const uint64_t requiredPrimaryFlags = BPV6_BUNDLEFLAG_SINGLETON | BPV6_BUNDLEFLAG_NOFRAGMENT | BPV6_BUNDLEFLAG_CUSTODY;
            BOOST_REQUIRE((primary.flags & requiredPrimaryFlags) == requiredPrimaryFlags);
            BOOST_REQUIRE_EQUAL(primary.m_custodianEid, cbhe_eid_t(PRIMARY_HDTN_NODE, PRIMARY_HDTN_SVC)); //hdtn is new custodian
        }
        { //check new cteb
            BOOST_REQUIRE_EQUAL(bv.GetNumCanonicalBlocks(), 2); //payload + cteb
            std::vector<BundleViewV6::Bpv6CanonicalBlockView*> blocks;
            bv.GetCanonicalBlocksByType(BPV6_BLOCKTYPE_CUST_TRANSFER_EXT, blocks);
            BOOST_REQUIRE_EQUAL(blocks.size(), 1);
            CustodyTransferEnhancementBlock cteb;
            BOOST_REQUIRE_EQUAL(cteb.DeserializeCtebCanonicalBlock((const uint8_t*)blocks[0]->actualSerializedHeaderAndBodyPtr.data()),
                blocks[0]->actualSerializedHeaderAndBodyPtr.size());
            BOOST_REQUIRE_EQUAL(cteb.m_ctebCreatorCustodianEidString, PRIMARY_HDTN_URI); //cteb matches new hdtn custodian
            BOOST_REQUIRE_EQUAL(cteb.m_custodyId, newHdtnCtebCustodyId);
        }

        //source node receives 5050 custody signal success
        {
            BundleViewV6 bvSrc;
            //std::cout << "sz " << bundleSerializedCopy.size() << std::endl;
            BOOST_REQUIRE(bvSrc.SwapInAndLoadBundle(custodySignalRfc5050SerializedBundle));
            { //check primary
                Bpv6CbhePrimaryBlock & primary = bvSrc.m_primaryBlockView.header;
                const uint64_t requiredPrimaryFlags = BPV6_BUNDLEFLAG_SINGLETON | BPV6_BUNDLEFLAG_NOFRAGMENT | BPV6_BUNDLEFLAG_ADMIN_RECORD;
                BOOST_REQUIRE((primary.flags & requiredPrimaryFlags) == requiredPrimaryFlags);
                BOOST_REQUIRE_EQUAL(primary.m_custodianEid, cbhe_eid_t(0, 0));
                BOOST_REQUIRE_EQUAL(primary.m_sourceNodeId, cbhe_eid_t(PRIMARY_HDTN_NODE, PRIMARY_HDTN_SVC));
                BOOST_REQUIRE_EQUAL(primary.m_destinationEid, cbhe_eid_t(PRIMARY_SRC_NODE, PRIMARY_SRC_SVC));
            }
            { //check admin record
                BOOST_REQUIRE_EQUAL(bvSrc.GetNumCanonicalBlocks(), 0); //admin record is not canonical
                BOOST_REQUIRE(bvSrc.m_applicationDataUnitStartPtr != NULL); //admin record is not canonical
                const uint8_t adminRecordType = (*bvSrc.m_applicationDataUnitStartPtr >> 4);
                BOOST_REQUIRE(adminRecordType == static_cast<uint8_t>(BPV6_ADMINISTRATIVE_RECORD_TYPES::CUSTODY_SIGNAL));
                CustodySignal cs;
                BOOST_REQUIRE(cs.Deserialize(bvSrc.m_applicationDataUnitStartPtr));
                BOOST_REQUIRE(cs.DidCustodyTransferSucceed());
                BOOST_REQUIRE(cs.GetReasonCode() == BPV6_CUSTODY_SIGNAL_REASON_CODES_7BIT::NO_ADDITIONAL_INFORMATION);
                BOOST_REQUIRE_EQUAL(cs.m_bundleSourceEid, PRIMARY_SRC_URI);
            }
        }
    }


    //bundle with custody bit set, hdtn acs enabled and accepts custody, missing cteb
    {
        std::vector<uint8_t> bundleData(missingCtebBundle);
        BundleViewV6 bv;
        //std::cout << "sz " << bundleSerializedCopy.size() << std::endl;
        Bpv6CbhePrimaryBlock originalPrimaryFromOriginator;
        BOOST_REQUIRE(bv.SwapInAndLoadBundle(bundleData));
        { //check primary
            Bpv6CbhePrimaryBlock & primary = bv.m_primaryBlockView.header;
            originalPrimaryFromOriginator = primary;
            const uint64_t requiredPrimaryFlags = BPV6_BUNDLEFLAG_SINGLETON | BPV6_BUNDLEFLAG_NOFRAGMENT | BPV6_BUNDLEFLAG_CUSTODY;
            BOOST_REQUIRE((primary.flags & requiredPrimaryFlags) == requiredPrimaryFlags);
            BOOST_REQUIRE_EQUAL(primary.m_custodianEid, cbhe_eid_t(PRIMARY_SRC_NODE, PRIMARY_SRC_SVC));
        }
        { //check cteb missing
            BOOST_REQUIRE_EQUAL(bv.GetNumCanonicalBlocks(), 1); //payload only
        }

        //hdtn node accept custody with acs
        const bool isAcsAware = true;
        CustodyTransferManager ctmHdtn(isAcsAware, PRIMARY_HDTN_NODE, PRIMARY_HDTN_SVC);
        BOOST_REQUIRE_EQUAL(ctmHdtn.GetAcsConstRef(custodianOriginator, BPV6_ACS_STATUS_REASON_INDICES::SUCCESS__NO_ADDITIONAL_INFORMATION).m_custodyIdFills.size(), 0);
        std::vector<uint8_t> custodySignalRfc5050SerializedBundle;
        Bpv6CbhePrimaryBlock custodySignalRfc5050Primary;
        BOOST_REQUIRE(ctmHdtn.ProcessCustodyOfBundle(bv, true, newHdtnCtebCustodyId, BPV6_ACS_STATUS_REASON_INDICES::SUCCESS__NO_ADDITIONAL_INFORMATION,
            custodySignalRfc5050SerializedBundle, custodySignalRfc5050Primary));
        BOOST_REQUIRE_EQUAL(ctmHdtn.GetAcsConstRef(custodianOriginator, BPV6_ACS_STATUS_REASON_INDICES::SUCCESS__NO_ADDITIONAL_INFORMATION).m_custodyIdFills.size(), 0); //acs not used due to invalid cteb
        BOOST_REQUIRE_GT(custodySignalRfc5050SerializedBundle.size(), 0); //using 5050 custody signal

        //hdtn modifies bundle for next hop
        BOOST_REQUIRE(bv.Render(2000));
        bundleData.swap(bv.m_frontBuffer); //bundleData is now hdtn's modified bundle for next hop
        BOOST_REQUIRE(bv.SwapInAndLoadBundle(bundleData)); //bv is now hdtn's
        { //check new primary
            Bpv6CbhePrimaryBlock & primary = bv.m_primaryBlockView.header;
            const uint64_t requiredPrimaryFlags = BPV6_BUNDLEFLAG_SINGLETON | BPV6_BUNDLEFLAG_NOFRAGMENT | BPV6_BUNDLEFLAG_CUSTODY;
            BOOST_REQUIRE((primary.flags & requiredPrimaryFlags) == requiredPrimaryFlags);
            BOOST_REQUIRE_EQUAL(primary.m_custodianEid, cbhe_eid_t(PRIMARY_HDTN_NODE, PRIMARY_HDTN_SVC)); //hdtn is new custodian
        }
        { //check new cteb WAS CREATED/APPENDED
            BOOST_REQUIRE_EQUAL(bv.GetNumCanonicalBlocks(), 2); //payload + cteb
            std::vector<BundleViewV6::Bpv6CanonicalBlockView*> blocks;
            bv.GetCanonicalBlocksByType(BPV6_BLOCKTYPE_CUST_TRANSFER_EXT, blocks);
            BOOST_REQUIRE_EQUAL(blocks.size(), 1);
            CustodyTransferEnhancementBlock cteb;
            BOOST_REQUIRE_EQUAL(cteb.DeserializeCtebCanonicalBlock((const uint8_t*)blocks[0]->actualSerializedHeaderAndBodyPtr.data()),
                blocks[0]->actualSerializedHeaderAndBodyPtr.size());
            BOOST_REQUIRE_EQUAL(cteb.m_ctebCreatorCustodianEidString, PRIMARY_HDTN_URI); //cteb matches new hdtn custodian
            BOOST_REQUIRE_EQUAL(cteb.m_custodyId, newHdtnCtebCustodyId);
        }

        //source node receives 5050 custody signal success
        {
            BundleViewV6 bvSrc;
            //std::cout << "sz " << bundleSerializedCopy.size() << std::endl;
            BOOST_REQUIRE(bvSrc.SwapInAndLoadBundle(custodySignalRfc5050SerializedBundle));
            { //check primary
                Bpv6CbhePrimaryBlock & primary = bvSrc.m_primaryBlockView.header;
                const uint64_t requiredPrimaryFlags = BPV6_BUNDLEFLAG_SINGLETON | BPV6_BUNDLEFLAG_NOFRAGMENT | BPV6_BUNDLEFLAG_ADMIN_RECORD;
                BOOST_REQUIRE((primary.flags & requiredPrimaryFlags) == requiredPrimaryFlags);
                BOOST_REQUIRE_EQUAL(primary.m_custodianEid, cbhe_eid_t(0, 0));
                BOOST_REQUIRE_EQUAL(primary.m_sourceNodeId, cbhe_eid_t(PRIMARY_HDTN_NODE, PRIMARY_HDTN_SVC));
                BOOST_REQUIRE_EQUAL(primary.m_destinationEid, cbhe_eid_t(PRIMARY_SRC_NODE, PRIMARY_SRC_SVC));
            }
            { //check admin record
                BOOST_REQUIRE_EQUAL(bvSrc.GetNumCanonicalBlocks(), 0); //admin record is not canonical
                BOOST_REQUIRE(bvSrc.m_applicationDataUnitStartPtr != NULL); //admin record is not canonical
                const uint8_t adminRecordType = (*bvSrc.m_applicationDataUnitStartPtr >> 4);
                BOOST_REQUIRE(adminRecordType == static_cast<uint8_t>(BPV6_ADMINISTRATIVE_RECORD_TYPES::CUSTODY_SIGNAL));
                CustodySignal cs;
                BOOST_REQUIRE(cs.Deserialize(bvSrc.m_applicationDataUnitStartPtr));
                BOOST_REQUIRE(cs.DidCustodyTransferSucceed());
                BOOST_REQUIRE(cs.GetReasonCode() == BPV6_CUSTODY_SIGNAL_REASON_CODES_7BIT::NO_ADDITIONAL_INFORMATION);
                BOOST_REQUIRE_EQUAL(cs.m_bundleSourceEid, PRIMARY_SRC_URI);
            }
        }
    }


    //bundle with custody bit set, hdtn acs enabled and refuses custody, valid cteb
    {
        std::vector<uint8_t> bundleData(validCtebBundle);
        BundleViewV6 bv;
        //std::cout << "sz " << bundleSerializedCopy.size() << std::endl;
        BOOST_REQUIRE(bv.SwapInAndLoadBundle(bundleData));
        const Bpv6CbhePrimaryBlock originalPrimaryFromOriginator = bv.m_primaryBlockView.header;
        
        //hdtn node refuses custody with acs
        const bool isAcsAware = true;
        CustodyTransferManager ctmHdtn(isAcsAware, PRIMARY_HDTN_NODE, PRIMARY_HDTN_SVC);
        BOOST_REQUIRE_EQUAL(ctmHdtn.GetAcsConstRef(custodianOriginator, BPV6_ACS_STATUS_REASON_INDICES::FAIL__DEPLETED_STORAGE).m_custodyIdFills.size(), 0);
        std::vector<uint8_t> custodySignalRfc5050SerializedBundle;
        Bpv6CbhePrimaryBlock custodySignalRfc5050Primary;
        BOOST_REQUIRE(ctmHdtn.ProcessCustodyOfBundle(bv, false, newHdtnCtebCustodyId, BPV6_ACS_STATUS_REASON_INDICES::FAIL__DEPLETED_STORAGE,
            custodySignalRfc5050SerializedBundle, custodySignalRfc5050Primary));
        BOOST_REQUIRE_EQUAL(ctmHdtn.GetAcsConstRef(custodianOriginator, BPV6_ACS_STATUS_REASON_INDICES::FAIL__DEPLETED_STORAGE).m_custodyIdFills.size(), 1);
        BOOST_REQUIRE_EQUAL(custodySignalRfc5050SerializedBundle.size(), 0);
        //pretend it was time to send acs
        std::pair<Bpv6CbhePrimaryBlock, std::vector<uint8_t> > primaryPlusSerializedBundle;
        std::vector<uint8_t> & serializedAcsBundleFromHdtn = primaryPlusSerializedBundle.second;
        //originalPrimaryFromOriginator.bpv6_primary_block_print();
        BOOST_REQUIRE(ctmHdtn.GenerateAcsBundle(primaryPlusSerializedBundle,
            originalPrimaryFromOriginator.m_custodianEid,
            BPV6_ACS_STATUS_REASON_INDICES::FAIL__DEPLETED_STORAGE));
        //test with generate all
        std::list<std::pair<Bpv6CbhePrimaryBlock, std::vector<uint8_t> > > serializedPrimariesAndBundlesList;
        BOOST_REQUIRE_EQUAL(ctmHdtn.GetLargestNumberOfFills(), 1);
        BOOST_REQUIRE(ctmHdtn.GenerateAllAcsBundlesAndClear(serializedPrimariesAndBundlesList));
        BOOST_REQUIRE_EQUAL(ctmHdtn.GetLargestNumberOfFills(), 0);
        BOOST_REQUIRE_EQUAL(serializedPrimariesAndBundlesList.size(), 1);
        //all cleared at this point
        BOOST_REQUIRE(ctmHdtn.GenerateAllAcsBundlesAndClear(serializedPrimariesAndBundlesList));
        BOOST_REQUIRE_EQUAL(serializedPrimariesAndBundlesList.size(), 0);
        //hdtn does not modify bundle for next hop because custody was refused
        
        //source node receives acs
        {
            BundleViewV6 bvSrc;
            //std::cout << "sz " << bundleSerializedCopy.size() << std::endl;
            BOOST_REQUIRE(bvSrc.SwapInAndLoadBundle(serializedAcsBundleFromHdtn));
            { //check primary
                Bpv6CbhePrimaryBlock & primary = bvSrc.m_primaryBlockView.header;
                const uint64_t requiredPrimaryFlags = BPV6_BUNDLEFLAG_SINGLETON | BPV6_BUNDLEFLAG_NOFRAGMENT | BPV6_BUNDLEFLAG_ADMIN_RECORD;
                BOOST_REQUIRE((primary.flags & requiredPrimaryFlags) == requiredPrimaryFlags);
                BOOST_REQUIRE_EQUAL(primary.m_custodianEid, cbhe_eid_t(0,0));
                BOOST_REQUIRE_EQUAL(primary.m_sourceNodeId, cbhe_eid_t(PRIMARY_HDTN_NODE, PRIMARY_HDTN_SVC));
                BOOST_REQUIRE_EQUAL(primary.m_destinationEid, cbhe_eid_t(PRIMARY_SRC_NODE, PRIMARY_SRC_SVC));
            }
            { //check acs
                BOOST_REQUIRE_EQUAL(bvSrc.GetNumCanonicalBlocks(), 0); //admin record is not canonical
                BOOST_REQUIRE(bvSrc.m_applicationDataUnitStartPtr != NULL); //admin record is not canonical
                const uint8_t adminRecordType = (*bvSrc.m_applicationDataUnitStartPtr >> 4);
                BOOST_REQUIRE(adminRecordType == static_cast<uint8_t>(BPV6_ADMINISTRATIVE_RECORD_TYPES::AGGREGATE_CUSTODY_SIGNAL));
                AggregateCustodySignal acs;
                BOOST_REQUIRE(acs.Deserialize(bvSrc.m_applicationDataUnitStartPtr, bvSrc.m_frontBuffer.size() - bvSrc.m_primaryBlockView.actualSerializedPrimaryBlockPtr.size()));
                BOOST_REQUIRE(!acs.DidCustodyTransferSucceed());
                BOOST_REQUIRE(acs.GetReasonCode() == BPV6_CUSTODY_SIGNAL_REASON_CODES_7BIT::DEPLETED_STORAGE);
                BOOST_REQUIRE_EQUAL(acs.m_custodyIdFills.size(), 1);
                BOOST_REQUIRE_EQUAL(acs.m_custodyIdFills.cbegin()->beginIndex, srcCtebCustodyId);
                BOOST_REQUIRE_EQUAL(acs.m_custodyIdFills.cbegin()->endIndex, srcCtebCustodyId);
            }
        }
    }


    //bundle with custody bit set, hdtn acs enabled and refuses custody, invalid cteb
    {
        std::vector<uint8_t> bundleData(invalidCtebBundle);
        BundleViewV6 bv;
        //std::cout << "sz " << bundleSerializedCopy.size() << std::endl;
        
        BOOST_REQUIRE(bv.SwapInAndLoadBundle(bundleData));
        const Bpv6CbhePrimaryBlock originalPrimaryFromOriginator = bv.m_primaryBlockView.header;
        
        //hdtn node accept custody with acs
        const bool isAcsAware = true;
        CustodyTransferManager ctmHdtn(isAcsAware, PRIMARY_HDTN_NODE, PRIMARY_HDTN_SVC);
        BOOST_REQUIRE_EQUAL(ctmHdtn.GetAcsConstRef(custodianOriginator, BPV6_ACS_STATUS_REASON_INDICES::FAIL__DEPLETED_STORAGE).m_custodyIdFills.size(), 0);
        std::vector<uint8_t> custodySignalRfc5050SerializedBundle;
        Bpv6CbhePrimaryBlock custodySignalRfc5050Primary;
        BOOST_REQUIRE(ctmHdtn.ProcessCustodyOfBundle(bv, false, newHdtnCtebCustodyId, BPV6_ACS_STATUS_REASON_INDICES::FAIL__DEPLETED_STORAGE,
            custodySignalRfc5050SerializedBundle, custodySignalRfc5050Primary));
        BOOST_REQUIRE_EQUAL(ctmHdtn.GetAcsConstRef(custodianOriginator, BPV6_ACS_STATUS_REASON_INDICES::FAIL__DEPLETED_STORAGE).m_custodyIdFills.size(), 0); //acs not used due to invalid cteb
        BOOST_REQUIRE_GT(custodySignalRfc5050SerializedBundle.size(), 0); //using 5050 custody signal

        //hdtn does not modify bundle for next hop because custody was refused

        //source node receives 5050 custody signal success
        {
            BundleViewV6 bvSrc;
            //std::cout << "sz " << bundleSerializedCopy.size() << std::endl;
            BOOST_REQUIRE(bvSrc.SwapInAndLoadBundle(custodySignalRfc5050SerializedBundle));
            { //check primary
                Bpv6CbhePrimaryBlock & primary = bvSrc.m_primaryBlockView.header;
                const uint64_t requiredPrimaryFlags = BPV6_BUNDLEFLAG_SINGLETON | BPV6_BUNDLEFLAG_NOFRAGMENT | BPV6_BUNDLEFLAG_ADMIN_RECORD;
                BOOST_REQUIRE((primary.flags & requiredPrimaryFlags) == requiredPrimaryFlags);
                BOOST_REQUIRE_EQUAL(primary.m_custodianEid, cbhe_eid_t(0,0));
                BOOST_REQUIRE_EQUAL(primary.m_sourceNodeId, cbhe_eid_t(PRIMARY_HDTN_NODE, PRIMARY_HDTN_SVC));
                BOOST_REQUIRE_EQUAL(primary.m_destinationEid, cbhe_eid_t(PRIMARY_SRC_NODE, PRIMARY_SRC_SVC));
            }
            { //check admin record
                BOOST_REQUIRE_EQUAL(bvSrc.GetNumCanonicalBlocks(), 0); //admin record is not canonical
                BOOST_REQUIRE(bvSrc.m_applicationDataUnitStartPtr != NULL); //admin record is not canonical
                const uint8_t adminRecordType = (*bvSrc.m_applicationDataUnitStartPtr >> 4);
                BOOST_REQUIRE(adminRecordType == static_cast<uint8_t>(BPV6_ADMINISTRATIVE_RECORD_TYPES::CUSTODY_SIGNAL));
                CustodySignal cs;
                BOOST_REQUIRE(cs.Deserialize(bvSrc.m_applicationDataUnitStartPtr));
                BOOST_REQUIRE(!cs.DidCustodyTransferSucceed());
                BOOST_REQUIRE(cs.GetReasonCode() == BPV6_CUSTODY_SIGNAL_REASON_CODES_7BIT::DEPLETED_STORAGE);
                BOOST_REQUIRE_EQUAL(cs.m_bundleSourceEid, PRIMARY_SRC_URI);
            }
        }
    }


    //bundle with custody bit set, hdtn acs disabled and accepts custody (still valid cteb bundle)
    {
        std::vector<uint8_t> bundleData(validCtebBundle);
        BundleViewV6 bv;
        //std::cout << "sz " << bundleSerializedCopy.size() << std::endl;
        
        BOOST_REQUIRE(bv.SwapInAndLoadBundle(bundleData));
        const Bpv6CbhePrimaryBlock originalPrimaryFromOriginator = bv.m_primaryBlockView.header;
            

        //hdtn node accept custody without acs
        const bool isAcsAware = false;
        CustodyTransferManager ctmHdtn(isAcsAware, PRIMARY_HDTN_NODE, PRIMARY_HDTN_SVC);
        BOOST_REQUIRE_EQUAL(ctmHdtn.GetAcsConstRef(custodianOriginator, BPV6_ACS_STATUS_REASON_INDICES::SUCCESS__NO_ADDITIONAL_INFORMATION).m_custodyIdFills.size(), 0);
        std::vector<uint8_t> custodySignalRfc5050SerializedBundle;
        Bpv6CbhePrimaryBlock custodySignalRfc5050Primary;
        BOOST_REQUIRE(ctmHdtn.ProcessCustodyOfBundle(bv, true, newHdtnCtebCustodyId, BPV6_ACS_STATUS_REASON_INDICES::SUCCESS__NO_ADDITIONAL_INFORMATION,
            custodySignalRfc5050SerializedBundle, custodySignalRfc5050Primary));
        BOOST_REQUIRE_EQUAL(ctmHdtn.GetAcsConstRef(custodianOriginator, BPV6_ACS_STATUS_REASON_INDICES::SUCCESS__NO_ADDITIONAL_INFORMATION).m_custodyIdFills.size(), 0); //acs not used (disabled)
        BOOST_REQUIRE_GT(custodySignalRfc5050SerializedBundle.size(), 0); //using 5050 custody signal

        //hdtn modifies bundle for next hop
        BOOST_REQUIRE(bv.Render(2000));
        bundleData.swap(bv.m_frontBuffer); //bundleData is now hdtn's modified bundle for next hop
        BOOST_REQUIRE(bv.SwapInAndLoadBundle(bundleData)); //bv is now hdtn's
        { //check new primary
            Bpv6CbhePrimaryBlock & primary = bv.m_primaryBlockView.header;
            const uint64_t requiredPrimaryFlags = BPV6_BUNDLEFLAG_SINGLETON | BPV6_BUNDLEFLAG_NOFRAGMENT | BPV6_BUNDLEFLAG_CUSTODY;
            BOOST_REQUIRE((primary.flags & requiredPrimaryFlags) == requiredPrimaryFlags);
            BOOST_REQUIRE_EQUAL(primary.m_custodianEid, cbhe_eid_t(PRIMARY_HDTN_NODE, PRIMARY_HDTN_SVC)); //hdtn is new custodian
        }
        { //check cteb unchanged
            BOOST_REQUIRE_EQUAL(bv.GetNumCanonicalBlocks(), 2); //payload + cteb
            std::vector<BundleViewV6::Bpv6CanonicalBlockView*> blocks;
            bv.GetCanonicalBlocksByType(BPV6_BLOCKTYPE_CUST_TRANSFER_EXT, blocks);
            BOOST_REQUIRE_EQUAL(blocks.size(), 1);
            CustodyTransferEnhancementBlock cteb;
            BOOST_REQUIRE_EQUAL(cteb.DeserializeCtebCanonicalBlock((const uint8_t*)blocks[0]->actualSerializedHeaderAndBodyPtr.data()),
                blocks[0]->actualSerializedHeaderAndBodyPtr.size());
            BOOST_REQUIRE_EQUAL(cteb.m_ctebCreatorCustodianEidString, PRIMARY_SRC_URI); //cteb unmodified (still src)
        }

        //source node receives 5050 custody signal success
        {
            BundleViewV6 bvSrc;
            //std::cout << "sz " << bundleSerializedCopy.size() << std::endl;
            BOOST_REQUIRE(bvSrc.SwapInAndLoadBundle(custodySignalRfc5050SerializedBundle));
            { //check primary
                Bpv6CbhePrimaryBlock & primary = bvSrc.m_primaryBlockView.header;
                const uint64_t requiredPrimaryFlags = BPV6_BUNDLEFLAG_SINGLETON | BPV6_BUNDLEFLAG_NOFRAGMENT | BPV6_BUNDLEFLAG_ADMIN_RECORD;
                BOOST_REQUIRE((primary.flags & requiredPrimaryFlags) == requiredPrimaryFlags);
                BOOST_REQUIRE_EQUAL(primary.m_custodianEid, cbhe_eid_t(0, 0));
                BOOST_REQUIRE_EQUAL(primary.m_sourceNodeId, cbhe_eid_t(PRIMARY_HDTN_NODE, PRIMARY_HDTN_SVC));
                BOOST_REQUIRE_EQUAL(primary.m_destinationEid, cbhe_eid_t(PRIMARY_SRC_NODE, PRIMARY_SRC_SVC));
            }
            { //check admin record
                BOOST_REQUIRE_EQUAL(bvSrc.GetNumCanonicalBlocks(), 0); //admin record is not canonical
                BOOST_REQUIRE(bvSrc.m_applicationDataUnitStartPtr != NULL); //admin record is not canonical
                const uint8_t adminRecordType = (*bvSrc.m_applicationDataUnitStartPtr >> 4);
                BOOST_REQUIRE(adminRecordType == static_cast<uint8_t>(BPV6_ADMINISTRATIVE_RECORD_TYPES::CUSTODY_SIGNAL));
                CustodySignal cs;
                BOOST_REQUIRE(cs.Deserialize(bvSrc.m_applicationDataUnitStartPtr));
                BOOST_REQUIRE(cs.DidCustodyTransferSucceed());
                BOOST_REQUIRE(cs.GetReasonCode() == BPV6_CUSTODY_SIGNAL_REASON_CODES_7BIT::NO_ADDITIONAL_INFORMATION);
                BOOST_REQUIRE_EQUAL(cs.m_bundleSourceEid, PRIMARY_SRC_URI);
            }
        }
    }


    //bundle with custody bit set, hdtn acs disabled and refuses custody (still valid cteb bundle)
    {
        std::vector<uint8_t> bundleData(validCtebBundle);
        BundleViewV6 bv;
        //std::cout << "sz " << bundleSerializedCopy.size() << std::endl;

        BOOST_REQUIRE(bv.SwapInAndLoadBundle(bundleData));
        const Bpv6CbhePrimaryBlock originalPrimaryFromOriginator = bv.m_primaryBlockView.header;


        //hdtn node refuse custody without acs
        const bool isAcsAware = false;
        CustodyTransferManager ctmHdtn(isAcsAware, PRIMARY_HDTN_NODE, PRIMARY_HDTN_SVC);
        BOOST_REQUIRE_EQUAL(ctmHdtn.GetAcsConstRef(custodianOriginator, BPV6_ACS_STATUS_REASON_INDICES::FAIL__DEPLETED_STORAGE).m_custodyIdFills.size(), 0);
        std::vector<uint8_t> custodySignalRfc5050SerializedBundle;
        Bpv6CbhePrimaryBlock custodySignalRfc5050Primary;
        BOOST_REQUIRE(ctmHdtn.ProcessCustodyOfBundle(bv, false, newHdtnCtebCustodyId, BPV6_ACS_STATUS_REASON_INDICES::FAIL__DEPLETED_STORAGE,
            custodySignalRfc5050SerializedBundle, custodySignalRfc5050Primary));
        BOOST_REQUIRE_EQUAL(ctmHdtn.GetAcsConstRef(custodianOriginator, BPV6_ACS_STATUS_REASON_INDICES::FAIL__DEPLETED_STORAGE).m_custodyIdFills.size(), 0); //acs not used (disabled)
        BOOST_REQUIRE_GT(custodySignalRfc5050SerializedBundle.size(), 0); //using 5050 custody signal

        //hdtn does not modify bundle for next hop because custody was refused

        //source node receives 5050 custody signal fail
        {
            BundleViewV6 bvSrc;
            //std::cout << "sz " << bundleSerializedCopy.size() << std::endl;
            BOOST_REQUIRE(bvSrc.SwapInAndLoadBundle(custodySignalRfc5050SerializedBundle));
            { //check primary
                Bpv6CbhePrimaryBlock & primary = bvSrc.m_primaryBlockView.header;
                const uint64_t requiredPrimaryFlags = BPV6_BUNDLEFLAG_SINGLETON | BPV6_BUNDLEFLAG_NOFRAGMENT | BPV6_BUNDLEFLAG_ADMIN_RECORD;
                BOOST_REQUIRE((primary.flags & requiredPrimaryFlags) == requiredPrimaryFlags);
                BOOST_REQUIRE_EQUAL(primary.m_custodianEid, cbhe_eid_t(0, 0));
                BOOST_REQUIRE_EQUAL(primary.m_sourceNodeId, cbhe_eid_t(PRIMARY_HDTN_NODE, PRIMARY_HDTN_SVC));
                BOOST_REQUIRE_EQUAL(primary.m_destinationEid, cbhe_eid_t(PRIMARY_SRC_NODE, PRIMARY_SRC_SVC));

                BOOST_REQUIRE_EQUAL(custodySignalRfc5050Primary.m_sourceNodeId, cbhe_eid_t(PRIMARY_HDTN_NODE, PRIMARY_HDTN_SVC));
                BOOST_REQUIRE_EQUAL(custodySignalRfc5050Primary.m_destinationEid, cbhe_eid_t(PRIMARY_SRC_NODE, PRIMARY_SRC_SVC));
            }
            { //check admin record
                BOOST_REQUIRE_EQUAL(bvSrc.GetNumCanonicalBlocks(), 0); //admin record is not canonical
                BOOST_REQUIRE(bvSrc.m_applicationDataUnitStartPtr != NULL); //admin record is not canonical
                const uint8_t adminRecordType = (*bvSrc.m_applicationDataUnitStartPtr >> 4);
                BOOST_REQUIRE(adminRecordType == static_cast<uint8_t>(BPV6_ADMINISTRATIVE_RECORD_TYPES::CUSTODY_SIGNAL));
                CustodySignal cs;
                BOOST_REQUIRE(cs.Deserialize(bvSrc.m_applicationDataUnitStartPtr));
                BOOST_REQUIRE(!cs.DidCustodyTransferSucceed());
                BOOST_REQUIRE(cs.GetReasonCode() == BPV6_CUSTODY_SIGNAL_REASON_CODES_7BIT::DEPLETED_STORAGE);
                BOOST_REQUIRE_EQUAL(cs.m_bundleSourceEid, PRIMARY_SRC_URI);
            }
        }
    }

}


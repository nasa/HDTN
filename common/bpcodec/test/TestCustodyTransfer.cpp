/**
 * @file TestCustodyTransfer.cpp
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

#include <boost/test/unit_test.hpp>
#include <boost/thread.hpp>
#include "codec/CustodyTransferManager.h"
#include <iostream>
#include <string>
#include <inttypes.h>
#include <vector>
#include "Uri.h"
#include <boost/make_unique.hpp>

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
static const uint64_t REPORT_TO_NODE = 12;
static const uint64_t REPORT_TO_SVC = 13;
#define BP_MSG_BUFSZ 2000 //don't care, not used

static void GenerateBundleWithCteb(uint64_t primaryCustodianNode, uint64_t primaryCustodianService,
    uint64_t ctebCustodianNode, uint64_t ctebCustodianService, uint64_t ctebCustodyId,
    const std::string & bundleDataStr, BundleViewV6 & bundleViewWithCteb)
{
    BundleViewV6 & bv = bundleViewWithCteb;
    Bpv6CbhePrimaryBlock & primary = bv.m_primaryBlockView.header;
    primary.SetZero();


    primary.m_bundleProcessingControlFlags = BPV6_BUNDLEFLAG::PRIORITY_EXPEDITED | BPV6_BUNDLEFLAG::SINGLETON | BPV6_BUNDLEFLAG::NOFRAGMENT | BPV6_BUNDLEFLAG::CUSTODY_REQUESTED;
    primary.m_sourceNodeId.Set(PRIMARY_SRC_NODE, PRIMARY_SRC_SVC);
    primary.m_destinationEid.Set(PRIMARY_DEST_NODE, PRIMARY_DEST_SVC);
    primary.m_custodianEid.Set(primaryCustodianNode, primaryCustodianService);
    primary.m_creationTimestamp.secondsSinceStartOfYear2000 = PRIMARY_TIME; //(uint64_t)bpv6_unix_to_5050(curr_time);
    primary.m_lifetimeSeconds = PRIMARY_LIFETIME;
    primary.m_creationTimestamp.sequenceNumber = PRIMARY_SEQ;
    bv.m_primaryBlockView.SetManuallyModified();

    //add cteb
    {
        std::unique_ptr<Bpv6CanonicalBlock> blockPtr = boost::make_unique<Bpv6CustodyTransferEnhancementBlock>();
        Bpv6CustodyTransferEnhancementBlock & block = *(reinterpret_cast<Bpv6CustodyTransferEnhancementBlock*>(blockPtr.get()));
        //block.SetZero();

        block.m_blockProcessingControlFlags = BPV6_BLOCKFLAG::NO_FLAGS_SET; //something for checking against
        block.m_custodyId = ctebCustodyId;
        block.m_ctebCreatorCustodianEidString = Uri::GetIpnUriString(ctebCustodianNode, ctebCustodianService);
        bv.AppendMoveCanonicalBlock(std::move(blockPtr));
    }

    //add payload block
    {

        std::unique_ptr<Bpv6CanonicalBlock> blockPtr = boost::make_unique<Bpv6CanonicalBlock>();
        Bpv6CanonicalBlock & block = *blockPtr;
        //block.SetZero();

        block.m_blockTypeCode = BPV6_BLOCK_TYPE_CODE::PAYLOAD;
        block.m_blockProcessingControlFlags = BPV6_BLOCKFLAG::NO_FLAGS_SET;
        block.m_blockTypeSpecificDataLength = bundleDataStr.length();
        block.m_blockTypeSpecificDataPtr = (uint8_t*)bundleDataStr.data(); //bundleDataStr must remain in scope until after render
        bv.AppendMoveCanonicalBlock(std::move(blockPtr));

    }

    BOOST_REQUIRE(bv.Render(5000));
    
}

static void GenerateBundleWithoutCteb(uint64_t primaryCustodianNode, uint64_t primaryCustodianService,
    const std::string & bundleDataStr, BundleViewV6 & bundleViewWithoutCteb)
{
    BundleViewV6 & bv = bundleViewWithoutCteb;
    Bpv6CbhePrimaryBlock & primary = bv.m_primaryBlockView.header;
    primary.SetZero();


    primary.m_bundleProcessingControlFlags = BPV6_BUNDLEFLAG::PRIORITY_EXPEDITED | BPV6_BUNDLEFLAG::SINGLETON | BPV6_BUNDLEFLAG::NOFRAGMENT | BPV6_BUNDLEFLAG::CUSTODY_REQUESTED;
    primary.m_sourceNodeId.Set(PRIMARY_SRC_NODE, PRIMARY_SRC_SVC);
    primary.m_destinationEid.Set(PRIMARY_DEST_NODE, PRIMARY_DEST_SVC);
    primary.m_custodianEid.Set(primaryCustodianNode, primaryCustodianService);
    primary.m_creationTimestamp.secondsSinceStartOfYear2000 = PRIMARY_TIME; //(uint64_t)bpv6_unix_to_5050(curr_time);
    primary.m_lifetimeSeconds = PRIMARY_LIFETIME;
    primary.m_creationTimestamp.sequenceNumber = PRIMARY_SEQ;
    bv.m_primaryBlockView.SetManuallyModified();

    //add payload block
    {

        std::unique_ptr<Bpv6CanonicalBlock> blockPtr = boost::make_unique<Bpv6CanonicalBlock>();
        Bpv6CanonicalBlock & block = *blockPtr;
        //block.SetZero();

        block.m_blockTypeCode = BPV6_BLOCK_TYPE_CODE::PAYLOAD;
        block.m_blockProcessingControlFlags = BPV6_BLOCKFLAG::NO_FLAGS_SET;
        block.m_blockTypeSpecificDataLength = bundleDataStr.length();
        block.m_blockTypeSpecificDataPtr = (uint8_t*)bundleDataStr.data(); //bundleDataStr must remain in scope until after render
        bv.AppendMoveCanonicalBlock(std::move(blockPtr));

    }

    BOOST_REQUIRE(bv.Render(5000));
}

static void GenerateBundleForStatusReport(cbhe_eid_t custodianEid, cbhe_eid_t reportToEid,
    const std::string & data, BundleViewV6 & bv) {
    GenerateBundleWithoutCteb(custodianEid.nodeId, custodianEid.serviceId, data, bv);

    bv.m_primaryBlockView.header.m_reportToEid = reportToEid;
    bv.m_primaryBlockView.SetManuallyModified();
    BOOST_REQUIRE(bv.Render(5000));
}

BOOST_AUTO_TEST_CASE(CustodyTransferTestCase)
{
    //create bundle that accepts custody from bundle originator which will be sent to hdtn
    const uint64_t srcCtebCustodyId = 10;
    const uint64_t newHdtnCtebCustodyId = 11;
    const std::string bundleDataStr = "bundle data!!!";
    //make valid cteb (primary custodian matches cteb custodian) from bundle originator
    const cbhe_eid_t custodianOriginator(PRIMARY_SRC_NODE, PRIMARY_SRC_SVC);
    BundleViewV6 bundleViewWithCteb;
    GenerateBundleWithCteb(
        PRIMARY_SRC_NODE, PRIMARY_SRC_SVC, //primary custodian
        PRIMARY_SRC_NODE, PRIMARY_SRC_SVC, srcCtebCustodyId, //cteb custodian
        bundleDataStr, bundleViewWithCteb);
    padded_vector_uint8_t validCtebBundle(bundleViewWithCteb.m_frontBuffer);
    BOOST_REQUIRE_GT(validCtebBundle.size(), 0);

    //create invalid cteb bundle where cteb doesnt match
    BundleViewV6 bundleViewWithInvalidCteb;
    GenerateBundleWithCteb(
        PRIMARY_SRC_NODE, PRIMARY_SRC_SVC, //primary custodian
        INVALID_CTEB_SRC_NODE, INVALID_CTEB_SRC_SVC, srcCtebCustodyId, //cteb custodian
        bundleDataStr, bundleViewWithInvalidCteb);
    padded_vector_uint8_t invalidCtebBundle(bundleViewWithInvalidCteb.m_frontBuffer);
    BOOST_REQUIRE_GT(invalidCtebBundle.size(), 0);

    //create no cteb bundle where cteb missing
    BundleViewV6 bundleViewWithoutCteb;
    //make valid cteb (primary custodian matches cteb custodian) from bundle originator
    GenerateBundleWithoutCteb(
        PRIMARY_SRC_NODE, PRIMARY_SRC_SVC, //primary custodian
        bundleDataStr, bundleViewWithoutCteb);
    padded_vector_uint8_t missingCtebBundle(bundleViewWithoutCteb.m_frontBuffer);
    BOOST_REQUIRE_GT(missingCtebBundle.size(), 0);

    

    //bundle with custody bit set, hdtn acs enabled and accepts custody, valid cteb
    {
        padded_vector_uint8_t bundleData(validCtebBundle);
        BundleViewV6 bv;
        //std::cout << "sz " << bundleSerializedCopy.size() << std::endl;
        Bpv6CbhePrimaryBlock originalPrimaryFromOriginator;
        BOOST_REQUIRE(bv.SwapInAndLoadBundle(bundleData));
        { //check primary
            Bpv6CbhePrimaryBlock & primary = bv.m_primaryBlockView.header;
            originalPrimaryFromOriginator = primary;
            const BPV6_BUNDLEFLAG requiredPrimaryFlags = BPV6_BUNDLEFLAG::SINGLETON | BPV6_BUNDLEFLAG::NOFRAGMENT | BPV6_BUNDLEFLAG::CUSTODY_REQUESTED;
            BOOST_REQUIRE_EQUAL(primary.m_bundleProcessingControlFlags & requiredPrimaryFlags, requiredPrimaryFlags);
            BOOST_REQUIRE_EQUAL(primary.m_custodianEid, cbhe_eid_t(PRIMARY_SRC_NODE, PRIMARY_SRC_SVC));
        }
        { //check cteb
            BOOST_REQUIRE_EQUAL(bv.GetNumCanonicalBlocks(), 2); //payload + cteb

            std::vector<BundleViewV6::Bpv6CanonicalBlockView*> blocks;
            bv.GetCanonicalBlocksByType(BPV6_BLOCK_TYPE_CODE::CUSTODY_TRANSFER_ENHANCEMENT, blocks);
            BOOST_REQUIRE_EQUAL(blocks.size(), 1);
            Bpv6CustodyTransferEnhancementBlock* ctebBlockPtr = dynamic_cast<Bpv6CustodyTransferEnhancementBlock*>(blocks[0]->headerPtr.get());
            BOOST_REQUIRE(ctebBlockPtr);
            BOOST_REQUIRE_EQUAL(ctebBlockPtr->m_blockTypeCode, BPV6_BLOCK_TYPE_CODE::CUSTODY_TRANSFER_ENHANCEMENT);
            
            BOOST_REQUIRE_EQUAL(ctebBlockPtr->m_custodyId, srcCtebCustodyId);
            BOOST_REQUIRE_EQUAL(ctebBlockPtr->m_ctebCreatorCustodianEidString, PRIMARY_SRC_URI); //cteb matches primary custodian
            BOOST_REQUIRE_EQUAL(blocks[0]->actualSerializedBlockPtr.size(), ctebBlockPtr->GetSerializationSize());
        }

        //hdtn node accept custody with acs
        const bool isAcsAware = true;
        CustodyTransferManager ctmHdtn(isAcsAware, PRIMARY_HDTN_NODE, PRIMARY_HDTN_SVC);
        BOOST_REQUIRE_EQUAL(ctmHdtn.GetAcsConstRef(custodianOriginator, BPV6_ACS_STATUS_REASON_INDICES::SUCCESS__NO_ADDITIONAL_INFORMATION).m_custodyIdFills.size(), 0);
        BundleViewV6 custodySignalRfc5050RenderedBundleView;
        BOOST_REQUIRE(ctmHdtn.ProcessCustodyOfBundle(bv, true, newHdtnCtebCustodyId, BPV6_ACS_STATUS_REASON_INDICES::SUCCESS__NO_ADDITIONAL_INFORMATION,
            custodySignalRfc5050RenderedBundleView));
        BOOST_REQUIRE_EQUAL(ctmHdtn.GetAcsConstRef(custodianOriginator, BPV6_ACS_STATUS_REASON_INDICES::SUCCESS__NO_ADDITIONAL_INFORMATION).m_custodyIdFills.size(), 1);
        BOOST_REQUIRE_EQUAL(custodySignalRfc5050RenderedBundleView.m_renderedBundle.size(), 0);
        //pretend it was time to send acs
        BundleViewV6 newAcsRenderedBundleView;
        padded_vector_uint8_t& serializedAcsBundleFromHdtn = newAcsRenderedBundleView.m_frontBuffer;
        //originalPrimaryFromOriginator.bpv6_primary_block_print();
        BOOST_REQUIRE(ctmHdtn.GenerateAcsBundle(newAcsRenderedBundleView,
            originalPrimaryFromOriginator.m_custodianEid,
            BPV6_ACS_STATUS_REASON_INDICES::SUCCESS__NO_ADDITIONAL_INFORMATION, true)); //true => just copy as not to mess up the next generate all test
        //test with generate all
        std::list<BundleViewV6> newAcsRenderedBundleViewList;
        BOOST_REQUIRE_EQUAL(ctmHdtn.GetLargestNumberOfFills(), 1);
        BOOST_REQUIRE(ctmHdtn.GenerateAllAcsBundlesAndClear(newAcsRenderedBundleViewList));
        BOOST_REQUIRE_EQUAL(ctmHdtn.GetLargestNumberOfFills(), 0);
        BOOST_REQUIRE_EQUAL(newAcsRenderedBundleViewList.size(), 1);
        //all cleared at this point
        BOOST_REQUIRE(ctmHdtn.GenerateAllAcsBundlesAndClear(newAcsRenderedBundleViewList));
        BOOST_REQUIRE_EQUAL(newAcsRenderedBundleViewList.size(), 0);
        //hdtn modifies bundle for next hop
        BOOST_REQUIRE(bv.Render(2000));
        bundleData.swap(bv.m_frontBuffer); //bundleData is now hdtn's modified bundle for next hop
        BOOST_REQUIRE(bv.SwapInAndLoadBundle(bundleData)); //bv is now hdtn's
        { //check new primary
            Bpv6CbhePrimaryBlock & primary = bv.m_primaryBlockView.header;
            const BPV6_BUNDLEFLAG requiredPrimaryFlags = BPV6_BUNDLEFLAG::SINGLETON | BPV6_BUNDLEFLAG::NOFRAGMENT | BPV6_BUNDLEFLAG::CUSTODY_REQUESTED;
            BOOST_REQUIRE_EQUAL(primary.m_bundleProcessingControlFlags & requiredPrimaryFlags, requiredPrimaryFlags);
            BOOST_REQUIRE_EQUAL(primary.m_custodianEid, cbhe_eid_t(PRIMARY_HDTN_NODE, PRIMARY_HDTN_SVC)); //hdtn is new custodian
        }
        { //check new cteb
            BOOST_REQUIRE_EQUAL(bv.GetNumCanonicalBlocks(), 2); //payload + cteb

            std::vector<BundleViewV6::Bpv6CanonicalBlockView*> blocks;
            bv.GetCanonicalBlocksByType(BPV6_BLOCK_TYPE_CODE::CUSTODY_TRANSFER_ENHANCEMENT, blocks);
            BOOST_REQUIRE_EQUAL(blocks.size(), 1);
            Bpv6CustodyTransferEnhancementBlock* ctebBlockPtr = dynamic_cast<Bpv6CustodyTransferEnhancementBlock*>(blocks[0]->headerPtr.get());
            BOOST_REQUIRE(ctebBlockPtr);
            BOOST_REQUIRE_EQUAL(ctebBlockPtr->m_blockTypeCode, BPV6_BLOCK_TYPE_CODE::CUSTODY_TRANSFER_ENHANCEMENT);
            BOOST_REQUIRE_EQUAL(ctebBlockPtr->m_custodyId, newHdtnCtebCustodyId);
            BOOST_REQUIRE_EQUAL(ctebBlockPtr->m_ctebCreatorCustodianEidString, PRIMARY_HDTN_URI); //cteb matches new hdtn custodian
            BOOST_REQUIRE_EQUAL(blocks[0]->actualSerializedBlockPtr.size(), ctebBlockPtr->GetSerializationSize());
        }

        //source node receives acs
        {
            BundleViewV6 bvSrc;
            //std::cout << "sz " << bundleSerializedCopy.size() << std::endl;
            BOOST_REQUIRE(bvSrc.SwapInAndLoadBundle(serializedAcsBundleFromHdtn));
            { //check primary
                Bpv6CbhePrimaryBlock & primary = bvSrc.m_primaryBlockView.header;
                const BPV6_BUNDLEFLAG requiredPrimaryFlags = BPV6_BUNDLEFLAG::SINGLETON | BPV6_BUNDLEFLAG::NOFRAGMENT | BPV6_BUNDLEFLAG::ADMINRECORD;
                BOOST_REQUIRE_EQUAL(primary.m_bundleProcessingControlFlags & requiredPrimaryFlags, requiredPrimaryFlags);
                BOOST_REQUIRE_EQUAL(primary.m_custodianEid, cbhe_eid_t(0, 0));
                BOOST_REQUIRE_EQUAL(primary.m_sourceNodeId, cbhe_eid_t(PRIMARY_HDTN_NODE, PRIMARY_HDTN_SVC));
                BOOST_REQUIRE_EQUAL(primary.m_destinationEid, cbhe_eid_t(PRIMARY_SRC_NODE, PRIMARY_SRC_SVC));
            }
            //get ACS payload block and check acs
            {
                std::vector<BundleViewV6::Bpv6CanonicalBlockView*> blocks;
                bvSrc.GetCanonicalBlocksByType(BPV6_BLOCK_TYPE_CODE::PAYLOAD, blocks);
                BOOST_REQUIRE_EQUAL(blocks.size(), 1);
                Bpv6AdministrativeRecord* adminRecordBlockPtr = dynamic_cast<Bpv6AdministrativeRecord*>(blocks[0]->headerPtr.get());
                BOOST_REQUIRE(adminRecordBlockPtr);
                BOOST_REQUIRE_EQUAL(adminRecordBlockPtr->m_blockTypeCode, BPV6_BLOCK_TYPE_CODE::PAYLOAD);
                BOOST_REQUIRE_EQUAL(adminRecordBlockPtr->m_adminRecordTypeCode, BPV6_ADMINISTRATIVE_RECORD_TYPE_CODE::AGGREGATE_CUSTODY_SIGNAL);

                BOOST_REQUIRE_EQUAL(blocks[0]->actualSerializedBlockPtr.size(), adminRecordBlockPtr->GetSerializationSize());

                Bpv6AdministrativeRecordContentAggregateCustodySignal * acsPtr = dynamic_cast<Bpv6AdministrativeRecordContentAggregateCustodySignal*>(adminRecordBlockPtr->m_adminRecordContentPtr.get());
                BOOST_REQUIRE(acsPtr);
                Bpv6AdministrativeRecordContentAggregateCustodySignal & acs = *(reinterpret_cast<Bpv6AdministrativeRecordContentAggregateCustodySignal*>(acsPtr));
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
        padded_vector_uint8_t bundleData(invalidCtebBundle);
        BundleViewV6 bv;
        //std::cout << "sz " << bundleSerializedCopy.size() << std::endl;
        Bpv6CbhePrimaryBlock originalPrimaryFromOriginator;
        BOOST_REQUIRE(bv.SwapInAndLoadBundle(bundleData));
        { //check primary
            Bpv6CbhePrimaryBlock & primary = bv.m_primaryBlockView.header;
            originalPrimaryFromOriginator = primary;
            const BPV6_BUNDLEFLAG requiredPrimaryFlags = BPV6_BUNDLEFLAG::SINGLETON | BPV6_BUNDLEFLAG::NOFRAGMENT | BPV6_BUNDLEFLAG::CUSTODY_REQUESTED;
            BOOST_REQUIRE_EQUAL(primary.m_bundleProcessingControlFlags & requiredPrimaryFlags, requiredPrimaryFlags);
            BOOST_REQUIRE_EQUAL(primary.m_custodianEid, cbhe_eid_t(PRIMARY_SRC_NODE, PRIMARY_SRC_SVC));
        }
        { //check cteb
            BOOST_REQUIRE_EQUAL(bv.GetNumCanonicalBlocks(), 2); //payload + cteb

            std::vector<BundleViewV6::Bpv6CanonicalBlockView*> blocks;
            bv.GetCanonicalBlocksByType(BPV6_BLOCK_TYPE_CODE::CUSTODY_TRANSFER_ENHANCEMENT, blocks);
            BOOST_REQUIRE_EQUAL(blocks.size(), 1);
            Bpv6CustodyTransferEnhancementBlock* ctebBlockPtr = dynamic_cast<Bpv6CustodyTransferEnhancementBlock*>(blocks[0]->headerPtr.get());
            BOOST_REQUIRE(ctebBlockPtr);
            BOOST_REQUIRE_EQUAL(ctebBlockPtr->m_blockTypeCode, BPV6_BLOCK_TYPE_CODE::CUSTODY_TRANSFER_ENHANCEMENT);
            BOOST_REQUIRE_EQUAL(ctebBlockPtr->m_ctebCreatorCustodianEidString, INVALID_CTEB_SRC_URI); //cteb matches primary custodian
            BOOST_REQUIRE_EQUAL(blocks[0]->actualSerializedBlockPtr.size(), ctebBlockPtr->GetSerializationSize());
        }
        
        //hdtn node accept custody with acs
        const bool isAcsAware = true;
        CustodyTransferManager ctmHdtn(isAcsAware, PRIMARY_HDTN_NODE, PRIMARY_HDTN_SVC);
        BOOST_REQUIRE_EQUAL(ctmHdtn.GetAcsConstRef(custodianOriginator, BPV6_ACS_STATUS_REASON_INDICES::SUCCESS__NO_ADDITIONAL_INFORMATION).m_custodyIdFills.size(), 0);
        BundleViewV6 custodySignalRfc5050RenderedBundleView;
        BOOST_REQUIRE(ctmHdtn.ProcessCustodyOfBundle(bv, true, newHdtnCtebCustodyId, BPV6_ACS_STATUS_REASON_INDICES::SUCCESS__NO_ADDITIONAL_INFORMATION,
            custodySignalRfc5050RenderedBundleView));
        BOOST_REQUIRE_EQUAL(ctmHdtn.GetAcsConstRef(custodianOriginator, BPV6_ACS_STATUS_REASON_INDICES::SUCCESS__NO_ADDITIONAL_INFORMATION).m_custodyIdFills.size(), 0); //acs not used due to invalid cteb
        BOOST_REQUIRE_GT(custodySignalRfc5050RenderedBundleView.m_renderedBundle.size(), 0); //using 5050 custody signal
        
        //hdtn modifies bundle for next hop
        BOOST_REQUIRE(bv.Render(2000));
        bundleData.swap(bv.m_frontBuffer); //bundleData is now hdtn's modified bundle for next hop
        BOOST_REQUIRE(bv.SwapInAndLoadBundle(bundleData)); //bv is now hdtn's
        { //check new primary
            Bpv6CbhePrimaryBlock & primary = bv.m_primaryBlockView.header;
            const BPV6_BUNDLEFLAG requiredPrimaryFlags = BPV6_BUNDLEFLAG::SINGLETON | BPV6_BUNDLEFLAG::NOFRAGMENT | BPV6_BUNDLEFLAG::CUSTODY_REQUESTED;
            BOOST_REQUIRE_EQUAL(primary.m_bundleProcessingControlFlags & requiredPrimaryFlags, requiredPrimaryFlags);
            BOOST_REQUIRE_EQUAL(primary.m_custodianEid, cbhe_eid_t(PRIMARY_HDTN_NODE, PRIMARY_HDTN_SVC)); //hdtn is new custodian
        }
        { //check new cteb
            BOOST_REQUIRE_EQUAL(bv.GetNumCanonicalBlocks(), 2); //payload + cteb

            std::vector<BundleViewV6::Bpv6CanonicalBlockView*> blocks;
            bv.GetCanonicalBlocksByType(BPV6_BLOCK_TYPE_CODE::CUSTODY_TRANSFER_ENHANCEMENT, blocks);
            BOOST_REQUIRE_EQUAL(blocks.size(), 1);
            Bpv6CustodyTransferEnhancementBlock* ctebBlockPtr = dynamic_cast<Bpv6CustodyTransferEnhancementBlock*>(blocks[0]->headerPtr.get());
            BOOST_REQUIRE(ctebBlockPtr);
            BOOST_REQUIRE_EQUAL(ctebBlockPtr->m_blockTypeCode, BPV6_BLOCK_TYPE_CODE::CUSTODY_TRANSFER_ENHANCEMENT);
            BOOST_REQUIRE_EQUAL(ctebBlockPtr->m_custodyId, newHdtnCtebCustodyId);
            BOOST_REQUIRE_EQUAL(ctebBlockPtr->m_ctebCreatorCustodianEidString, PRIMARY_HDTN_URI); //cteb matches new hdtn custodian
            BOOST_REQUIRE_EQUAL(blocks[0]->actualSerializedBlockPtr.size(), ctebBlockPtr->GetSerializationSize());
        }

        //source node receives 5050 custody signal success
        {
            BundleViewV6 bvSrc;
            //std::cout << "sz " << bundleSerializedCopy.size() << std::endl;
            BOOST_REQUIRE(bvSrc.SwapInAndLoadBundle(custodySignalRfc5050RenderedBundleView.m_frontBuffer));
            { //check primary
                Bpv6CbhePrimaryBlock & primary = bvSrc.m_primaryBlockView.header;
                const BPV6_BUNDLEFLAG requiredPrimaryFlags = BPV6_BUNDLEFLAG::SINGLETON | BPV6_BUNDLEFLAG::NOFRAGMENT | BPV6_BUNDLEFLAG::ADMINRECORD;
                BOOST_REQUIRE_EQUAL(primary.m_bundleProcessingControlFlags & requiredPrimaryFlags, requiredPrimaryFlags);
                BOOST_REQUIRE_EQUAL(primary.m_custodianEid, cbhe_eid_t(0, 0));
                BOOST_REQUIRE_EQUAL(primary.m_sourceNodeId, cbhe_eid_t(PRIMARY_HDTN_NODE, PRIMARY_HDTN_SVC));
                BOOST_REQUIRE_EQUAL(primary.m_destinationEid, cbhe_eid_t(PRIMARY_SRC_NODE, PRIMARY_SRC_SVC));
            }
            { //check admin record
                std::vector<BundleViewV6::Bpv6CanonicalBlockView*> blocks;
                bvSrc.GetCanonicalBlocksByType(BPV6_BLOCK_TYPE_CODE::PAYLOAD, blocks);
                BOOST_REQUIRE_EQUAL(blocks.size(), 1);
                Bpv6AdministrativeRecord* adminRecordBlockPtr = dynamic_cast<Bpv6AdministrativeRecord*>(blocks[0]->headerPtr.get());
                BOOST_REQUIRE(adminRecordBlockPtr);
                BOOST_REQUIRE_EQUAL(adminRecordBlockPtr->m_blockTypeCode, BPV6_BLOCK_TYPE_CODE::PAYLOAD);
                BOOST_REQUIRE_EQUAL(adminRecordBlockPtr->m_adminRecordTypeCode, BPV6_ADMINISTRATIVE_RECORD_TYPE_CODE::CUSTODY_SIGNAL);

                BOOST_REQUIRE_EQUAL(blocks[0]->actualSerializedBlockPtr.size(), adminRecordBlockPtr->GetSerializationSize());

                Bpv6AdministrativeRecordContentCustodySignal * csPtr = dynamic_cast<Bpv6AdministrativeRecordContentCustodySignal*>(adminRecordBlockPtr->m_adminRecordContentPtr.get());
                BOOST_REQUIRE(csPtr);
                Bpv6AdministrativeRecordContentCustodySignal & cs = *(reinterpret_cast<Bpv6AdministrativeRecordContentCustodySignal*>(csPtr));
                BOOST_REQUIRE(cs.DidCustodyTransferSucceed());
                BOOST_REQUIRE(cs.GetReasonCode() == BPV6_CUSTODY_SIGNAL_REASON_CODES_7BIT::NO_ADDITIONAL_INFORMATION);
                BOOST_REQUIRE_EQUAL(cs.m_bundleSourceEid, PRIMARY_SRC_URI);
            }
        }
    }


    //bundle with custody bit set, hdtn acs enabled and accepts custody, missing cteb
    {
        padded_vector_uint8_t bundleData(missingCtebBundle);
        BundleViewV6 bv;
        //std::cout << "sz " << bundleSerializedCopy.size() << std::endl;
        Bpv6CbhePrimaryBlock originalPrimaryFromOriginator;
        BOOST_REQUIRE(bv.SwapInAndLoadBundle(bundleData));
        { //check primary
            Bpv6CbhePrimaryBlock & primary = bv.m_primaryBlockView.header;
            originalPrimaryFromOriginator = primary;
            const BPV6_BUNDLEFLAG requiredPrimaryFlags = BPV6_BUNDLEFLAG::SINGLETON | BPV6_BUNDLEFLAG::NOFRAGMENT | BPV6_BUNDLEFLAG::CUSTODY_REQUESTED;
            BOOST_REQUIRE_EQUAL(primary.m_bundleProcessingControlFlags & requiredPrimaryFlags, requiredPrimaryFlags);
            BOOST_REQUIRE_EQUAL(primary.m_custodianEid, cbhe_eid_t(PRIMARY_SRC_NODE, PRIMARY_SRC_SVC));
        }
        { //check cteb missing
            BOOST_REQUIRE_EQUAL(bv.GetNumCanonicalBlocks(), 1); //payload only
        }

        //hdtn node accept custody with acs
        const bool isAcsAware = true;
        CustodyTransferManager ctmHdtn(isAcsAware, PRIMARY_HDTN_NODE, PRIMARY_HDTN_SVC);
        BOOST_REQUIRE_EQUAL(ctmHdtn.GetAcsConstRef(custodianOriginator, BPV6_ACS_STATUS_REASON_INDICES::SUCCESS__NO_ADDITIONAL_INFORMATION).m_custodyIdFills.size(), 0);
        BundleViewV6 custodySignalRfc5050RenderedBundleView;
        BOOST_REQUIRE(ctmHdtn.ProcessCustodyOfBundle(bv, true, newHdtnCtebCustodyId, BPV6_ACS_STATUS_REASON_INDICES::SUCCESS__NO_ADDITIONAL_INFORMATION,
            custodySignalRfc5050RenderedBundleView));
        BOOST_REQUIRE_EQUAL(ctmHdtn.GetAcsConstRef(custodianOriginator, BPV6_ACS_STATUS_REASON_INDICES::SUCCESS__NO_ADDITIONAL_INFORMATION).m_custodyIdFills.size(), 0); //acs not used due to invalid cteb
        BOOST_REQUIRE_GT(custodySignalRfc5050RenderedBundleView.m_renderedBundle.size(), 0); //using 5050 custody signal

        //hdtn modifies bundle for next hop
        BOOST_REQUIRE(bv.Render(2000));
        bundleData.swap(bv.m_frontBuffer); //bundleData is now hdtn's modified bundle for next hop
        BOOST_REQUIRE(bv.SwapInAndLoadBundle(bundleData)); //bv is now hdtn's
        { //check new primary
            Bpv6CbhePrimaryBlock & primary = bv.m_primaryBlockView.header;
            const BPV6_BUNDLEFLAG requiredPrimaryFlags = BPV6_BUNDLEFLAG::SINGLETON | BPV6_BUNDLEFLAG::NOFRAGMENT | BPV6_BUNDLEFLAG::CUSTODY_REQUESTED;
            BOOST_REQUIRE_EQUAL(primary.m_bundleProcessingControlFlags & requiredPrimaryFlags, requiredPrimaryFlags);
            BOOST_REQUIRE_EQUAL(primary.m_custodianEid, cbhe_eid_t(PRIMARY_HDTN_NODE, PRIMARY_HDTN_SVC)); //hdtn is new custodian
        }
        { //check new cteb WAS CREATED/APPENDED
            BOOST_REQUIRE_EQUAL(bv.GetNumCanonicalBlocks(), 2); //payload + cteb

            std::vector<BundleViewV6::Bpv6CanonicalBlockView*> blocks;
            bv.GetCanonicalBlocksByType(BPV6_BLOCK_TYPE_CODE::CUSTODY_TRANSFER_ENHANCEMENT, blocks);
            BOOST_REQUIRE_EQUAL(blocks.size(), 1);
            Bpv6CustodyTransferEnhancementBlock* ctebBlockPtr = dynamic_cast<Bpv6CustodyTransferEnhancementBlock*>(blocks[0]->headerPtr.get());
            BOOST_REQUIRE(ctebBlockPtr);
            BOOST_REQUIRE_EQUAL(ctebBlockPtr->m_blockTypeCode, BPV6_BLOCK_TYPE_CODE::CUSTODY_TRANSFER_ENHANCEMENT);
            BOOST_REQUIRE_EQUAL(ctebBlockPtr->m_custodyId, newHdtnCtebCustodyId);
            BOOST_REQUIRE_EQUAL(ctebBlockPtr->m_ctebCreatorCustodianEidString, PRIMARY_HDTN_URI); //cteb matches new hdtn custodian
            BOOST_REQUIRE_EQUAL(blocks[0]->actualSerializedBlockPtr.size(), ctebBlockPtr->GetSerializationSize());
        }
        
        //source node receives 5050 custody signal success
        {
            BundleViewV6 bvSrc;
            //std::cout << "sz " << bundleSerializedCopy.size() << std::endl;
            BOOST_REQUIRE(bvSrc.SwapInAndLoadBundle(custodySignalRfc5050RenderedBundleView.m_frontBuffer));
            { //check primary
                Bpv6CbhePrimaryBlock & primary = bvSrc.m_primaryBlockView.header;
                const BPV6_BUNDLEFLAG requiredPrimaryFlags = BPV6_BUNDLEFLAG::SINGLETON | BPV6_BUNDLEFLAG::NOFRAGMENT | BPV6_BUNDLEFLAG::ADMINRECORD;
                BOOST_REQUIRE_EQUAL(primary.m_bundleProcessingControlFlags & requiredPrimaryFlags, requiredPrimaryFlags);
                BOOST_REQUIRE_EQUAL(primary.m_custodianEid, cbhe_eid_t(0, 0));
                BOOST_REQUIRE_EQUAL(primary.m_sourceNodeId, cbhe_eid_t(PRIMARY_HDTN_NODE, PRIMARY_HDTN_SVC));
                BOOST_REQUIRE_EQUAL(primary.m_destinationEid, cbhe_eid_t(PRIMARY_SRC_NODE, PRIMARY_SRC_SVC));
            }
            { //check admin record
                std::vector<BundleViewV6::Bpv6CanonicalBlockView*> blocks;
                bvSrc.GetCanonicalBlocksByType(BPV6_BLOCK_TYPE_CODE::PAYLOAD, blocks);
                BOOST_REQUIRE_EQUAL(blocks.size(), 1);
                Bpv6AdministrativeRecord* adminRecordBlockPtr = dynamic_cast<Bpv6AdministrativeRecord*>(blocks[0]->headerPtr.get());
                BOOST_REQUIRE(adminRecordBlockPtr);
                BOOST_REQUIRE_EQUAL(adminRecordBlockPtr->m_blockTypeCode, BPV6_BLOCK_TYPE_CODE::PAYLOAD);
                BOOST_REQUIRE_EQUAL(adminRecordBlockPtr->m_adminRecordTypeCode, BPV6_ADMINISTRATIVE_RECORD_TYPE_CODE::CUSTODY_SIGNAL);

                BOOST_REQUIRE_EQUAL(blocks[0]->actualSerializedBlockPtr.size(), adminRecordBlockPtr->GetSerializationSize());

                Bpv6AdministrativeRecordContentCustodySignal * csPtr = dynamic_cast<Bpv6AdministrativeRecordContentCustodySignal*>(adminRecordBlockPtr->m_adminRecordContentPtr.get());
                BOOST_REQUIRE(csPtr);
                Bpv6AdministrativeRecordContentCustodySignal & cs = *(reinterpret_cast<Bpv6AdministrativeRecordContentCustodySignal*>(csPtr));
                BOOST_REQUIRE(cs.DidCustodyTransferSucceed());
                BOOST_REQUIRE(cs.GetReasonCode() == BPV6_CUSTODY_SIGNAL_REASON_CODES_7BIT::NO_ADDITIONAL_INFORMATION);
                BOOST_REQUIRE_EQUAL(cs.m_bundleSourceEid, PRIMARY_SRC_URI);
            }
        }
    }


    //bundle with custody bit set, hdtn acs enabled and refuses custody, valid cteb
    {
        padded_vector_uint8_t bundleData(validCtebBundle);
        BundleViewV6 bv;
        //std::cout << "sz " << bundleSerializedCopy.size() << std::endl;
        BOOST_REQUIRE(bv.SwapInAndLoadBundle(bundleData));
        const Bpv6CbhePrimaryBlock originalPrimaryFromOriginator = bv.m_primaryBlockView.header;
        
        //hdtn node refuses custody with acs
        const bool isAcsAware = true;
        CustodyTransferManager ctmHdtn(isAcsAware, PRIMARY_HDTN_NODE, PRIMARY_HDTN_SVC);
        BOOST_REQUIRE_EQUAL(ctmHdtn.GetAcsConstRef(custodianOriginator, BPV6_ACS_STATUS_REASON_INDICES::FAIL__DEPLETED_STORAGE).m_custodyIdFills.size(), 0);
        BundleViewV6 custodySignalRfc5050RenderedBundleView;
        BOOST_REQUIRE(ctmHdtn.ProcessCustodyOfBundle(bv, false, newHdtnCtebCustodyId, BPV6_ACS_STATUS_REASON_INDICES::FAIL__DEPLETED_STORAGE,
            custodySignalRfc5050RenderedBundleView));
        BOOST_REQUIRE_EQUAL(ctmHdtn.GetAcsConstRef(custodianOriginator, BPV6_ACS_STATUS_REASON_INDICES::FAIL__DEPLETED_STORAGE).m_custodyIdFills.size(), 1);
        BOOST_REQUIRE_EQUAL(custodySignalRfc5050RenderedBundleView.m_renderedBundle.size(), 0);
        //pretend it was time to send acs
        BundleViewV6 newAcsRenderedBundleView;
        padded_vector_uint8_t& serializedAcsBundleFromHdtn = newAcsRenderedBundleView.m_frontBuffer;
        //originalPrimaryFromOriginator.bpv6_primary_block_print();
        BOOST_REQUIRE(ctmHdtn.GenerateAcsBundle(newAcsRenderedBundleView,
            originalPrimaryFromOriginator.m_custodianEid,
            BPV6_ACS_STATUS_REASON_INDICES::FAIL__DEPLETED_STORAGE, true)); //true => just copy as not to mess up the next generate all test
        //test with generate all
        std::list<BundleViewV6> newAcsRenderedBundleViewList;
        BOOST_REQUIRE_EQUAL(ctmHdtn.GetLargestNumberOfFills(), 1);
        BOOST_REQUIRE(ctmHdtn.GenerateAllAcsBundlesAndClear(newAcsRenderedBundleViewList));
        BOOST_REQUIRE_EQUAL(ctmHdtn.GetLargestNumberOfFills(), 0);
        BOOST_REQUIRE_EQUAL(newAcsRenderedBundleViewList.size(), 1);
        //all cleared at this point
        BOOST_REQUIRE(ctmHdtn.GenerateAllAcsBundlesAndClear(newAcsRenderedBundleViewList));
        BOOST_REQUIRE_EQUAL(newAcsRenderedBundleViewList.size(), 0);
        //hdtn does not modify bundle for next hop because custody was refused
        
        //source node receives acs
        {
            BundleViewV6 bvSrc;
            //std::cout << "sz " << bundleSerializedCopy.size() << std::endl;
            BOOST_REQUIRE(bvSrc.SwapInAndLoadBundle(serializedAcsBundleFromHdtn));
            { //check primary
                Bpv6CbhePrimaryBlock & primary = bvSrc.m_primaryBlockView.header;
                const BPV6_BUNDLEFLAG requiredPrimaryFlags = BPV6_BUNDLEFLAG::SINGLETON | BPV6_BUNDLEFLAG::NOFRAGMENT | BPV6_BUNDLEFLAG::ADMINRECORD;
                BOOST_REQUIRE_EQUAL(primary.m_bundleProcessingControlFlags & requiredPrimaryFlags, requiredPrimaryFlags);
                BOOST_REQUIRE_EQUAL(primary.m_custodianEid, cbhe_eid_t(0,0));
                BOOST_REQUIRE_EQUAL(primary.m_sourceNodeId, cbhe_eid_t(PRIMARY_HDTN_NODE, PRIMARY_HDTN_SVC));
                BOOST_REQUIRE_EQUAL(primary.m_destinationEid, cbhe_eid_t(PRIMARY_SRC_NODE, PRIMARY_SRC_SVC));
            }
            //get ACS payload block and check acs
            {
                std::vector<BundleViewV6::Bpv6CanonicalBlockView*> blocks;
                bvSrc.GetCanonicalBlocksByType(BPV6_BLOCK_TYPE_CODE::PAYLOAD, blocks);
                BOOST_REQUIRE_EQUAL(blocks.size(), 1);
                Bpv6AdministrativeRecord* adminRecordBlockPtr = dynamic_cast<Bpv6AdministrativeRecord*>(blocks[0]->headerPtr.get());
                BOOST_REQUIRE(adminRecordBlockPtr);
                BOOST_REQUIRE_EQUAL(adminRecordBlockPtr->m_blockTypeCode, BPV6_BLOCK_TYPE_CODE::PAYLOAD);
                BOOST_REQUIRE_EQUAL(adminRecordBlockPtr->m_adminRecordTypeCode, BPV6_ADMINISTRATIVE_RECORD_TYPE_CODE::AGGREGATE_CUSTODY_SIGNAL);

                BOOST_REQUIRE_EQUAL(blocks[0]->actualSerializedBlockPtr.size(), adminRecordBlockPtr->GetSerializationSize());

                Bpv6AdministrativeRecordContentAggregateCustodySignal * acsPtr = dynamic_cast<Bpv6AdministrativeRecordContentAggregateCustodySignal*>(adminRecordBlockPtr->m_adminRecordContentPtr.get());
                BOOST_REQUIRE(acsPtr);
                Bpv6AdministrativeRecordContentAggregateCustodySignal & acs = *(reinterpret_cast<Bpv6AdministrativeRecordContentAggregateCustodySignal*>(acsPtr));
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
        padded_vector_uint8_t bundleData(invalidCtebBundle);
        BundleViewV6 bv;
        //std::cout << "sz " << bundleSerializedCopy.size() << std::endl;
        
        BOOST_REQUIRE(bv.SwapInAndLoadBundle(bundleData));
        const Bpv6CbhePrimaryBlock originalPrimaryFromOriginator = bv.m_primaryBlockView.header;
        
        //hdtn node accept custody with acs
        const bool isAcsAware = true;
        CustodyTransferManager ctmHdtn(isAcsAware, PRIMARY_HDTN_NODE, PRIMARY_HDTN_SVC);
        BOOST_REQUIRE_EQUAL(ctmHdtn.GetAcsConstRef(custodianOriginator, BPV6_ACS_STATUS_REASON_INDICES::FAIL__DEPLETED_STORAGE).m_custodyIdFills.size(), 0);
        BundleViewV6 custodySignalRfc5050RenderedBundleView;
        BOOST_REQUIRE(ctmHdtn.ProcessCustodyOfBundle(bv, false, newHdtnCtebCustodyId, BPV6_ACS_STATUS_REASON_INDICES::FAIL__DEPLETED_STORAGE,
            custodySignalRfc5050RenderedBundleView));
        BOOST_REQUIRE_EQUAL(ctmHdtn.GetAcsConstRef(custodianOriginator, BPV6_ACS_STATUS_REASON_INDICES::FAIL__DEPLETED_STORAGE).m_custodyIdFills.size(), 0); //acs not used due to invalid cteb
        BOOST_REQUIRE_GT(custodySignalRfc5050RenderedBundleView.m_renderedBundle.size(), 0); //using 5050 custody signal

        //hdtn does not modify bundle for next hop because custody was refused

        //source node receives 5050 custody signal success
        {
            BundleViewV6 bvSrc;
            //std::cout << "sz " << bundleSerializedCopy.size() << std::endl;
            BOOST_REQUIRE(bvSrc.SwapInAndLoadBundle(custodySignalRfc5050RenderedBundleView.m_frontBuffer));
            { //check primary
                Bpv6CbhePrimaryBlock & primary = bvSrc.m_primaryBlockView.header;
                const BPV6_BUNDLEFLAG requiredPrimaryFlags = BPV6_BUNDLEFLAG::SINGLETON | BPV6_BUNDLEFLAG::NOFRAGMENT | BPV6_BUNDLEFLAG::ADMINRECORD;
                BOOST_REQUIRE_EQUAL(primary.m_bundleProcessingControlFlags & requiredPrimaryFlags, requiredPrimaryFlags);
                BOOST_REQUIRE_EQUAL(primary.m_custodianEid, cbhe_eid_t(0,0));
                BOOST_REQUIRE_EQUAL(primary.m_sourceNodeId, cbhe_eid_t(PRIMARY_HDTN_NODE, PRIMARY_HDTN_SVC));
                BOOST_REQUIRE_EQUAL(primary.m_destinationEid, cbhe_eid_t(PRIMARY_SRC_NODE, PRIMARY_SRC_SVC));
            }
            { //check admin record
                std::vector<BundleViewV6::Bpv6CanonicalBlockView*> blocks;
                bvSrc.GetCanonicalBlocksByType(BPV6_BLOCK_TYPE_CODE::PAYLOAD, blocks);
                BOOST_REQUIRE_EQUAL(blocks.size(), 1);
                Bpv6AdministrativeRecord* adminRecordBlockPtr = dynamic_cast<Bpv6AdministrativeRecord*>(blocks[0]->headerPtr.get());
                BOOST_REQUIRE(adminRecordBlockPtr);
                BOOST_REQUIRE_EQUAL(adminRecordBlockPtr->m_blockTypeCode, BPV6_BLOCK_TYPE_CODE::PAYLOAD);
                BOOST_REQUIRE_EQUAL(adminRecordBlockPtr->m_adminRecordTypeCode, BPV6_ADMINISTRATIVE_RECORD_TYPE_CODE::CUSTODY_SIGNAL);

                BOOST_REQUIRE_EQUAL(blocks[0]->actualSerializedBlockPtr.size(), adminRecordBlockPtr->GetSerializationSize());

                Bpv6AdministrativeRecordContentCustodySignal * csPtr = dynamic_cast<Bpv6AdministrativeRecordContentCustodySignal*>(adminRecordBlockPtr->m_adminRecordContentPtr.get());
                BOOST_REQUIRE(csPtr);
                Bpv6AdministrativeRecordContentCustodySignal & cs = *(reinterpret_cast<Bpv6AdministrativeRecordContentCustodySignal*>(csPtr));
                BOOST_REQUIRE(!cs.DidCustodyTransferSucceed());
                BOOST_REQUIRE(cs.GetReasonCode() == BPV6_CUSTODY_SIGNAL_REASON_CODES_7BIT::DEPLETED_STORAGE);
                BOOST_REQUIRE_EQUAL(cs.m_bundleSourceEid, PRIMARY_SRC_URI);
            }
        }
    }


    //bundle with custody bit set, hdtn acs disabled and accepts custody (still valid cteb bundle)
    {
        padded_vector_uint8_t bundleData(validCtebBundle);
        BundleViewV6 bv;
        //std::cout << "sz " << bundleSerializedCopy.size() << std::endl;
        
        BOOST_REQUIRE(bv.SwapInAndLoadBundle(bundleData));
        const Bpv6CbhePrimaryBlock originalPrimaryFromOriginator = bv.m_primaryBlockView.header;
            

        //hdtn node accept custody without acs
        const bool isAcsAware = false;
        CustodyTransferManager ctmHdtn(isAcsAware, PRIMARY_HDTN_NODE, PRIMARY_HDTN_SVC);
        BOOST_REQUIRE_EQUAL(ctmHdtn.GetAcsConstRef(custodianOriginator, BPV6_ACS_STATUS_REASON_INDICES::SUCCESS__NO_ADDITIONAL_INFORMATION).m_custodyIdFills.size(), 0);
        BundleViewV6 custodySignalRfc5050RenderedBundleView;
        BOOST_REQUIRE(ctmHdtn.ProcessCustodyOfBundle(bv, true, newHdtnCtebCustodyId, BPV6_ACS_STATUS_REASON_INDICES::SUCCESS__NO_ADDITIONAL_INFORMATION,
            custodySignalRfc5050RenderedBundleView));
        BOOST_REQUIRE_EQUAL(ctmHdtn.GetAcsConstRef(custodianOriginator, BPV6_ACS_STATUS_REASON_INDICES::SUCCESS__NO_ADDITIONAL_INFORMATION).m_custodyIdFills.size(), 0); //acs not used (disabled)
        BOOST_REQUIRE_GT(custodySignalRfc5050RenderedBundleView.m_renderedBundle.size(), 0); //using 5050 custody signal

        //hdtn modifies bundle for next hop
        BOOST_REQUIRE(bv.Render(2000));
        bundleData.swap(bv.m_frontBuffer); //bundleData is now hdtn's modified bundle for next hop
        BOOST_REQUIRE(bv.SwapInAndLoadBundle(bundleData)); //bv is now hdtn's
        { //check new primary
            Bpv6CbhePrimaryBlock & primary = bv.m_primaryBlockView.header;
            const BPV6_BUNDLEFLAG requiredPrimaryFlags = BPV6_BUNDLEFLAG::SINGLETON | BPV6_BUNDLEFLAG::NOFRAGMENT | BPV6_BUNDLEFLAG::CUSTODY_REQUESTED;
            BOOST_REQUIRE_EQUAL(primary.m_bundleProcessingControlFlags & requiredPrimaryFlags, requiredPrimaryFlags);
            BOOST_REQUIRE_EQUAL(primary.m_custodianEid, cbhe_eid_t(PRIMARY_HDTN_NODE, PRIMARY_HDTN_SVC)); //hdtn is new custodian
        }
        { //check cteb unchanged
            BOOST_REQUIRE_EQUAL(bv.GetNumCanonicalBlocks(), 2); //payload + cteb

            std::vector<BundleViewV6::Bpv6CanonicalBlockView*> blocks;
            bv.GetCanonicalBlocksByType(BPV6_BLOCK_TYPE_CODE::CUSTODY_TRANSFER_ENHANCEMENT, blocks);
            BOOST_REQUIRE_EQUAL(blocks.size(), 1);
            Bpv6CustodyTransferEnhancementBlock* ctebBlockPtr = dynamic_cast<Bpv6CustodyTransferEnhancementBlock*>(blocks[0]->headerPtr.get());
            BOOST_REQUIRE(ctebBlockPtr);
            BOOST_REQUIRE_EQUAL(ctebBlockPtr->m_blockTypeCode, BPV6_BLOCK_TYPE_CODE::CUSTODY_TRANSFER_ENHANCEMENT);
            BOOST_REQUIRE_EQUAL(ctebBlockPtr->m_ctebCreatorCustodianEidString, PRIMARY_SRC_URI); //cteb unmodified (still src)
            BOOST_REQUIRE_EQUAL(blocks[0]->actualSerializedBlockPtr.size(), ctebBlockPtr->GetSerializationSize());
        }

        //source node receives 5050 custody signal success
        {
            BundleViewV6 bvSrc;
            //std::cout << "sz " << bundleSerializedCopy.size() << std::endl;
            BOOST_REQUIRE(bvSrc.SwapInAndLoadBundle(custodySignalRfc5050RenderedBundleView.m_frontBuffer));
            { //check primary
                Bpv6CbhePrimaryBlock & primary = bvSrc.m_primaryBlockView.header;
                const BPV6_BUNDLEFLAG requiredPrimaryFlags = BPV6_BUNDLEFLAG::SINGLETON | BPV6_BUNDLEFLAG::NOFRAGMENT | BPV6_BUNDLEFLAG::ADMINRECORD;
                BOOST_REQUIRE_EQUAL(primary.m_bundleProcessingControlFlags & requiredPrimaryFlags, requiredPrimaryFlags);
                BOOST_REQUIRE_EQUAL(primary.m_custodianEid, cbhe_eid_t(0, 0));
                BOOST_REQUIRE_EQUAL(primary.m_sourceNodeId, cbhe_eid_t(PRIMARY_HDTN_NODE, PRIMARY_HDTN_SVC));
                BOOST_REQUIRE_EQUAL(primary.m_destinationEid, cbhe_eid_t(PRIMARY_SRC_NODE, PRIMARY_SRC_SVC));
            }
            { //check admin record
                std::vector<BundleViewV6::Bpv6CanonicalBlockView*> blocks;
                bvSrc.GetCanonicalBlocksByType(BPV6_BLOCK_TYPE_CODE::PAYLOAD, blocks);
                BOOST_REQUIRE_EQUAL(blocks.size(), 1);
                Bpv6AdministrativeRecord* adminRecordBlockPtr = dynamic_cast<Bpv6AdministrativeRecord*>(blocks[0]->headerPtr.get());
                BOOST_REQUIRE(adminRecordBlockPtr);
                BOOST_REQUIRE_EQUAL(adminRecordBlockPtr->m_blockTypeCode, BPV6_BLOCK_TYPE_CODE::PAYLOAD);
                BOOST_REQUIRE_EQUAL(adminRecordBlockPtr->m_adminRecordTypeCode, BPV6_ADMINISTRATIVE_RECORD_TYPE_CODE::CUSTODY_SIGNAL);

                BOOST_REQUIRE_EQUAL(blocks[0]->actualSerializedBlockPtr.size(), adminRecordBlockPtr->GetSerializationSize());

                Bpv6AdministrativeRecordContentCustodySignal * csPtr = dynamic_cast<Bpv6AdministrativeRecordContentCustodySignal*>(adminRecordBlockPtr->m_adminRecordContentPtr.get());
                BOOST_REQUIRE(csPtr);
                Bpv6AdministrativeRecordContentCustodySignal & cs = *(reinterpret_cast<Bpv6AdministrativeRecordContentCustodySignal*>(csPtr));
                BOOST_REQUIRE(cs.DidCustodyTransferSucceed());
                BOOST_REQUIRE(cs.GetReasonCode() == BPV6_CUSTODY_SIGNAL_REASON_CODES_7BIT::NO_ADDITIONAL_INFORMATION);
                BOOST_REQUIRE_EQUAL(cs.m_bundleSourceEid, PRIMARY_SRC_URI);
            }
        }
    }


    //bundle with custody bit set, hdtn acs disabled and refuses custody (still valid cteb bundle)
    {
        padded_vector_uint8_t bundleData(validCtebBundle);
        BundleViewV6 bv;
        //std::cout << "sz " << bundleSerializedCopy.size() << std::endl;

        BOOST_REQUIRE(bv.SwapInAndLoadBundle(bundleData));
        const Bpv6CbhePrimaryBlock originalPrimaryFromOriginator = bv.m_primaryBlockView.header;


        //hdtn node refuse custody without acs
        const bool isAcsAware = false;
        CustodyTransferManager ctmHdtn(isAcsAware, PRIMARY_HDTN_NODE, PRIMARY_HDTN_SVC);
        BOOST_REQUIRE_EQUAL(ctmHdtn.GetAcsConstRef(custodianOriginator, BPV6_ACS_STATUS_REASON_INDICES::FAIL__DEPLETED_STORAGE).m_custodyIdFills.size(), 0);
        BundleViewV6 custodySignalRfc5050RenderedBundleView;
        BOOST_REQUIRE(ctmHdtn.ProcessCustodyOfBundle(bv, false, newHdtnCtebCustodyId, BPV6_ACS_STATUS_REASON_INDICES::FAIL__DEPLETED_STORAGE,
            custodySignalRfc5050RenderedBundleView));
        BOOST_REQUIRE_EQUAL(ctmHdtn.GetAcsConstRef(custodianOriginator, BPV6_ACS_STATUS_REASON_INDICES::FAIL__DEPLETED_STORAGE).m_custodyIdFills.size(), 0); //acs not used (disabled)
        BOOST_REQUIRE_GT(custodySignalRfc5050RenderedBundleView.m_renderedBundle.size(), 0); //using 5050 custody signal

        //hdtn does not modify bundle for next hop because custody was refused

        //source node receives 5050 custody signal fail
        {
            BundleViewV6 bvSrc;
            //std::cout << "sz " << bundleSerializedCopy.size() << std::endl;
            BOOST_REQUIRE(bvSrc.SwapInAndLoadBundle(custodySignalRfc5050RenderedBundleView.m_frontBuffer));
            { //check primary
                Bpv6CbhePrimaryBlock & primary = bvSrc.m_primaryBlockView.header;
                const BPV6_BUNDLEFLAG requiredPrimaryFlags = BPV6_BUNDLEFLAG::SINGLETON | BPV6_BUNDLEFLAG::NOFRAGMENT | BPV6_BUNDLEFLAG::ADMINRECORD;
                BOOST_REQUIRE_EQUAL(primary.m_bundleProcessingControlFlags & requiredPrimaryFlags, requiredPrimaryFlags);
                BOOST_REQUIRE_EQUAL(primary.m_custodianEid, cbhe_eid_t(0, 0));
                BOOST_REQUIRE_EQUAL(primary.m_sourceNodeId, cbhe_eid_t(PRIMARY_HDTN_NODE, PRIMARY_HDTN_SVC));
                BOOST_REQUIRE_EQUAL(primary.m_destinationEid, cbhe_eid_t(PRIMARY_SRC_NODE, PRIMARY_SRC_SVC));

                BOOST_REQUIRE_EQUAL(custodySignalRfc5050RenderedBundleView.m_primaryBlockView.header.m_sourceNodeId, cbhe_eid_t(PRIMARY_HDTN_NODE, PRIMARY_HDTN_SVC));
                BOOST_REQUIRE_EQUAL(custodySignalRfc5050RenderedBundleView.m_primaryBlockView.header.m_destinationEid, cbhe_eid_t(PRIMARY_SRC_NODE, PRIMARY_SRC_SVC));
            }
            { //check admin record
                std::vector<BundleViewV6::Bpv6CanonicalBlockView*> blocks;
                bvSrc.GetCanonicalBlocksByType(BPV6_BLOCK_TYPE_CODE::PAYLOAD, blocks);
                BOOST_REQUIRE_EQUAL(blocks.size(), 1);
                Bpv6AdministrativeRecord* adminRecordBlockPtr = dynamic_cast<Bpv6AdministrativeRecord*>(blocks[0]->headerPtr.get());
                BOOST_REQUIRE(adminRecordBlockPtr);
                BOOST_REQUIRE_EQUAL(adminRecordBlockPtr->m_blockTypeCode, BPV6_BLOCK_TYPE_CODE::PAYLOAD);
                BOOST_REQUIRE_EQUAL(adminRecordBlockPtr->m_adminRecordTypeCode, BPV6_ADMINISTRATIVE_RECORD_TYPE_CODE::CUSTODY_SIGNAL);

                BOOST_REQUIRE_EQUAL(blocks[0]->actualSerializedBlockPtr.size(), adminRecordBlockPtr->GetSerializationSize());

                Bpv6AdministrativeRecordContentCustodySignal * csPtr = dynamic_cast<Bpv6AdministrativeRecordContentCustodySignal*>(adminRecordBlockPtr->m_adminRecordContentPtr.get());
                BOOST_REQUIRE(csPtr);
                Bpv6AdministrativeRecordContentCustodySignal & cs = *(reinterpret_cast<Bpv6AdministrativeRecordContentCustodySignal*>(csPtr));
                BOOST_REQUIRE(!cs.DidCustodyTransferSucceed());
                BOOST_REQUIRE(cs.GetReasonCode() == BPV6_CUSTODY_SIGNAL_REASON_CODES_7BIT::DEPLETED_STORAGE);
                BOOST_REQUIRE_EQUAL(cs.m_bundleSourceEid, PRIMARY_SRC_URI);
            }
        }
    }

}

BOOST_AUTO_TEST_CASE(DeletionStatusReportTestCase)
{
    // Create a bundle to-be-deleted (will have custody transfer)
    BundleViewV6 deletedBundle;
    const std::string bundleDataStr = "bundle data!!!";
    const cbhe_eid_t deletedReportToEid(REPORT_TO_NODE, REPORT_TO_SVC);
    const cbhe_eid_t deletedSrcEid(PRIMARY_SRC_NODE, PRIMARY_SRC_SVC);

    GenerateBundleForStatusReport(
        cbhe_eid_t(PRIMARY_SRC_NODE, PRIMARY_SRC_SVC),
        deletedReportToEid,
        bundleDataStr, deletedBundle);
    BOOST_REQUIRE_GT(deletedBundle.m_frontBuffer.size(), 0);

    {
        const Bpv6CbhePrimaryBlock & deletedPrimary = deletedBundle.m_primaryBlockView.header;

        BundleViewV6 bv;

        bv.m_frontBuffer.reserve(2000);
        bv.m_backBuffer.reserve(2000);

        const bool isAcsAware = false;
        CustodyTransferManager ctmHdtn(false, PRIMARY_HDTN_NODE, PRIMARY_HDTN_SVC);

        TimestampUtil::dtn_time_t timeBefore = TimestampUtil::GenerateDtnTimeNow();
        bool success = ctmHdtn.GenerateBundleDeletionStatusReport(deletedPrimary, bundleDataStr.size(), bv);
        TimestampUtil::dtn_time_t timeAfter = TimestampUtil::GenerateDtnTimeNow();

        BOOST_REQUIRE(success);
        BOOST_REQUIRE_GT(bv.m_renderedBundle.size(), 0);

        const Bpv6CbhePrimaryBlock primary = bv.m_primaryBlockView.header;
        const BPV6_BUNDLEFLAG requiredPrimaryFlags = BPV6_BUNDLEFLAG::SINGLETON | BPV6_BUNDLEFLAG::NOFRAGMENT | BPV6_BUNDLEFLAG::ADMINRECORD;
        BOOST_REQUIRE_EQUAL(primary.m_bundleProcessingControlFlags & requiredPrimaryFlags, requiredPrimaryFlags);

        BOOST_REQUIRE_EQUAL(primary.m_destinationEid, deletedReportToEid);
        BOOST_REQUIRE_EQUAL(primary.m_sourceNodeId, cbhe_eid_t(PRIMARY_HDTN_NODE, PRIMARY_HDTN_SVC));
        BOOST_REQUIRE_EQUAL(primary.m_custodianEid, cbhe_eid_t(0, 0));

        std::vector<BundleViewV6::Bpv6CanonicalBlockView*> blocks;

        bv.GetCanonicalBlocksByType(BPV6_BLOCK_TYPE_CODE::PAYLOAD, blocks);
        BOOST_REQUIRE_EQUAL(blocks.size(), 1);

        Bpv6AdministrativeRecord * adminRecordBlockPtr = dynamic_cast<Bpv6AdministrativeRecord*>(blocks[0]->headerPtr.get());
        BOOST_REQUIRE(adminRecordBlockPtr);
        if(adminRecordBlockPtr) {
            BOOST_REQUIRE_EQUAL(adminRecordBlockPtr->m_blockTypeCode, BPV6_BLOCK_TYPE_CODE::PAYLOAD);
            BOOST_REQUIRE_EQUAL(adminRecordBlockPtr->m_adminRecordTypeCode, BPV6_ADMINISTRATIVE_RECORD_TYPE_CODE::BUNDLE_STATUS_REPORT);
            BOOST_REQUIRE_EQUAL(blocks[0]->actualSerializedBlockPtr.size(), adminRecordBlockPtr->GetSerializationSize());

            Bpv6AdministrativeRecordContentBundleStatusReport * srPtr = dynamic_cast<Bpv6AdministrativeRecordContentBundleStatusReport*>(adminRecordBlockPtr->m_adminRecordContentPtr.get());
            BOOST_REQUIRE(srPtr);
            if(srPtr) {
                Bpv6AdministrativeRecordContentBundleStatusReport & sr = *(reinterpret_cast<Bpv6AdministrativeRecordContentBundleStatusReport*>(srPtr));

                BOOST_REQUIRE((timeBefore < sr.m_timeOfDeletionOfBundle) || (timeBefore == sr.m_timeOfDeletionOfBundle));
                BOOST_REQUIRE((sr.m_timeOfDeletionOfBundle < timeAfter) || (sr.m_timeOfDeletionOfBundle == timeAfter));

                BOOST_REQUIRE_EQUAL(sr.m_bundleSourceEid, PRIMARY_SRC_URI);
                BOOST_REQUIRE_EQUAL(sr.m_copyOfBundleCreationTimestamp, deletedPrimary.m_creationTimestamp);
                BOOST_REQUIRE(!sr.m_isFragment);
                BOOST_REQUIRE_EQUAL(sr.m_reasonCode, BPV6_BUNDLE_STATUS_REPORT_REASON_CODES::LIFETIME_EXPIRED);
                BOOST_REQUIRE_EQUAL(sr.m_statusFlags, BPV6_BUNDLE_STATUS_REPORT_STATUS_FLAGS::REPORTING_NODE_DELETED_BUNDLE);
            }
        }
    }
}


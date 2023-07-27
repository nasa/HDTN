/**
 * @file CustodyTransferManager.cpp
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

#include "codec/CustodyTransferManager.h"
#include <iostream>
#include "TimestampUtil.h"
#include "Uri.h"
#include <boost/make_unique.hpp>

static const bool INDEX_TO_IS_SUCCESS[NUM_ACS_STATUS_INDICES] = {
    true,
    false,
    false,
    false,
    false,
    false,
    false
};
static const BPV6_CUSTODY_SIGNAL_REASON_CODES_7BIT INDEX_TO_REASON_CODE[NUM_ACS_STATUS_INDICES] = {
    BPV6_CUSTODY_SIGNAL_REASON_CODES_7BIT::NO_ADDITIONAL_INFORMATION,
    BPV6_CUSTODY_SIGNAL_REASON_CODES_7BIT::REDUNDANT_RECEPTION,
    BPV6_CUSTODY_SIGNAL_REASON_CODES_7BIT::DEPLETED_STORAGE,
    BPV6_CUSTODY_SIGNAL_REASON_CODES_7BIT::DESTINATION_ENDPOINT_ID_UNINTELLIGIBLE,
    BPV6_CUSTODY_SIGNAL_REASON_CODES_7BIT::NO_KNOWN_ROUTE_TO_DESTINATION_FROM_HERE,
    BPV6_CUSTODY_SIGNAL_REASON_CODES_7BIT::NO_TIMELY_CONTACT_WITH_NEXT_NODE_ON_ROUTE,
    BPV6_CUSTODY_SIGNAL_REASON_CODES_7BIT::BLOCK_UNINTELLIGIBLE
};

void CustodyTransferManager::SetCreationAndSequence(uint64_t & creation, uint64_t & sequence) {
    creation = TimestampUtil::GetSecondsSinceEpochRfc5050();
    if (creation != m_lastCreation) {
        m_sequence = 0;
    }
    sequence = m_sequence++;
}

bool CustodyTransferManager::GenerateCustodySignalBundle(BundleViewV6 & newRenderedBundleView, const Bpv6CbhePrimaryBlock & primaryFromSender, uint64_t payloadLen, const BPV6_ACS_STATUS_REASON_INDICES statusReasonIndex) {
    Bpv6CbhePrimaryBlock & newPrimary = newRenderedBundleView.m_primaryBlockView.header;
    newPrimary.SetZero();
    

    newPrimary.m_bundleProcessingControlFlags = (primaryFromSender.m_bundleProcessingControlFlags & BPV6_BUNDLEFLAG::PRIORITY_BIT_MASK) |
        (BPV6_BUNDLEFLAG::SINGLETON | BPV6_BUNDLEFLAG::NOFRAGMENT | BPV6_BUNDLEFLAG::ADMINRECORD);
    newPrimary.m_sourceNodeId.Set(m_myCustodianNodeId, m_myCustodianServiceId);
    newPrimary.m_destinationEid = primaryFromSender.m_custodianEid;
    SetCreationAndSequence(newPrimary.m_creationTimestamp.secondsSinceStartOfYear2000, newPrimary.m_creationTimestamp.sequenceNumber);
    newPrimary.m_lifetimeSeconds = 1000; //todo
    newRenderedBundleView.m_primaryBlockView.SetManuallyModified();

    //add cs payload block
    {
        std::unique_ptr<Bpv6CanonicalBlock> blockPtr = boost::make_unique<Bpv6AdministrativeRecord>();
        Bpv6AdministrativeRecord & block = *(reinterpret_cast<Bpv6AdministrativeRecord*>(blockPtr.get()));

        //block.m_blockTypeCode = BPV7_BLOCK_TYPE_CODE::PAYLOAD; //not needed because handled by Bpv7AdministrativeRecord constructor
        block.m_blockProcessingControlFlags = BPV6_BLOCKFLAG::NO_FLAGS_SET;

        block.m_adminRecordTypeCode = BPV6_ADMINISTRATIVE_RECORD_TYPE_CODE::CUSTODY_SIGNAL;
        block.m_isFragment = primaryFromSender.HasFragmentationFlagSet();
        block.m_adminRecordContentPtr = boost::make_unique<Bpv6AdministrativeRecordContentCustodySignal>();

        Bpv6AdministrativeRecordContentCustodySignal & sig = *(reinterpret_cast<Bpv6AdministrativeRecordContentCustodySignal*>(block.m_adminRecordContentPtr.get()));
        sig.m_copyOfBundleCreationTimestamp = primaryFromSender.m_creationTimestamp;
        sig.m_bundleSourceEid = Uri::GetIpnUriString(primaryFromSender.m_sourceNodeId.nodeId, primaryFromSender.m_sourceNodeId.serviceId);
        //REQ D4.2.2.7 An ACS-aware bundle protocol agent shall utilize the ACS bundle timestamp
        //time as the Time of Signal when executing RFC 5050 section 6.3
        sig.SetTimeOfSignalGeneration(TimestampUtil::GenerateDtnTimeNow());//add custody
        const uint8_t sri = static_cast<uint8_t>(statusReasonIndex);
        sig.SetCustodyTransferStatusAndReason(INDEX_TO_IS_SUCCESS[sri], INDEX_TO_REASON_CODE[sri]);

        if(primaryFromSender.HasFragmentationFlagSet()) {
            sig.m_fragmentOffsetIfPresent = primaryFromSender.m_fragmentOffset;
            sig.m_fragmentLengthIfPresent = payloadLen;
        }

        newRenderedBundleView.AppendMoveCanonicalBlock(std::move(blockPtr));
    }
    if (!newRenderedBundleView.Render(CBHE_BPV6_MINIMUM_SAFE_PRIMARY_HEADER_ENCODE_SIZE + Bpv6AdministrativeRecordContentCustodySignal::CBHE_MAX_SERIALIZATION_SIZE)) { //todo size
        return false;
    }
    return true;

}
bool CustodyTransferManager::GenerateAllAcsBundlesAndClear(std::list<BundleViewV6> & newAcsRenderedBundleViewList) {
    newAcsRenderedBundleViewList.clear();
    m_largestNumberOfFills = 0;
    for (std::map<cbhe_eid_t, acs_array_t>::iterator it = m_mapCustodianToAcsArray.begin(); it != m_mapCustodianToAcsArray.end(); ++it) {
        const cbhe_eid_t & custodianEid = it->first;
        acs_array_t & acsArray = it->second;
        for (unsigned int statusReasonIndex = 0; statusReasonIndex < NUM_ACS_STATUS_INDICES; ++statusReasonIndex) {
            Bpv6AdministrativeRecordContentAggregateCustodySignal & currentAcsToMove = acsArray[statusReasonIndex];
            if (!currentAcsToMove.m_custodyIdFills.empty()) {
                newAcsRenderedBundleViewList.emplace_back();
                BundleViewV6 & bv = newAcsRenderedBundleViewList.back();
                if (GenerateAcsBundle(bv, custodianEid, currentAcsToMove)) {
                    currentAcsToMove.Reset();
                }
                else { //failure
                    return false;
                }
            }
        }
    }
    return true;
}
uint64_t CustodyTransferManager::GetLargestNumberOfFills() const {
    return m_largestNumberOfFills;
}
bool CustodyTransferManager::GenerateAcsBundle(BundleViewV6 & newAcsRenderedBundleView, const cbhe_eid_t & custodianEid, Bpv6AdministrativeRecordContentAggregateCustodySignal & acsToMove, const bool copyAcsOnly) {
    
    if (acsToMove.m_custodyIdFills.empty()) {
        return false;
    }
    Bpv6CbhePrimaryBlock & newPrimary = newAcsRenderedBundleView.m_primaryBlockView.header;
    newPrimary.SetZero();

    newPrimary.m_bundleProcessingControlFlags = //(primaryFromSender.m_bundleProcessingControlFlags & BPV6_BUNDLEFLAG::PRIORITY_BIT_MASK) |
        (BPV6_BUNDLEFLAG::SINGLETON | BPV6_BUNDLEFLAG::NOFRAGMENT | BPV6_BUNDLEFLAG::ADMINRECORD);
    newPrimary.m_sourceNodeId.Set(m_myCustodianNodeId, m_myCustodianServiceId);
    newPrimary.m_destinationEid = custodianEid;
    SetCreationAndSequence(newPrimary.m_creationTimestamp.secondsSinceStartOfYear2000, newPrimary.m_creationTimestamp.sequenceNumber);
    newPrimary.m_lifetimeSeconds = 1000; //todo
    newAcsRenderedBundleView.m_primaryBlockView.SetManuallyModified();

    //add acs payload block
    {
        std::unique_ptr<Bpv6CanonicalBlock> blockPtr = boost::make_unique<Bpv6AdministrativeRecord>();
        Bpv6AdministrativeRecord & block = *(reinterpret_cast<Bpv6AdministrativeRecord*>(blockPtr.get()));

        //block.m_blockTypeCode = BPV7_BLOCK_TYPE_CODE::PAYLOAD; //not needed because handled by Bpv7AdministrativeRecord constructor
        block.m_blockProcessingControlFlags = BPV6_BLOCKFLAG::NO_FLAGS_SET;

        block.m_adminRecordTypeCode = BPV6_ADMINISTRATIVE_RECORD_TYPE_CODE::AGGREGATE_CUSTODY_SIGNAL;
        if (copyAcsOnly) {
            block.m_adminRecordContentPtr = boost::make_unique<Bpv6AdministrativeRecordContentAggregateCustodySignal>(acsToMove);
        }
        else {
            block.m_adminRecordContentPtr = boost::make_unique<Bpv6AdministrativeRecordContentAggregateCustodySignal>(std::move(acsToMove));
        }
        newAcsRenderedBundleView.AppendMoveCanonicalBlock(std::move(blockPtr));
    }
    if (!newAcsRenderedBundleView.Render(CBHE_BPV6_MINIMUM_SAFE_PRIMARY_HEADER_ENCODE_SIZE + 2000)) { //todo size
        return false;
    }
    return true;
}
bool CustodyTransferManager::GenerateAcsBundle(BundleViewV6 & newAcsRenderedBundleView, const cbhe_eid_t & custodianEid, const BPV6_ACS_STATUS_REASON_INDICES statusReasonIndex, const bool copyAcsOnly) {
    //const cbhe_eid_t custodianEidFromPrimary(primaryFromSender.custodian_node, primaryFromSender.custodian_svc);
    std::map<cbhe_eid_t, acs_array_t>::iterator it = m_mapCustodianToAcsArray.find(custodianEid);
    if (it == m_mapCustodianToAcsArray.end()) {
        return false;
    }
    acs_array_t & acsArray = it->second;
    Bpv6AdministrativeRecordContentAggregateCustodySignal & currentAcs = acsArray[static_cast<uint8_t>(statusReasonIndex)];
    return GenerateAcsBundle(newAcsRenderedBundleView, custodianEid, currentAcs, copyAcsOnly);
}

acs_array_t::acs_array_t() {
    at(static_cast<uint8_t>(BPV6_ACS_STATUS_REASON_INDICES::SUCCESS__NO_ADDITIONAL_INFORMATION)).SetCustodyTransferStatusAndReason(
        true, BPV6_CUSTODY_SIGNAL_REASON_CODES_7BIT::NO_ADDITIONAL_INFORMATION);
    at(static_cast<uint8_t>(BPV6_ACS_STATUS_REASON_INDICES::FAIL__REDUNDANT_RECEPTION)).SetCustodyTransferStatusAndReason(
        false, BPV6_CUSTODY_SIGNAL_REASON_CODES_7BIT::REDUNDANT_RECEPTION);
    at(static_cast<uint8_t>(BPV6_ACS_STATUS_REASON_INDICES::FAIL__DEPLETED_STORAGE)).SetCustodyTransferStatusAndReason(
        false, BPV6_CUSTODY_SIGNAL_REASON_CODES_7BIT::DEPLETED_STORAGE);
    at(static_cast<uint8_t>(BPV6_ACS_STATUS_REASON_INDICES::FAIL__DESTINATION_ENDPOINT_ID_UNINTELLIGIBLE)).SetCustodyTransferStatusAndReason(
        false, BPV6_CUSTODY_SIGNAL_REASON_CODES_7BIT::DESTINATION_ENDPOINT_ID_UNINTELLIGIBLE);
    at(static_cast<uint8_t>(BPV6_ACS_STATUS_REASON_INDICES::FAIL__NO_KNOWN_ROUTE_TO_DESTINATION_FROM_HERE)).SetCustodyTransferStatusAndReason(
        false, BPV6_CUSTODY_SIGNAL_REASON_CODES_7BIT::NO_KNOWN_ROUTE_TO_DESTINATION_FROM_HERE);
    at(static_cast<uint8_t>(BPV6_ACS_STATUS_REASON_INDICES::FAIL__NO_TIMELY_CONTACT_WITH_NEXT_NODE_ON_ROUTE)).SetCustodyTransferStatusAndReason(
        false, BPV6_CUSTODY_SIGNAL_REASON_CODES_7BIT::NO_TIMELY_CONTACT_WITH_NEXT_NODE_ON_ROUTE);
    at(static_cast<uint8_t>(BPV6_ACS_STATUS_REASON_INDICES::FAIL__BLOCK_UNINTELLIGIBLE)).SetCustodyTransferStatusAndReason(
        false, BPV6_CUSTODY_SIGNAL_REASON_CODES_7BIT::BLOCK_UNINTELLIGIBLE);
}

CustodyTransferManager::CustodyTransferManager(const bool isAcsAware, const uint64_t myCustodianNodeId, const uint64_t myCustodianServiceId) : 
    m_isAcsAware(isAcsAware),
    m_myCustodianNodeId(myCustodianNodeId),
    m_myCustodianServiceId(myCustodianServiceId),
    m_myCtebCreatorCustodianEidString(Uri::GetIpnUriString(m_myCustodianNodeId, m_myCustodianServiceId)),
    m_lastCreation(0),
    m_sequence(0),
    m_largestNumberOfFills(0)
{
 
}
CustodyTransferManager::~CustodyTransferManager() {}



bool CustodyTransferManager::ProcessCustodyOfBundle(BundleViewV6 & bv, bool acceptCustody, const uint64_t custodyId,
    const BPV6_ACS_STATUS_REASON_INDICES statusReasonIndex, BundleViewV6 & custodySignalRfc5050RenderedBundleView)
{

    CustodyTransferContext prevCustodyInfo;
    if(!GetCustodyInfo(bv, prevCustodyInfo)) {
        return false;
    }
    if(!UpdateBundleCustodyFields(bv, acceptCustody, custodyId)) {
        return false;
    }

    return GenerateCustodySignal(prevCustodyInfo, acceptCustody, statusReasonIndex, custodySignalRfc5050RenderedBundleView);

}

bool CustodyTransferManager::GetCustodyInfo(BundleViewV6 & bv, struct CustodyTransferContext &prevCustodyInfo)
{
    Bpv6CbhePrimaryBlock & primary = bv.m_primaryBlockView.header;
    const cbhe_eid_t custodianEidFromPrimary(primary.m_custodianEid);

    prevCustodyInfo.primary = primary;
    if(!bv.GetPayloadSize(prevCustodyInfo.payloadLen)) {
        return false;
    }

    if (m_isAcsAware) {
        prevCustodyInfo.validCtebPresent = false;
        std::vector<BundleViewV6::Bpv6CanonicalBlockView*> blocks;
        Bpv6CustodyTransferEnhancementBlock* ctebBlockPtr;
        bv.GetCanonicalBlocksByType(BPV6_BLOCK_TYPE_CODE::CUSTODY_TRANSFER_ENHANCEMENT, blocks);
        if (blocks.size() > 1) { //D3.3.3 There shall be only one CTEB per bundle. 
            return false; //treat as malformed
        }
        else if (blocks.size() == 1) { //cteb present
            ctebBlockPtr = dynamic_cast<Bpv6CustodyTransferEnhancementBlock*>(blocks[0]->headerPtr.get());
            if (ctebBlockPtr == NULL) {
                return false;
            }
            
            uint64_t ctebNodeNumber;
            uint64_t ctebServiceNumber;
            if (!Uri::ParseIpnUriString(ctebBlockPtr->m_ctebCreatorCustodianEidString, ctebNodeNumber, ctebServiceNumber)) {
                return false;
            }
            cbhe_eid_t custodianEidFromCteb(ctebNodeNumber, ctebServiceNumber);
            //d) For an intermediate node which is ACS capable and accepts custody, the bundle
            //protocol agent compares the CTEB custodian with the primary bundle block
            //custodian.
            if (custodianEidFromPrimary == custodianEidFromCteb) {
                prevCustodyInfo.validCtebPresent = true;
                prevCustodyInfo.receivedCtebCustodyId = ctebBlockPtr->m_custodyId;
            }
        }
    }
    return true;
}

bool CustodyTransferManager::UpdateBundleCustodyFields(BundleViewV6 & bv, bool acceptCustody, const uint64_t custodyId)
{
    Bpv6CbhePrimaryBlock & primary = bv.m_primaryBlockView.header;
    //Bpv6CbhePrimaryBlock originalPrimaryFromSender = primary; //make a copy
    const cbhe_eid_t custodianEidFromPrimary(primary.m_custodianEid);

    if (m_isAcsAware) {
        std::vector<BundleViewV6::Bpv6CanonicalBlockView*> blocks;
        Bpv6CustodyTransferEnhancementBlock* ctebBlockPtr;
        bv.GetCanonicalBlocksByType(BPV6_BLOCK_TYPE_CODE::CUSTODY_TRANSFER_ENHANCEMENT, blocks);
        if (blocks.size() > 1) { //D3.3.3 There shall be only one CTEB per bundle. 
            return false; //treat as malformed
        }
        else if (blocks.size() == 1) { //cteb present
            ctebBlockPtr = dynamic_cast<Bpv6CustodyTransferEnhancementBlock*>(blocks[0]->headerPtr.get());
            if (ctebBlockPtr == NULL) {
                return false;
            }
            
            uint64_t ctebNodeNumber;
            uint64_t ctebServiceNumber;
            if (!Uri::ParseIpnUriString(ctebBlockPtr->m_ctebCreatorCustodianEidString, ctebNodeNumber, ctebServiceNumber)) {
                return false;
            }
            cbhe_eid_t custodianEidFromCteb(ctebNodeNumber, ctebServiceNumber);
            //d) For an intermediate node which is ACS capable and accepts custody, the bundle
            //protocol agent compares the CTEB custodian with the primary bundle block
            //custodian.
            if (custodianEidFromPrimary == custodianEidFromCteb) {
                // Leave cteb in place TODO why?
            }
            else {
                //If they are different, the CTEB is invalid and deleted.
                blocks[0]->markedForDeletion = true; //deleted when/if re-rendered
                //note.. unmark for deletion if going to reuse it if accept custody (below)
            }
            
        }

        if(!acceptCustody) {
            return true;
        }
        //REQUIREMENT D4.2.2.3b For ACS-aware bundle protocol agents which do accept custody of ACS: (regardless of valid or invalid cteb)
        //  b) the bundle protocol agent shall update the custodian of the Primary Bundle Block and the CTEB as identified in D3.3;

        //update primary bundle block (pbb) with new custodian (do this after creating a custody signal to avoid copying the primary)
        primary.m_custodianEid.Set(m_myCustodianNodeId, m_myCustodianServiceId);
        bv.m_primaryBlockView.SetManuallyModified(); //will update after render


        if (blocks.size() == 1) { //cteb present
            //update (reuse existing) CTEB with new custodian
            blocks[0]->markedForDeletion = false;
            ctebBlockPtr->m_custodyId = custodyId; //ctebBlockPtr asserted to be non-null from above
            ctebBlockPtr->m_ctebCreatorCustodianEidString = m_myCtebCreatorCustodianEidString;
            blocks[0]->SetManuallyModified(); //bundle needs rerendered
        }
        else { //non-existing cteb present.. append a new one to the bundle
            //https://cwe.ccsds.org/sis/docs/SIS-DTN/Meeting%20Materials/2011/Fall%20(Colorado)/jenkins-sisdtn-aggregate-custody-signals.pdf
            //slide 20 - ACS-enabled nodes add CTEBs when they become custodian.
            std::unique_ptr<Bpv6CanonicalBlock> blockPtr = boost::make_unique<Bpv6CustodyTransferEnhancementBlock>();
            Bpv6CustodyTransferEnhancementBlock & block = *(reinterpret_cast<Bpv6CustodyTransferEnhancementBlock*>(blockPtr.get()));
            //block.SetZero();

            block.m_blockProcessingControlFlags = BPV6_BLOCKFLAG::NO_FLAGS_SET;
            block.m_custodyId = custodyId;
            block.m_ctebCreatorCustodianEidString = m_myCtebCreatorCustodianEidString;
            bv.AppendMoveCanonicalBlock(std::move(blockPtr)); //bundle needs rerendered
        }

    }
    else { //not acs aware
        if(!acceptCustody) {
            return true;
        }
        primary.m_custodianEid.Set(m_myCustodianNodeId, m_myCustodianServiceId);
        bv.m_primaryBlockView.SetManuallyModified(); //will update after render
    }
    return true;
}

bool CustodyTransferManager::GenerateCustodySignal(CustodyTransferContext &info, bool acceptCustody,
    const BPV6_ACS_STATUS_REASON_INDICES statusReasonIndex, BundleViewV6 & custodySignalRfc5050RenderedBundleView)
{
    custodySignalRfc5050RenderedBundleView.Reset();
    const cbhe_eid_t custodianEidFromPrimary(info.primary.m_custodianEid);

    if (m_isAcsAware) {
        if (acceptCustody) {
            if (info.validCtebPresent) { //identical custodians
                //acs capable ba, ba accepts custody, valid cteb => pending succeeded acs for custodian

                //d continued) For an intermediate node which is ACS capable and accepts custody...
                //For identical custodians, the primary bundle block and CTEB are updated with the new custodian
                //by the bundle protocol agent, and custody aggregation is utilized to improve link
                //efficiency
                //Item d) bounds the defining set of capabilities unique to ACS. By accepting a bundle for
                //custody transfer, an ACS - capable bundle protocol agent will process the bundle per RFC
                //5050, section 5.10.1.However, instead of the normal custody signaling, the CTEB identifies
                //in shortened form the specific bundle by custody ID.

                //REQUIREMENT D4.2.2.3c For ACS-aware bundle protocol agents which do accept custody of ACS:
                //  c) for bundles with a valid CTEB: 
                //      1) the bundle protocol agent shall aggregate Succeeded status into a single bundle
                //      as identified in D3.2:
                //          i) the aggregation of Succeeded status shall not exceed maximum allowed
                //          bundle size;
                //          ii) the time period for aggregation of bundle status shall not exceed the
                //          maximum allowed;
                //      2) the bundle protocol agent shall delete, upon successful transmission of an ACS
                //      signal, the associated timer and pending ACS Succeeded.

                //aggregate succeeded status
                acs_array_t & acsArray = m_mapCustodianToAcsArray[custodianEidFromPrimary];
                m_largestNumberOfFills = std::max(acsArray[static_cast<uint8_t>(BPV6_ACS_STATUS_REASON_INDICES::SUCCESS__NO_ADDITIONAL_INFORMATION)].AddCustodyIdToFill(info.receivedCtebCustodyId), m_largestNumberOfFills);
            }
            else { //invalid cteb
                //acs capable ba, ba accepts custody, invalid cteb => generate succeeded and follow 5.10
                //invalid cteb was deleted above
                if (!GenerateCustodySignalBundle(custodySignalRfc5050RenderedBundleView, info.primary, info.payloadLen, BPV6_ACS_STATUS_REASON_INDICES::SUCCESS__NO_ADDITIONAL_INFORMATION)) {
                    return false;
                }
            }

            
        }
        else { //acs aware and don't accept custody
            //c) For an intermediate node which is ACS capable and does not accept custody,
            //possibility b) is the mode of operation.A node may not accept a bundle for any of a
            //number of reasons as defined in RFC 5050.

            
            //REQ D4.2.2.2 For ACS-aware bundle protocol agents which do not accept custody of ACS:
            if (info.validCtebPresent) { //identical custodians
                //acs capable ba, ba refuses custody, valid cteb => pending failed acs for custodian

                //b) for bundles with a valid CTEB:
                //  1) the bundle protocol agent shall aggregate Failed status into a single bundle as identified in D3.2:
                //      i) the aggregation of Failed status shall not exceed the maximum allowed bundle size;
                //      ii) the time period for aggregation of bundle status shall not exceed the maximum allowed;
                //  2) the bundle protocol agent shall transmit an ACS as identified in RFC 5050 section 5.10;
                //  3) the bundle protocol agent shall delete, upon successful transmission of an ACS
                //  signal, the associated timer and pending ACS Failed.

                //aggregate failed status
                acs_array_t & acsArray = m_mapCustodianToAcsArray[custodianEidFromPrimary];
                m_largestNumberOfFills = std::max(acsArray[static_cast<uint8_t>(statusReasonIndex)].AddCustodyIdToFill(info.receivedCtebCustodyId), m_largestNumberOfFills);
            }
            else { //invalid cteb
                //acs capable ba, ba refuses custody, invalid cteb => generate failed and follow 5.10

                //a) for bundles without a valid CTEB block as identified in RFC 5050 section 5.10, the
                //  bundle protocol agent shall generate a Failed status;
                if (!GenerateCustodySignalBundle(custodySignalRfc5050RenderedBundleView, info.primary, info.payloadLen, statusReasonIndex)) {
                    return false;
                }
            }
            

        }
    }
    else { //not acs aware
        //Req D4.2.2.1 Non-ACS-aware bundle protocol agents shall process ACS-supporting bundles per RFC 5050 section 5.10.
        if (acceptCustody) {
            //a) For an intermediate node which is not ACS capable and accepts custody, the bundle
            //protocol agent ignores the CTEB and updates the custodian field in the primary
            //bundle block.Since the CTEB custodian is not updated, the CTEB is invalid, and the
            //next ACS - capable bundle protocol agent will delete the CTEB.

            //acs unsupported ba, ba accepts custody => update pbb with custodian and generate succeeded and follow 5.10
                //invalid cteb was deleted above
            if (!GenerateCustodySignalBundle(custodySignalRfc5050RenderedBundleView, info.primary, info.payloadLen, BPV6_ACS_STATUS_REASON_INDICES::SUCCESS__NO_ADDITIONAL_INFORMATION)) {
                return false;
            }
        }
        else {
            //b) For an intermediate node which is not ACS capable and does not accept custody, the
            //bundle protocol agent forwards the bundle without change.The CTEB is not
            //recognized.

            //acs unsupported ba, ba refuses custody => generate failed and follow 5.10

            if (!GenerateCustodySignalBundle(custodySignalRfc5050RenderedBundleView, info.primary, info.payloadLen, statusReasonIndex)) {
                return false;
            }
        }
    }
    return true;
}

const Bpv6AdministrativeRecordContentAggregateCustodySignal & CustodyTransferManager::GetAcsConstRef(const cbhe_eid_t & custodianEid, const BPV6_ACS_STATUS_REASON_INDICES statusReasonIndex) {
    return m_mapCustodianToAcsArray[custodianEid][static_cast<uint8_t>(statusReasonIndex)];
}

bool CustodyTransferManager::GenerateBundleDeletionStatusReport(const Bpv6CbhePrimaryBlock & primaryOfDeleted, uint64_t payloadLen, BundleViewV6 & bv) {

    // Get values needed for report from orginal bundle primary block

    const BPV6_BUNDLEFLAG priority = primaryOfDeleted.m_bundleProcessingControlFlags & BPV6_BUNDLEFLAG::PRIORITY_BIT_MASK;
    const cbhe_eid_t & reportToEid = primaryOfDeleted.m_reportToEid;
    const cbhe_eid_t & sourceEid = primaryOfDeleted.m_sourceNodeId;
    const bool isFragment = static_cast<bool>(primaryOfDeleted.m_bundleProcessingControlFlags & BPV6_BUNDLEFLAG::ISFRAGMENT);
    uint64_t fragmentOffset = 0, fragmentLength = 0;
    if(isFragment) {
        fragmentOffset = primaryOfDeleted.m_fragmentOffset;
        fragmentLength = payloadLen;
    }
    TimestampUtil::bpv6_creation_timestamp_t copyOfCreationTime = primaryOfDeleted.m_creationTimestamp;

    // Make sure there's no stale data here
    bv.Reset();

    // Set up primary block

    Bpv6CbhePrimaryBlock & primary = bv.m_primaryBlockView.header;
    primary.SetZero();

    primary.m_bundleProcessingControlFlags = (
        priority | BPV6_BUNDLEFLAG::SINGLETON  | BPV6_BUNDLEFLAG::ADMINRECORD | BPV6_BUNDLEFLAG::NOFRAGMENT);

    primary.m_sourceNodeId.Set(m_myCustodianNodeId, m_myCustodianServiceId);
    primary.m_destinationEid = reportToEid;

    SetCreationAndSequence(primary.m_creationTimestamp.secondsSinceStartOfYear2000, primary.m_creationTimestamp.sequenceNumber);

    primary.m_lifetimeSeconds = 1000; //todo; same as custody signals

    // We've modified the header
    bv.m_primaryBlockView.SetManuallyModified();

    // Set up Status Report

    std::unique_ptr<Bpv6CanonicalBlock> blockPtr = boost::make_unique<Bpv6AdministrativeRecord>();
    Bpv6AdministrativeRecord & block = *(reinterpret_cast<Bpv6AdministrativeRecord*>(blockPtr.get()));
    block.SetZero();

    block.m_blockProcessingControlFlags = BPV6_BLOCKFLAG::NO_FLAGS_SET;
    block.m_adminRecordTypeCode = BPV6_ADMINISTRATIVE_RECORD_TYPE_CODE::BUNDLE_STATUS_REPORT;
    block.m_isFragment = isFragment;

    block.m_adminRecordContentPtr = boost::make_unique<Bpv6AdministrativeRecordContentBundleStatusReport>();
    Bpv6AdministrativeRecordContentBundleStatusReport & report = *(reinterpret_cast<Bpv6AdministrativeRecordContentBundleStatusReport*>(block.m_adminRecordContentPtr.get()));
    report.Reset();

    TimestampUtil::dtn_time_t deletionTime = TimestampUtil::GenerateDtnTimeNow();
    report.SetTimeOfDeletionOfBundleAndStatusFlag(deletionTime);

    report.m_reasonCode = BPV6_BUNDLE_STATUS_REPORT_REASON_CODES::LIFETIME_EXPIRED;

    if(isFragment) {
        report.m_isFragment = true;
        report.m_fragmentOffsetIfPresent = fragmentOffset;
        report.m_fragmentLengthIfPresent = fragmentLength;
    }

    report.m_copyOfBundleCreationTimestamp = copyOfCreationTime;
    report.m_bundleSourceEid = Uri::GetIpnUriString(sourceEid.nodeId, sourceEid.serviceId);

    bv.AppendMoveCanonicalBlock(std::move(blockPtr));
    bool success = bv.Render(CBHE_BPV6_MINIMUM_SAFE_PRIMARY_HEADER_ENCODE_SIZE + Bpv6AdministrativeRecordContentBundleStatusReport::CBHE_MAX_SERIALIZATION_SIZE + 100);

    return success;
}

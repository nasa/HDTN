/**
 * @file BpSecPolicyManager.cpp
 * @author  Nadia Kortas <nadia.kortas@nasa.gov>
 * @author  Brian Tomko <brian.j.tomko@nasa.gov>
 *
 * @copyright Copyright  2021 United States Government as represented by
 * the National Aeronautics and Space Administration.
 * No copyright is claimed in the United States under Title 17, U.S.Code.
 * All Other Rights Reserved.
 *
 * @section LICENSE
 * Released under the NASA Open Source Agreement (NOSA)
 * See LICENSE.md in the source root directory for more information.
 */

#include "BpSecPolicyManager.h"
#include "BPSecManager.h"
#include <boost/make_unique.hpp>
#include "Uri.h"
#include "Logger.h"

static constexpr hdtn::Logger::SubProcess subprocess = hdtn::Logger::SubProcess::none;

bool BpSecPolicy::ValidateAndFinalize() {
    m_bcbTargetsPayloadBlock = false;
    m_bibMustBeEncrypted = false;

    if (m_doConfidentiality) {
        for (FragmentSet::data_fragment_set_t::const_iterator it = m_bcbBlockTypeTargets.cbegin();
            it != m_bcbBlockTypeTargets.cend(); ++it)
        {
            const FragmentSet::data_fragment_t& df = *it;
            for (uint64_t blockType = df.beginIndex; blockType <= df.endIndex; ++blockType) {
                if (blockType == static_cast<uint64_t>(BPV7_BLOCK_TYPE_CODE::PAYLOAD)) {
                    m_bcbTargetsPayloadBlock = true;
                    break;
                }
            }
        }
    }

    if (m_doIntegrity && m_doConfidentiality) {
        //When adding a BCB to a bundle, if some (or all) of the security
        //targets of the BCB match all of the security targets of an
        //existing BIB, then the existing BIB MUST also be encrypted.
        m_bibMustBeEncrypted = FragmentSet::FragmentSetsHaveOverlap(m_bcbBlockTypeTargets, m_bibBlockTypeTargets);
        if (m_bibMustBeEncrypted) {
            const bool bcbAlreadyTargetsBib = FragmentSet::ContainsFragmentEntirely(m_bcbBlockTypeTargets,
                FragmentSet::data_fragment_t(static_cast<std::size_t>(BPV7_BLOCK_TYPE_CODE::INTEGRITY),
                    static_cast<std::size_t>(BPV7_BLOCK_TYPE_CODE::INTEGRITY))); //just payload block
            if (bcbAlreadyTargetsBib) {
                LOG_DEBUG(subprocess) << "bpsec shall encrypt BIB since the BIB shares target(s) with the BCB";
            }
            else {
                LOG_FATAL(subprocess) << "bpsec policy must be fixed to encrypt the BIB since the BIB shares target(s) with the BCB";
                return false;
            }
        }
    }
    return true;
}

BpSecPolicyProcessingContext::BpSecPolicyProcessingContext() :
    m_ivStruct(InitializationVectorsForOneThread::Create()),
    m_bcbTargetBibBlockNumberPlaceholderIndex(UINT64_MAX) {}

static BpSecPolicyFilter* Internal_AddPolicyFilterToThisFilter(const std::string& eidUri, BpSecPolicyFilter& thisPolicyFilter) {
    static const std::string anyUriString("ipn:*.*");
    BpSecPolicyFilter* newFilterPtr;

    if (eidUri == anyUriString) {
        if (!thisPolicyFilter.m_anyEidToNextFilterPtr) {
            thisPolicyFilter.m_anyEidToNextFilterPtr = boost::make_unique<BpSecPolicyFilter>();
        }
        newFilterPtr = thisPolicyFilter.m_anyEidToNextFilterPtr.get();
    }
    else {
        cbhe_eid_t destEid;
        bool serviceNumberIsWildCard;
        if (!Uri::ParseIpnUriString(eidUri, destEid.nodeId, destEid.serviceId, &serviceNumberIsWildCard)) {
            LOG_ERROR(subprocess) << "BpSecPolicyManager: eidUri " << eidUri << " is invalid.";
            return NULL;
        }
        if (serviceNumberIsWildCard) {
            newFilterPtr = &thisPolicyFilter.m_nodeIdToNextFilterMap[destEid.nodeId];
        }
        else { //fully qualified eid
            newFilterPtr = &thisPolicyFilter.m_eidToNextFilterMap[destEid];
        }
    }
    return newFilterPtr;
}
BpSecPolicy* BpSecPolicyManager::CreateAndGetNewPolicy(const std::string& securitySourceEidUri,
    const std::string& bundleSourceEidUri, const std::string& bundleFinalDestEidUri, const BPSEC_ROLE role)
{
    BpSecPolicyFilter* policyFilterBundleSourcePtr = Internal_AddPolicyFilterToThisFilter(securitySourceEidUri, m_policyFilterSecuritySource);
    if (!policyFilterBundleSourcePtr) {
        return NULL;
    }
    BpSecPolicyFilter* policyFilterBundleFinalDestPtr = Internal_AddPolicyFilterToThisFilter(bundleSourceEidUri, *policyFilterBundleSourcePtr);
    if (!policyFilterBundleFinalDestPtr) {
        return NULL;
    }
    BpSecPolicyFilter* policyFilterRollArraysPtr = Internal_AddPolicyFilterToThisFilter(bundleFinalDestEidUri, *policyFilterBundleFinalDestPtr);
    if (!policyFilterRollArraysPtr) {
        return NULL;
    }
    const std::size_t roleIndex = static_cast<std::size_t>(role);
    if (roleIndex >= static_cast<std::size_t>(BPSEC_ROLE::RESERVED_MAX_ROLE_TYPES)) {
        return NULL;
    }
    BpSecPolicySharedPtr& policyPtr = policyFilterRollArraysPtr->m_policiesByRollArray[roleIndex];
    if (policyPtr) {
        return NULL; //already exists
    }
    policyPtr = std::make_shared<BpSecPolicy>();
    return policyPtr.get();
}
/*Policy rule lookup:
When a bundle is being received or generated, a policy rule must be found via the policy lookup function.

Lookup shall be performed by a Cascading lookup order:
        - The fully qualified [node,service] pair is looked up first for a match.
        - The node number only is looked up second for a match (for wildcard service numbers such as "ipn:2.*").
        - The "any destination flag" is looked up third for a match (for wildcard all such as "ipn:*.*").

The function shall take the following parameters:

1.) Security source:
    - "acceptor" or "verifier" role:
        - When a bundle is received, this field is the security source field of the ASB.
    - "source role":
        - When a bundle is received (i.e. being forwarded), this field is this receiving node's node number
        - When a new bundle is being created, this field is this bundle originator's node number
2.) Bundle source: The bundle source field of the primary block.
3.) Bundle final destination: The bundle destination field of the primary block.

"acceptor" or "verifier" should filter by security source field of the ASB
"source creators" should filter by their own node number
"source forwarders" should filter by their own node number and optionally bundle primary source and bundle primary dest

*/
static const BpSecPolicyFilter* Internal_GetPolicyFilterFromThisFilter(const cbhe_eid_t& eid, const BpSecPolicyFilter& thisPolicyFilter) {
    { //The fully qualified [node,service] pair is looked up first for a match.
        BpSecPolicyFilter::map_eid_to_next_filter_t::const_iterator it = thisPolicyFilter.m_eidToNextFilterMap.find(eid);
        if (it != thisPolicyFilter.m_eidToNextFilterMap.cend()) {
            return &(it->second);
        }
    }
    { //The node number only is looked up second for a match (for wildcard service numbers such as "ipn:2.*").
        BpSecPolicyFilter::map_node_id_to_next_filter_t::const_iterator it = thisPolicyFilter.m_nodeIdToNextFilterMap.find(eid.nodeId);
        if (it != thisPolicyFilter.m_nodeIdToNextFilterMap.cend()) {
            return &(it->second);
        }
    }
    //The "any destination flag" is looked up third for a match (for wildcard all such as "ipn:*.*").
    return thisPolicyFilter.m_anyEidToNextFilterPtr.get();

}
const BpSecPolicy* BpSecPolicyManager::FindPolicy(const cbhe_eid_t& securitySourceEid,
    const cbhe_eid_t& bundleSourceEid, const cbhe_eid_t& bundleFinalDestEid, const BPSEC_ROLE role) const
{
    const BpSecPolicyFilter* policyFilterBundleSourcePtr = Internal_GetPolicyFilterFromThisFilter(securitySourceEid, m_policyFilterSecuritySource);
    if (!policyFilterBundleSourcePtr) {
        return NULL;
    }
    const BpSecPolicyFilter* policyFilterBundleFinalDestPtr = Internal_GetPolicyFilterFromThisFilter(bundleSourceEid, *policyFilterBundleSourcePtr);
    if (!policyFilterBundleFinalDestPtr) {
        return NULL;
    }
    const BpSecPolicyFilter* policyFilterRollArraysPtr = Internal_GetPolicyFilterFromThisFilter(bundleFinalDestEid, *policyFilterBundleFinalDestPtr);
    if (!policyFilterRollArraysPtr) {
        return NULL;
    }
    const std::size_t roleIndex = static_cast<std::size_t>(role);
    if (roleIndex >= static_cast<std::size_t>(BPSEC_ROLE::RESERVED_MAX_ROLE_TYPES)) {
        return NULL;
    }
    return policyFilterRollArraysPtr->m_policiesByRollArray[roleIndex].get();
}

const BpSecPolicy* BpSecPolicyManager::FindPolicyWithCacheSupport(const cbhe_eid_t& securitySourceEid,
    const cbhe_eid_t& bundleSourceEid, const cbhe_eid_t& bundleFinalDestEid, const BPSEC_ROLE role, PolicySearchCache& searchCache) const
{
    searchCache.wasCacheHit = false;
    if ((role == searchCache.role)
        && (securitySourceEid == searchCache.securitySourceEid)
        && (bundleSourceEid == searchCache.bundleSourceEid)
        && (bundleFinalDestEid == searchCache.bundleFinalDestEid))
    {
        if (searchCache.foundPolicy) { //looked this up last time and succeeded
            searchCache.wasCacheHit = true;
            return searchCache.foundPolicy;
        }
        else { //attempted to look this up last time and failed
            return NULL;
        }
    }
    //never tried to look this up last time, look it up and cache the [failed or succeeded] result
    searchCache.foundPolicy = BpSecPolicyManager::FindPolicy(securitySourceEid,
        bundleSourceEid, bundleFinalDestEid, role);
    searchCache.role = role;
    searchCache.securitySourceEid = securitySourceEid;
    searchCache.bundleSourceEid = bundleSourceEid;
    searchCache.bundleFinalDestEid = bundleFinalDestEid;
    return searchCache.foundPolicy;
}

bool BpSecPolicyManager::ProcessReceivedBundle(BundleViewV7& bv, BpSecPolicyProcessingContext& ctx) const {
    const Bpv7CbhePrimaryBlock& primary = bv.m_primaryBlockView.header;
    bv.GetCanonicalBlocksByType(BPV7_BLOCK_TYPE_CODE::CONFIDENTIALITY, ctx.m_tmpBlocks);
    for (std::size_t i = 0; i < ctx.m_tmpBlocks.size(); ++i) {
        BundleViewV7::Bpv7CanonicalBlockView& bcbBlockView = *(ctx.m_tmpBlocks[i]);
        Bpv7BlockConfidentialityBlock* bcbPtr = dynamic_cast<Bpv7BlockConfidentialityBlock*>(bcbBlockView.headerPtr.get());
        if (!bcbPtr) {
            LOG_ERROR(subprocess) << "cast to bcb block failed";
            return false;
        }
        const BpSecPolicy* bpSecPolicyPtr = FindPolicyWithCacheSupport(
            bcbPtr->m_securitySource, primary.m_sourceNodeId, primary.m_destinationEid, BPSEC_ROLE::ACCEPTOR, ctx.m_searchCacheBcbAcceptor);
        bool verifyOnly;
        if (bpSecPolicyPtr) {
            verifyOnly = false; //false for acceptors
        }
        else {
            bpSecPolicyPtr = FindPolicyWithCacheSupport(
                bcbPtr->m_securitySource, primary.m_sourceNodeId, primary.m_destinationEid, BPSEC_ROLE::VERIFIER, ctx.m_searchCacheBcbVerifier);
            if (!bpSecPolicyPtr) {
                continue;
            }
            verifyOnly = true; //true for verifiers
        }
        if (!bpSecPolicyPtr->m_doConfidentiality) {
            continue;
        }

        //does not rerender in place here, there are more ops to complete after decryption and then a manual render-in-place will be called later
        if (!BPSecManager::TryDecryptBundleByIndividualBcb(ctx.m_evpCtxWrapper,
            ctx.m_ctxWrapperKeyWrapOps,
            bv,
            bcbBlockView,
            bpSecPolicyPtr->m_confidentialityKeyEncryptionKey.empty() ? NULL : bpSecPolicyPtr->m_confidentialityKeyEncryptionKey.data(), //NULL if not present (for wrapping DEK only)
            static_cast<unsigned int>(bpSecPolicyPtr->m_confidentialityKeyEncryptionKey.size()),
            bpSecPolicyPtr->m_dataEncryptionKey.empty() ? NULL : bpSecPolicyPtr->m_dataEncryptionKey.data(), //NULL if not present (when no wrapped key is present)
            static_cast<unsigned int>(bpSecPolicyPtr->m_dataEncryptionKey.size()),
            ctx.m_bpsecReusableElementsInternal,
            verifyOnly))
        {
            LOG_ERROR(subprocess) << "Process: version 7 bundle received but cannot decrypt";
            return false;
        }
        //all decryption ops successful for this bcb block at this point
        if (verifyOnly) {
            static thread_local bool printedMsg = false;
            if (!printedMsg) {
                LOG_INFO(subprocess) << "first time VERIFIED THE DECRYPTION of a bundle successfully from source node "
                    << bv.m_primaryBlockView.header.m_sourceNodeId
                    << " ..(This message type will now be suppressed.)";
                printedMsg = true;
            }
        }
        else {
            static thread_local bool printedMsg = false;
            if (!printedMsg) {
                LOG_INFO(subprocess) << "first time ACCEPTED/DECRYPTED a bundle successfully from source node "
                    << bv.m_primaryBlockView.header.m_sourceNodeId
                    << " ..(This message type will now be suppressed.)";
                printedMsg = true;
            }
        }
    }

    

    
    bv.GetCanonicalBlocksByType(BPV7_BLOCK_TYPE_CODE::INTEGRITY, ctx.m_tmpBlocks);
    for (std::size_t i = 0; i < ctx.m_tmpBlocks.size(); ++i) {
        BundleViewV7::Bpv7CanonicalBlockView& bibBlockView = *(ctx.m_tmpBlocks[i]);
        Bpv7BlockIntegrityBlock* bibPtr = dynamic_cast<Bpv7BlockIntegrityBlock*>(bibBlockView.headerPtr.get());
        if (!bibPtr) {
            LOG_ERROR(subprocess) << "cast to bib block failed";
            return false;
        }
        const BpSecPolicy* bpSecPolicyPtr = FindPolicyWithCacheSupport(
            bibPtr->m_securitySource, primary.m_sourceNodeId, primary.m_destinationEid, BPSEC_ROLE::ACCEPTOR, ctx.m_searchCacheBibAcceptor);
        bool markBibForDeletion;
        if (bpSecPolicyPtr) {
            markBibForDeletion = true; //true for acceptors
        }
        else {
            bpSecPolicyPtr = FindPolicyWithCacheSupport(
                bibPtr->m_securitySource, primary.m_sourceNodeId, primary.m_destinationEid, BPSEC_ROLE::VERIFIER, ctx.m_searchCacheBibVerifier);
            if (!bpSecPolicyPtr) {
                continue;
            }
            markBibForDeletion = false; //false for verifiers
        }

        if (!bpSecPolicyPtr->m_doIntegrity) {
            continue;
        }

        //does not rerender in place here, there are more ops to complete after decryption and then a manual render-in-place will be called later
        if (!BPSecManager::TryVerifyBundleIntegrityByIndividualBib(ctx.m_hmacCtxWrapper,
            ctx.m_ctxWrapperKeyWrapOps,
            bv,
            bibBlockView,
            bpSecPolicyPtr->m_hmacKeyEncryptionKey.empty() ? NULL : bpSecPolicyPtr->m_hmacKeyEncryptionKey.data(), //NULL if not present (for unwrapping hmac key only)
            static_cast<unsigned int>(bpSecPolicyPtr->m_hmacKeyEncryptionKey.size()),
            bpSecPolicyPtr->m_hmacKey.empty() ? NULL : bpSecPolicyPtr->m_hmacKey.data(), //NULL if not present (when no wrapped key is present)
            static_cast<unsigned int>(bpSecPolicyPtr->m_hmacKey.size()),
            ctx.m_bpsecReusableElementsInternal,
            markBibForDeletion))
        {
            LOG_ERROR(subprocess) << "Process: version 7 bundle received but cannot check integrity";
            return false;
        }
        //all verification ops successful for this bib block at this point
        if (markBibForDeletion) {
            static thread_local bool printedMsg = false;
            if (!printedMsg) {
                LOG_INFO(subprocess) << "first time ACCEPTED a bundle's integrity successfully from source node "
                    << bv.m_primaryBlockView.header.m_sourceNodeId
                    << " ..(This message type will now be suppressed.)";
                printedMsg = true;
            }
        }
        else {
            static thread_local bool printedMsg = false;
            if (!printedMsg) {
                LOG_INFO(subprocess) << "first time VERIFIED a bundle's integrity successfully from source node "
                    << bv.m_primaryBlockView.header.m_sourceNodeId
                    << " ..(This message type will now be suppressed.)";
                printedMsg = true;
            }
        }
    }
    
    return true;
}

bool BpSecPolicyManager::PopulateTargetArraysForSecuritySource(BundleViewV7& bv,
    BpSecPolicyProcessingContext& ctx,
    const BpSecPolicy& policy)
{
    ctx.m_bibTargetBlockNumbers.clear();
    ctx.m_bcbTargetBlockNumbers.clear();
    ctx.m_bcbTargetBibBlockNumberPlaceholderIndex = UINT64_MAX;
    if (policy.m_doIntegrity) {
        static thread_local bool printedMsg = false;
        for (FragmentSet::data_fragment_set_t::const_iterator it = policy.m_bibBlockTypeTargets.cbegin();
            it != policy.m_bibBlockTypeTargets.cend(); ++it)
        {
            const FragmentSet::data_fragment_t& df = *it;
            for (uint64_t blockType = df.beginIndex; blockType <= df.endIndex; ++blockType) {
                bv.GetCanonicalBlocksByType(static_cast<BPV7_BLOCK_TYPE_CODE>(blockType), ctx.m_tmpBlocks);
                
                for (std::size_t i = 0; i < ctx.m_tmpBlocks.size(); ++i) {
                    ctx.m_bibTargetBlockNumbers.push_back(ctx.m_tmpBlocks[i]->headerPtr->m_blockNumber);
                    if (!printedMsg) {
                        LOG_DEBUG(subprocess) << "first time bpsec security source adds integrity target for block number "
                            << ctx.m_bibTargetBlockNumbers.back()
                            << " ..(This message type will now be suppressed.)";
                    }
                }
            }
        }
        printedMsg = true;
    }
    if (policy.m_doConfidentiality) {
        static thread_local bool printedMsg = false;
        for (FragmentSet::data_fragment_set_t::const_iterator it = policy.m_bcbBlockTypeTargets.cbegin();
            it != policy.m_bcbBlockTypeTargets.cend(); ++it)
        {
            const FragmentSet::data_fragment_t& df = *it;
            for (uint64_t blockType = df.beginIndex; blockType <= df.endIndex; ++blockType) {
                if (blockType == static_cast<uint64_t>(BPV7_BLOCK_TYPE_CODE::INTEGRITY)) {
                    //integrity block number is auto-assigned later
                    ctx.m_bcbTargetBibBlockNumberPlaceholderIndex = ctx.m_bcbTargetBlockNumbers.size();
                    ctx.m_bcbTargetBlockNumbers.push_back(0);
                    if (!printedMsg) {
                        LOG_DEBUG(subprocess) << "first time bpsec add block target confidentiality placeholder for bib ..(This message type will now be suppressed.)";
                    }
                }
                else {
                    bv.GetCanonicalBlocksByType(static_cast<BPV7_BLOCK_TYPE_CODE>(blockType), ctx.m_tmpBlocks);
                    for (std::size_t i = 0; i < ctx.m_tmpBlocks.size(); ++i) {
                        ctx.m_bcbTargetBlockNumbers.push_back(ctx.m_tmpBlocks[i]->headerPtr->m_blockNumber);
                        if (!printedMsg) {
                            LOG_DEBUG(subprocess) << "first time bpsec security source adds confidentiality target for block number "
                                << ctx.m_bcbTargetBlockNumbers.back()
                                << " ..(This message type will now be suppressed.)";
                        }
                    }
                }
            }
        }
        printedMsg = true;
    }
    return true;
}

bool BpSecPolicyManager::PopulateTargetArraysForSecuritySource(
    const uint8_t* bpv7BlockTypeToManuallyAssignedBlockNumberLut,
    BpSecPolicyProcessingContext& ctx,
    const BpSecPolicy& policy)
{
    static constexpr std::size_t MAX_NUM_BPV7_BLOCK_TYPE_CODES = static_cast<std::size_t>(BPV7_BLOCK_TYPE_CODE::RESERVED_MAX_BLOCK_TYPES);
    ctx.m_bibTargetBlockNumbers.clear();
    ctx.m_bcbTargetBlockNumbers.clear();
    ctx.m_bcbTargetBibBlockNumberPlaceholderIndex = UINT64_MAX;
    if (policy.m_doIntegrity) {
        for (FragmentSet::data_fragment_set_t::const_iterator it = policy.m_bibBlockTypeTargets.cbegin();
            it != policy.m_bibBlockTypeTargets.cend(); ++it)
        {
            const FragmentSet::data_fragment_t& df = *it;
            for (uint64_t blockType = df.beginIndex; blockType <= df.endIndex; ++blockType) {
                if (blockType >= MAX_NUM_BPV7_BLOCK_TYPE_CODES) {
                    LOG_FATAL(subprocess) << "policy error: invalid block type " << blockType;
                    return false;
                }
                ctx.m_bibTargetBlockNumbers.push_back(bpv7BlockTypeToManuallyAssignedBlockNumberLut[blockType]);
                LOG_DEBUG(subprocess) << "bpsec add block target integrity " << ctx.m_bibTargetBlockNumbers.back();
            }
        }
    }
    if (policy.m_doConfidentiality) {
        for (FragmentSet::data_fragment_set_t::const_iterator it = policy.m_bcbBlockTypeTargets.cbegin();
            it != policy.m_bcbBlockTypeTargets.cend(); ++it)
        {
            const FragmentSet::data_fragment_t& df = *it;
            for (uint64_t blockType = df.beginIndex; blockType <= df.endIndex; ++blockType) {
                if (blockType >= MAX_NUM_BPV7_BLOCK_TYPE_CODES) {
                    LOG_FATAL(subprocess) << "policy error: invalid block type " << blockType;
                    return false;
                }
                if (blockType == static_cast<uint64_t>(BPV7_BLOCK_TYPE_CODE::INTEGRITY)) {
                    //integrity block number is auto-assigned later
                    ctx.m_bcbTargetBibBlockNumberPlaceholderIndex = ctx.m_bcbTargetBlockNumbers.size();
                    ctx.m_bcbTargetBlockNumbers.push_back(0);
                    LOG_DEBUG(subprocess) << "bpsec add block target confidentiality placeholder for bib";
                }
                else {
                    ctx.m_bcbTargetBlockNumbers.push_back(bpv7BlockTypeToManuallyAssignedBlockNumberLut[blockType]);
                    LOG_DEBUG(subprocess) << "bpsec add block target confidentiality " << ctx.m_bcbTargetBlockNumbers.back();
                }
            }
        }
    }
    return true;
}

bool BpSecPolicyManager::ProcessOutgoingBundle(BundleViewV7& bv,
    BpSecPolicyProcessingContext& ctx,
    const BpSecPolicy& policy,
    const cbhe_eid_t& thisSecuritySourceEid)
{
    if (policy.m_doIntegrity) {
        if (!BPSecManager::TryAddBundleIntegrity(ctx.m_hmacCtxWrapper,
            ctx.m_ctxWrapperKeyWrapOps,
            bv,
            policy.m_integrityScopeMask,
            policy.m_integrityVariant,
            policy.m_bibCrcType,
            thisSecuritySourceEid,
            ctx.m_bibTargetBlockNumbers.data(), static_cast<unsigned int>(ctx.m_bibTargetBlockNumbers.size()),
            policy.m_hmacKeyEncryptionKey.empty() ? NULL : policy.m_hmacKeyEncryptionKey.data(), //NULL if not present (for unwrapping hmac key only)
            static_cast<unsigned int>(policy.m_hmacKeyEncryptionKey.size()),
            policy.m_hmacKey.empty() ? NULL : policy.m_hmacKey.data(), //NULL if not present (when no wrapped key is present)
            static_cast<unsigned int>(policy.m_hmacKey.size()),
            ctx.m_bpsecReusableElementsInternal,
            NULL, //bib placed immediately after primary
            true))
        {
            LOG_ERROR(subprocess) << "cannot add integrity to bundle";
            return false;
        }
        if (ctx.m_bcbTargetBibBlockNumberPlaceholderIndex != UINT64_MAX) {
            ctx.m_bcbTargetBlockNumbers[ctx.m_bcbTargetBibBlockNumberPlaceholderIndex] = bv.m_listCanonicalBlockView.front().headerPtr->m_blockNumber;
        }
    }
    if (policy.m_doConfidentiality) {
        ctx.m_ivStruct.SerializeAndIncrement(policy.m_use12ByteIv);
        if (!BPSecManager::TryEncryptBundle(ctx.m_evpCtxWrapper,
            ctx.m_ctxWrapperKeyWrapOps,
            bv,
            policy.m_aadScopeMask,
            policy.m_confidentialityVariant,
            policy.m_bcbCrcType,
            thisSecuritySourceEid,
            ctx.m_bcbTargetBlockNumbers.data(), static_cast<unsigned int>(ctx.m_bcbTargetBlockNumbers.size()),
            ctx.m_ivStruct.m_initializationVector.data(), static_cast<unsigned int>(ctx.m_ivStruct.m_initializationVector.size()),
            policy.m_confidentialityKeyEncryptionKey.empty() ? NULL : policy.m_confidentialityKeyEncryptionKey.data(), //NULL if not present (for wrapping DEK only)
            static_cast<unsigned int>(policy.m_confidentialityKeyEncryptionKey.size()),
            policy.m_dataEncryptionKey.empty() ? NULL : policy.m_dataEncryptionKey.data(), //NULL if not present (when no wrapped key is present)
            static_cast<unsigned int>(policy.m_dataEncryptionKey.size()),
            ctx.m_bpsecReusableElementsInternal,
            NULL,
            true))
        {
            LOG_ERROR(subprocess) << "cannot encrypt bundle";
            return false;
        }
    }
    return true;
}


bool BpSecPolicyManager::FindPolicyAndProcessOutgoingBundle(BundleViewV7& bv, BpSecPolicyProcessingContext& ctx,
    const cbhe_eid_t& thisSecuritySourceEid) const
{
    const Bpv7CbhePrimaryBlock& primary = bv.m_primaryBlockView.header;
    const BpSecPolicy* bpSecPolicyPtr = FindPolicyWithCacheSupport(
        thisSecuritySourceEid, primary.m_sourceNodeId, primary.m_destinationEid, BPSEC_ROLE::SOURCE, ctx.m_searchCacheSource);
    if (bpSecPolicyPtr) {
        if (!BpSecPolicyManager::PopulateTargetArraysForSecuritySource(bv, ctx, *bpSecPolicyPtr)) {
            return false;
        }
        if (!BpSecPolicyManager::ProcessOutgoingBundle(bv, ctx, *bpSecPolicyPtr, thisSecuritySourceEid)) {
            return false;
        }
    }
    return true;
}

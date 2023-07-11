/**
 * @file TestBpSecPolicyManager.cpp
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
#include "BpSecPolicyManager.h"
#include "Logger.h"
#include <set>
#include "codec/BundleViewV7.h"
#include <boost/make_unique.hpp>
#include <boost/regex.hpp>
#include "Environment.h"


BOOST_AUTO_TEST_CASE(BpSecPolicyManagerTestCase)
{
    bool isNewPolicy;
    { //bad syntax
        BpSecPolicyManager m;
        BOOST_REQUIRE(m.CreateOrGetNewPolicy("ipn:**.*", "ipn:*.*", "ipn:*.*", BPSEC_ROLE::ACCEPTOR, isNewPolicy) == NULL);
        BOOST_REQUIRE(m.CreateOrGetNewPolicy("ipn:*.*", "ipn:*.**", "ipn:*.*", BPSEC_ROLE::ACCEPTOR, isNewPolicy) == NULL);
        BOOST_REQUIRE(m.CreateOrGetNewPolicy("ipn:*.*", "ipn:*.*", "ipn:***.*", BPSEC_ROLE::ACCEPTOR, isNewPolicy) == NULL);
        BOOST_REQUIRE(m.CreateOrGetNewPolicy("ipn:*.*", "ipn:*.*", "ipn:*.*", BPSEC_ROLE::RESERVED_MAX_ROLE_TYPES, isNewPolicy) == NULL);
    }
    { //duplication
        BpSecPolicyManager m;
        BpSecPolicy* pA = m.CreateOrGetNewPolicy("ipn:*.*", "ipn:*.*", "ipn:*.*", BPSEC_ROLE::ACCEPTOR, isNewPolicy);
        BOOST_REQUIRE(pA);
        BOOST_REQUIRE(isNewPolicy);
        BOOST_REQUIRE(m.CreateOrGetNewPolicy("ipn:*.*", "ipn:*.*", "ipn:*.*", BPSEC_ROLE::ACCEPTOR, isNewPolicy) == pA);
        BOOST_REQUIRE(!isNewPolicy);
        BOOST_REQUIRE(m.CreateOrGetNewPolicy("ipn:*.*", "ipn:*.*", "ipn:*.*", BPSEC_ROLE::ACCEPTOR, isNewPolicy) == pA);
        BOOST_REQUIRE(!isNewPolicy);
        BpSecPolicy* pS = m.CreateOrGetNewPolicy("ipn:*.*", "ipn:*.*", "ipn:*.*", BPSEC_ROLE::SOURCE, isNewPolicy);
        BOOST_REQUIRE(pS);
        BOOST_REQUIRE(pS != pA);
        BOOST_REQUIRE(isNewPolicy);
        BOOST_REQUIRE(m.CreateOrGetNewPolicy("ipn:*.*", "ipn:*.*", "ipn:*.*", BPSEC_ROLE::SOURCE, isNewPolicy) == pS);
        BOOST_REQUIRE(!isNewPolicy);
        BpSecPolicy* pV = m.CreateOrGetNewPolicy("ipn:*.*", "ipn:*.*", "ipn:*.*", BPSEC_ROLE::VERIFIER, isNewPolicy);
        BOOST_REQUIRE(pV);
        BOOST_REQUIRE(pV != pA);
        BOOST_REQUIRE(pV != pS);
        BOOST_REQUIRE(isNewPolicy);
        BOOST_REQUIRE(m.CreateOrGetNewPolicy("ipn:*.*", "ipn:*.*", "ipn:*.*", BPSEC_ROLE::VERIFIER, isNewPolicy) == pV);
        BOOST_REQUIRE(!isNewPolicy);
    }
    { //create and find
        BpSecPolicyManager m;
        const cbhe_eid_t ss(1, 1), bs(2, 1), bd(3, 1);
        BOOST_REQUIRE(m.FindPolicy(ss, bs, bd, BPSEC_ROLE::ACCEPTOR) == NULL);
        BOOST_REQUIRE(m.FindPolicy(ss, bs, bd, BPSEC_ROLE::ACCEPTOR) == NULL);
        BpSecPolicy* pAcceptor = m.CreateOrGetNewPolicy("ipn:*.*", "ipn:*.*", "ipn:*.*", BPSEC_ROLE::ACCEPTOR, isNewPolicy);
        BOOST_REQUIRE(pAcceptor);
        BOOST_REQUIRE(isNewPolicy);
        const BpSecPolicy* policyAny = m.FindPolicy(ss, bs, bd, BPSEC_ROLE::ACCEPTOR);
        BOOST_REQUIRE(policyAny);
        BOOST_REQUIRE(policyAny == pAcceptor);
        BOOST_REQUIRE(m.FindPolicy(ss, bs, bd, BPSEC_ROLE::ACCEPTOR) == policyAny);

        {
            const BpSecPolicy* pNew = m.CreateOrGetNewPolicy("ipn:1.1", "ipn:*.*", "ipn:*.*", BPSEC_ROLE::ACCEPTOR, isNewPolicy);
            BOOST_REQUIRE(pNew);
            BOOST_REQUIRE(isNewPolicy);
            BOOST_REQUIRE(pNew != policyAny);
            const BpSecPolicy* pFound = m.FindPolicy(ss, bs, bd, BPSEC_ROLE::ACCEPTOR);
            BOOST_REQUIRE(pNew == pFound);
            BOOST_REQUIRE(m.FindPolicy(cbhe_eid_t(ss.nodeId, ss.serviceId + 1), bs, bd, BPSEC_ROLE::ACCEPTOR) == policyAny);
            BOOST_REQUIRE(m.FindPolicy(cbhe_eid_t(ss.nodeId, ss.serviceId + 1), bs, bd, BPSEC_ROLE::VERIFIER) == NULL);
            BOOST_REQUIRE(m.FindPolicy(ss, bs, bd, BPSEC_ROLE::VERIFIER) == NULL);
        }
    }
    { //brute force
        static const std::vector<std::vector<std::string> > testCases = {
            {"ipn:*.*", "ipn:*.*", "ipn:*.*"},
            {"ipn:1.1", "ipn:*.*", "ipn:*.*"},
            {"ipn:1.*", "ipn:*.*", "ipn:*.*"},
            {"ipn:*.*", "ipn:2.1", "ipn:*.*"},
            {"ipn:*.*", "ipn:2.*", "ipn:*.*"},
            {"ipn:*.*", "ipn:*.*", "ipn:3.1"},
            {"ipn:*.*", "ipn:*.*", "ipn:3.*"},
        };
        static const std::vector<std::vector<cbhe_eid_t> > testCaseMatches = {
            {cbhe_eid_t (10, 10), cbhe_eid_t(20, 10), cbhe_eid_t(30, 10)},
            {cbhe_eid_t(1, 1), cbhe_eid_t(20, 10), cbhe_eid_t(30, 10)},
            {cbhe_eid_t(1, 10), cbhe_eid_t(20, 10), cbhe_eid_t(30, 10)},
            {cbhe_eid_t(10, 10), cbhe_eid_t(2, 1), cbhe_eid_t(30, 10)},
            {cbhe_eid_t(10, 10), cbhe_eid_t(2, 10), cbhe_eid_t(30, 10)},
            {cbhe_eid_t(10, 10), cbhe_eid_t(20, 10), cbhe_eid_t(3, 1)},
            {cbhe_eid_t(10, 10), cbhe_eid_t(20, 10), cbhe_eid_t(3, 10)}
        };
        std::set<const BpSecPolicy*> ptrSet;
        std::map<std::string, const BpSecPolicy*> caseToPtrMap;
        BpSecPolicyManager m;
        for (std::size_t i = 0; i < testCases.size(); ++i) {
            const std::vector<std::string>& testCase = testCases[i];
            const BpSecPolicy* pNew = m.CreateOrGetNewPolicy(testCase[0], testCase[1], testCase[2], BPSEC_ROLE::ACCEPTOR, isNewPolicy);
            BOOST_REQUIRE(pNew);
            BOOST_REQUIRE(isNewPolicy);
            BOOST_REQUIRE(ptrSet.emplace(pNew).second); //was inserted (new ptr)
            BOOST_REQUIRE(caseToPtrMap.emplace(testCase[0] + testCase[1] + testCase[2], pNew).second);
        }
        for (std::size_t i = 0; i < testCases.size(); ++i) {
            const std::vector<std::string>& testCase = testCases[i];
            const std::vector<cbhe_eid_t>& testCaseMatch = testCaseMatches[i];
            const BpSecPolicy* pFound = m.FindPolicy(testCaseMatch[0], testCaseMatch[1], testCaseMatch[2], BPSEC_ROLE::ACCEPTOR);
            BOOST_REQUIRE(pFound);
            BOOST_REQUIRE(caseToPtrMap[testCase[0] + testCase[1] + testCase[2]] == pFound);
        }
    }
    { //cache
        BpSecPolicyManager m;
        PolicySearchCache searchCache;
        const cbhe_eid_t ss(1, 1), bs(2, 1), bd(3, 1);
        BOOST_REQUIRE(m.CreateOrGetNewPolicy("ipn:*.*", "ipn:*.*", "ipn:*.*", BPSEC_ROLE::ACCEPTOR, isNewPolicy));
        BOOST_REQUIRE(isNewPolicy);
        const BpSecPolicy* policyAny = m.FindPolicyWithCacheSupport(ss, bs, bd, BPSEC_ROLE::ACCEPTOR, searchCache);
        BOOST_REQUIRE(policyAny);
        BOOST_REQUIRE(!searchCache.wasCacheHit);
        BOOST_REQUIRE(m.FindPolicyWithCacheSupport(ss, bs, bd, BPSEC_ROLE::ACCEPTOR, searchCache) == policyAny);
        BOOST_REQUIRE(searchCache.wasCacheHit);

        //new query
        const cbhe_eid_t ss2(10, 1);
        BOOST_REQUIRE(m.FindPolicyWithCacheSupport(ss2, bs, bd, BPSEC_ROLE::ACCEPTOR, searchCache) == policyAny);
        BOOST_REQUIRE(!searchCache.wasCacheHit);
        BOOST_REQUIRE(m.FindPolicyWithCacheSupport(ss2, bs, bd, BPSEC_ROLE::ACCEPTOR, searchCache) == policyAny);
        BOOST_REQUIRE(searchCache.wasCacheHit);
    }
}

BOOST_AUTO_TEST_CASE(BpSecPolicyManager2TestCase)
{
    static const cbhe_eid_t BUNDLE_SRC(1, 1);
    static const cbhe_eid_t BUNDLE_FINAL_DEST(2, 1);
    static const std::string payloadString = { "This is the data inside the bpv7 payload block!!!" };
    static const std::string customExtensionBlockString = { "My custom extension block." };
    padded_vector_uint8_t bundleSerializedOriginal;
    {
        BundleViewV7 bv;
        Bpv7CbhePrimaryBlock& primary = bv.m_primaryBlockView.header;
        primary.SetZero();


        primary.m_bundleProcessingControlFlags = BPV7_BUNDLEFLAG::NOFRAGMENT;  //All BP endpoints identified by ipn-scheme endpoint IDs are singleton endpoints.
        primary.m_sourceNodeId = BUNDLE_SRC;
        primary.m_destinationEid = BUNDLE_FINAL_DEST;
        primary.m_reportToEid.Set(0, 0);
        primary.m_creationTimestamp.millisecondsSinceStartOfYear2000 = 1000;
        primary.m_lifetimeMilliseconds = 1000;
        primary.m_creationTimestamp.sequenceNumber = 1;
        primary.m_crcType = BPV7_CRC_TYPE::NONE;
        bv.m_primaryBlockView.SetManuallyModified();

        
        { //add custom extension block
            std::unique_ptr<Bpv7CanonicalBlock> blockPtr = boost::make_unique<Bpv7CanonicalBlock>();
            Bpv7CanonicalBlock& block = *blockPtr;
            //block.SetZero();

            block.m_blockTypeCode = BPV7_BLOCK_TYPE_CODE::UNUSED_4;
            block.m_blockProcessingControlFlags = BPV7_BLOCKFLAG::REMOVE_BLOCK_IF_IT_CANT_BE_PROCESSED; //something for checking against
            block.m_blockNumber = 2;
            block.m_crcType = BPV7_CRC_TYPE::NONE;
            block.m_dataLength = customExtensionBlockString.size();
            block.m_dataPtr = (uint8_t*)customExtensionBlockString.data(); //customExtensionBlockString must remain in scope until after render
            bv.AppendMoveCanonicalBlock(std::move(blockPtr));

        }
        
        { //add payload block
            std::unique_ptr<Bpv7CanonicalBlock> blockPtr = boost::make_unique<Bpv7CanonicalBlock>();
            Bpv7CanonicalBlock& block = *blockPtr;
            //block.SetZero();

            block.m_blockTypeCode = BPV7_BLOCK_TYPE_CODE::PAYLOAD;
            block.m_blockProcessingControlFlags = BPV7_BLOCKFLAG::REMOVE_BLOCK_IF_IT_CANT_BE_PROCESSED; //something for checking against
            block.m_blockNumber = 1; //must be 1
            block.m_crcType = BPV7_CRC_TYPE::NONE;
            block.m_dataLength = payloadString.size();
            block.m_dataPtr = (uint8_t*)payloadString.data(); //payloadString must remain in scope until after render
            bv.AppendMoveCanonicalBlock(std::move(blockPtr));

        }

        BOOST_REQUIRE(bv.Render(5000));

        bundleSerializedOriginal = bv.m_frontBuffer;
    }
    std::string keyDir = (Environment::GetPathHdtnSourceRoot() / "config_files" / "bpsec").string();
    std::replace(keyDir.begin(), keyDir.end(), '\\', '/'); // replace all '\' to '/'
    const std::string securitySourcePolicyJson = std::string(
R"({
    "bpsecConfigName": "my BPSec Config",
    "policyRules": [
        {
            "description": " Confidentiality source rule",
            "securityPolicyRuleId": 1,
            "securityRole": "source",
            "securitySource": "ipn:10.*",
            "bundleSource": [
                "ipn:*.*"
            ],
            "bundleFinalDestination": [
                "ipn:*.*"
            ],
            "securityTargetBlockTypes": [
                1
            ],
            "securityService": "confidentiality",
            "securityContext": "aesGcm",
            "securityFailureEventSetReference": "default_confidentiality",
            "securityContextParams": [
                {
                    "paramName": "aesVariant",
                    "value": 256
                },
                {
                    "paramName": "ivSizeBytes",
                    "value": 12
                },
                {
                    "paramName": "keyFile",
                    "value": ")") + keyDir + std::string(R"(/ipn10.1_confidentiality.key"
                },
                {
                    "paramName": "securityBlockCrc",
                    "value": 0
                },
                {
                    "paramName": "scopeFlags",
                    "value": 7
                }
            ]
        }
    ],
    "securityFailureEventSets": [
        {
            "name": "default_confidentiality",
            "description": "default bcb confidentiality security operations event set",
            "securityOperationEvents": [
                {
                    "eventId": "sopCorruptedAtAcceptor",
                    "actions": [
                        "removeSecurityOperation"
                    ]
                },
                {
                    "eventId": "sopMisconfiguredAtVerifier",
                    "actions": [
                        "failBundleForwarding",
                        "reportReasonCode"
                    ]
                }
            ]
        }
    ]
})");

    const std::string securityAcceptorPolicyJson = std::string(
R"({
    "bpsecConfigName": "my BPSec Config",
    "policyRules": [
        {
            "description": " Confidentiality acceptor rule",
            "securityPolicyRuleId": 1,
            "securityRole": "acceptor",
            "securitySource": "ipn:10.1",
            "bundleSource": [
                "ipn:*.*"
            ],
            "bundleFinalDestination": [
                "ipn:*.*"
            ],
            "securityTargetBlockTypes": [
                1
            ],
            "securityService": "confidentiality",
            "securityContext": "aesGcm",
            "securityFailureEventSetReference": "default_confidentiality",
            "securityContextParams": [
                {
                    "paramName": "aesVariant",
                    "value": 256
                },
                {
                    "paramName": "ivSizeBytes",
                    "value": 12
                },
                {
                    "paramName": "keyFile",
                    "value": ")") + keyDir + std::string(R"(/ipn10.1_confidentiality.key"
                },
                {
                    "paramName": "securityBlockCrc",
                    "value": 0
                },
                {
                    "paramName": "scopeFlags",
                    "value": 7
                }
            ]
        }
    ],
    "securityFailureEventSets": [
        {
            "name": "default_confidentiality",
            "description": "default bcb confidentiality security operations event set",
            "securityOperationEvents": [
                {
                    "eventId": "sopCorruptedAtAcceptor",
                    "actions": [
                        "removeSecurityOperation"
                    ]
                }
            ]
        }
    ]
})");
    //std::cout << mystring << "\n";
    const cbhe_eid_t THIS_EID_SECURITY_SOURCE(10, 1);
    const cbhe_eid_t THIS_EID_FINAL_DEST(2, 1);
    
    padded_vector_uint8_t encryptedBundle;
    { //simple confidentiality success from security source ipn:10.1 (which encrypts) to an acceptor which decrypts
        //security source read config and encrypt bundle
        BpSecConfig_ptr bpSecConfigPtrTx = BpSecConfig::CreateFromJson(securitySourcePolicyJson);
        BOOST_REQUIRE(bpSecConfigPtrTx);
        BpSecPolicyManager bpSecPolicyManagerTx;
        BpSecPolicyProcessingContext policyProcessingCtxTx;
        BOOST_REQUIRE(bpSecPolicyManagerTx.LoadFromConfig(*bpSecConfigPtrTx));
        BOOST_REQUIRE(bpSecPolicyManagerTx.FindPolicy(THIS_EID_SECURITY_SOURCE, cbhe_eid_t(1, 1), THIS_EID_FINAL_DEST, BPSEC_ROLE::SOURCE));
        BundleViewV7 bvTx;
        BOOST_REQUIRE(bvTx.CopyAndLoadBundle(bundleSerializedOriginal.data(), bundleSerializedOriginal.size()));
        BOOST_REQUIRE(bpSecPolicyManagerTx.FindPolicyAndProcessOutgoingBundle(bvTx, policyProcessingCtxTx, THIS_EID_SECURITY_SOURCE));
        BOOST_REQUIRE(bvTx.RenderInPlace(PaddedMallocatorConstants::PADDING_ELEMENTS_BEFORE));
        BOOST_REQUIRE_GT(bvTx.m_renderedBundle.size(), bundleSerializedOriginal.size()); //bundle gets bigger with added security
        encryptedBundle.assign((const uint8_t*)bvTx.m_renderedBundle.data(), ((const uint8_t*)bvTx.m_renderedBundle.data()) + bvTx.m_renderedBundle.size());

        //security acceptor read config and decrypt bundle
        BpSecConfig_ptr bpSecConfigPtrRx = BpSecConfig::CreateFromJson(securityAcceptorPolicyJson);
        BOOST_REQUIRE(bpSecConfigPtrRx);
        BpSecPolicyManager bpSecPolicyManagerRx;
        BpSecPolicyProcessingContext policyProcessingCtxRx;
        BOOST_REQUIRE(bpSecPolicyManagerRx.LoadFromConfig(*bpSecConfigPtrRx));
        BOOST_REQUIRE(bpSecPolicyManagerRx.FindPolicy(THIS_EID_SECURITY_SOURCE, cbhe_eid_t(1, 1), THIS_EID_FINAL_DEST, BPSEC_ROLE::ACCEPTOR));
        BundleViewV7 bvRx;
        BOOST_REQUIRE(bvRx.CopyAndLoadBundle(encryptedBundle.data(), encryptedBundle.size()));
        { //get payload encrypted
            std::vector<BundleViewV7::Bpv7CanonicalBlockView*> blocks;
            bvRx.GetCanonicalBlocksByType(BPV7_BLOCK_TYPE_CODE::PAYLOAD, blocks);
            BOOST_REQUIRE_EQUAL(blocks.size(), 1);
            BOOST_REQUIRE(blocks[0]->isEncrypted); //encrypted
        }
        BpSecBundleProcessor::ReturnResult res;
        BOOST_REQUIRE(bpSecPolicyManagerRx.ProcessReceivedBundle(bvRx, policyProcessingCtxRx, res, THIS_EID_FINAL_DEST.nodeId));
        BOOST_REQUIRE(res.errorCode == BpSecBundleProcessor::BPSEC_ERROR_CODES::NO_ERRORS);
        { //get payload decrypted
            std::vector<BundleViewV7::Bpv7CanonicalBlockView*> blocks;
            bvRx.GetCanonicalBlocksByType(BPV7_BLOCK_TYPE_CODE::PAYLOAD, blocks);
            BOOST_REQUIRE_EQUAL(blocks.size(), 1);
            const char* strPtr = (const char*)blocks[0]->headerPtr->m_dataPtr;
            std::string s(strPtr, strPtr + blocks[0]->headerPtr->m_dataLength);
            //LOG_INFO(subprocess) << "s: " << s;
            BOOST_REQUIRE_EQUAL(s, payloadString);
            BOOST_REQUIRE(!blocks[0]->isEncrypted); //not encrypted
        }
    }
    
    { //simple confidentiality failure (corruption) which has a bad key at acceptor
        //alter the key file (10.1 changes to 1.1)
        static const boost::regex regexMatch("ipn10.1_confidentiality.key");
        const std::string securityAcceptorPolicyBadJson = boost::regex_replace(securityAcceptorPolicyJson, regexMatch, "ipn1.1_confidentiality.key");
        //std::cout << securityAcceptorPolicyBadJson << "\n";

        //security acceptor read config and decrypt bundle
        BpSecConfig_ptr bpSecConfigPtrRx = BpSecConfig::CreateFromJson(securityAcceptorPolicyBadJson);
        BOOST_REQUIRE(bpSecConfigPtrRx);
        BpSecPolicyManager bpSecPolicyManagerRx;
        BpSecPolicyProcessingContext policyProcessingCtxRx;
        BOOST_REQUIRE(bpSecPolicyManagerRx.LoadFromConfig(*bpSecConfigPtrRx));
        BOOST_REQUIRE(bpSecPolicyManagerRx.FindPolicy(THIS_EID_SECURITY_SOURCE, cbhe_eid_t(1, 1), THIS_EID_FINAL_DEST, BPSEC_ROLE::ACCEPTOR));
        BundleViewV7 bvRx;
        BOOST_REQUIRE(bvRx.CopyAndLoadBundle(encryptedBundle.data(), encryptedBundle.size()));
        BpSecBundleProcessor::ReturnResult res;
        BOOST_REQUIRE(!bpSecPolicyManagerRx.ProcessReceivedBundle(bvRx, policyProcessingCtxRx, res, THIS_EID_FINAL_DEST.nodeId)); //bundle must be dropped (payload cannot be decrypted)
        BOOST_REQUIRE(res.errorCode == BpSecBundleProcessor::BPSEC_ERROR_CODES::CORRUPTED);
        BOOST_REQUIRE(res.errorStringPtr);
        if (res.errorStringPtr) {
            BOOST_REQUIRE_EQUAL(*res.errorStringPtr, "unable to decrypt the target block number 1");
        }
    }

    { //simple confidentiality failure (misconfigured) which has the wrong aes variant
        static const boost::regex regexMatch("256");
        const std::string securityAcceptorPolicyBadJson = boost::regex_replace(securityAcceptorPolicyJson, regexMatch, "128");
        //std::cout << securityAcceptorPolicyBadJson << "\n";

        //security acceptor read config and decrypt bundle
        BpSecConfig_ptr bpSecConfigPtrRx = BpSecConfig::CreateFromJson(securityAcceptorPolicyBadJson);
        BOOST_REQUIRE(bpSecConfigPtrRx);
        BpSecPolicyManager bpSecPolicyManagerRx;
        BpSecPolicyProcessingContext policyProcessingCtxRx;
        BOOST_REQUIRE(bpSecPolicyManagerRx.LoadFromConfig(*bpSecConfigPtrRx));
        BOOST_REQUIRE(bpSecPolicyManagerRx.FindPolicy(THIS_EID_SECURITY_SOURCE, cbhe_eid_t(1, 1), THIS_EID_FINAL_DEST, BPSEC_ROLE::ACCEPTOR));
        BundleViewV7 bvRx;
        BOOST_REQUIRE(bvRx.CopyAndLoadBundle(encryptedBundle.data(), encryptedBundle.size()));
        BpSecBundleProcessor::ReturnResult res;
        BOOST_REQUIRE(!bpSecPolicyManagerRx.ProcessReceivedBundle(bvRx, policyProcessingCtxRx, res, THIS_EID_FINAL_DEST.nodeId)); //bundle must be dropped (payload cannot be decrypted)
        BOOST_REQUIRE(res.errorCode == BpSecBundleProcessor::BPSEC_ERROR_CODES::MISCONFIGURED);
        BOOST_REQUIRE(res.errorStringPtr);
        if (res.errorStringPtr) {
            BOOST_REQUIRE_EQUAL(*res.errorStringPtr, "BCB AES variant received (A256GCM), does not match the expected variant in the policy (A128GCM)");
        }
    }

    { //simple confidentiality failure (misconfigured) which has the wrong iv size bytes
        static const boost::regex regexMatch(": 12");
        const std::string securityAcceptorPolicyBadJson = boost::regex_replace(securityAcceptorPolicyJson, regexMatch, ": 16");
        //std::cout << securityAcceptorPolicyBadJson << "\n";

        //security acceptor read config and decrypt bundle
        BpSecConfig_ptr bpSecConfigPtrRx = BpSecConfig::CreateFromJson(securityAcceptorPolicyBadJson);
        BOOST_REQUIRE(bpSecConfigPtrRx);
        BpSecPolicyManager bpSecPolicyManagerRx;
        BpSecPolicyProcessingContext policyProcessingCtxRx;
        BOOST_REQUIRE(bpSecPolicyManagerRx.LoadFromConfig(*bpSecConfigPtrRx));
        BOOST_REQUIRE(bpSecPolicyManagerRx.FindPolicy(THIS_EID_SECURITY_SOURCE, cbhe_eid_t(1, 1), THIS_EID_FINAL_DEST, BPSEC_ROLE::ACCEPTOR));
        BundleViewV7 bvRx;
        BOOST_REQUIRE(bvRx.CopyAndLoadBundle(encryptedBundle.data(), encryptedBundle.size()));
        BpSecBundleProcessor::ReturnResult res;
        BOOST_REQUIRE(!bpSecPolicyManagerRx.ProcessReceivedBundle(bvRx, policyProcessingCtxRx, res, THIS_EID_FINAL_DEST.nodeId)); //bundle must be dropped (payload cannot be decrypted)
        BOOST_REQUIRE(res.errorCode == BpSecBundleProcessor::BPSEC_ERROR_CODES::MISCONFIGURED);
        BOOST_REQUIRE(res.errorStringPtr);
        if (res.errorStringPtr) {
            BOOST_REQUIRE_EQUAL(*res.errorStringPtr, "BCB AES IV received length(12), does not match the expected IV length to receive in the policy (16)");
        }
    }

    { //simple confidentiality failure (misconfigured) which has the wrong scope flags
        static const boost::regex regexMatch(": 7");
        const std::string securityAcceptorPolicyBadJson = boost::regex_replace(securityAcceptorPolicyJson, regexMatch, ": 0");
        //std::cout << securityAcceptorPolicyBadJson << "\n";

        //security acceptor read config and decrypt bundle
        BpSecConfig_ptr bpSecConfigPtrRx = BpSecConfig::CreateFromJson(securityAcceptorPolicyBadJson);
        BOOST_REQUIRE(bpSecConfigPtrRx);
        BpSecPolicyManager bpSecPolicyManagerRx;
        BpSecPolicyProcessingContext policyProcessingCtxRx;
        BOOST_REQUIRE(bpSecPolicyManagerRx.LoadFromConfig(*bpSecConfigPtrRx));
        BOOST_REQUIRE(bpSecPolicyManagerRx.FindPolicy(THIS_EID_SECURITY_SOURCE, cbhe_eid_t(1, 1), THIS_EID_FINAL_DEST, BPSEC_ROLE::ACCEPTOR));
        BundleViewV7 bvRx;
        BOOST_REQUIRE(bvRx.CopyAndLoadBundle(encryptedBundle.data(), encryptedBundle.size()));
        BpSecBundleProcessor::ReturnResult res;
        BOOST_REQUIRE(!bpSecPolicyManagerRx.ProcessReceivedBundle(bvRx, policyProcessingCtxRx, res, THIS_EID_FINAL_DEST.nodeId)); //bundle must be dropped (payload cannot be decrypted)
        BOOST_REQUIRE(res.errorCode == BpSecBundleProcessor::BPSEC_ERROR_CODES::MISCONFIGURED);
        BOOST_REQUIRE(res.errorStringPtr);
        if (res.errorStringPtr) {
            BOOST_REQUIRE_EQUAL(*res.errorStringPtr, "BCB AES aad scope mask received (7), does not match the expected aad scope mask in the policy (0)");
        }
    }

    { //simple confidentiality failure (misconfigured) which has a policy that has MORE securityTargetBlockTypes than the block
        static const boost::regex regexMatch("        1");
        const std::string securityAcceptorPolicyBadJson = boost::regex_replace(securityAcceptorPolicyJson, regexMatch, "1, 2");
        //std::cout << securityAcceptorPolicyBadJson << "\n";

        //security acceptor read config and decrypt bundle
        BpSecConfig_ptr bpSecConfigPtrRx = BpSecConfig::CreateFromJson(securityAcceptorPolicyBadJson);
        BOOST_REQUIRE(bpSecConfigPtrRx);
        BpSecPolicyManager bpSecPolicyManagerRx;
        BpSecPolicyProcessingContext policyProcessingCtxRx;
        BOOST_REQUIRE(bpSecPolicyManagerRx.LoadFromConfig(*bpSecConfigPtrRx));
        BOOST_REQUIRE(bpSecPolicyManagerRx.FindPolicy(THIS_EID_SECURITY_SOURCE, cbhe_eid_t(1, 1), THIS_EID_FINAL_DEST, BPSEC_ROLE::ACCEPTOR));
        BundleViewV7 bvRx;
        BOOST_REQUIRE(bvRx.CopyAndLoadBundle(encryptedBundle.data(), encryptedBundle.size()));
        BpSecBundleProcessor::ReturnResult res;
        BOOST_REQUIRE(!bpSecPolicyManagerRx.ProcessReceivedBundle(bvRx, policyProcessingCtxRx, res, THIS_EID_FINAL_DEST.nodeId)); //bundle must be dropped (payload cannot be decrypted)
        BOOST_REQUIRE(res.errorCode == BpSecBundleProcessor::BPSEC_ERROR_CODES::MISCONFIGURED);
        BOOST_REQUIRE(res.errorStringPtr);
        if (res.errorStringPtr) {
            BOOST_REQUIRE_EQUAL(*res.errorStringPtr, "the BCB AES failed to target all of the canonical block types within the policy (missing_mask=4d)"); //4=0b100 => block type 2 missing
        }
    }

    { //simple confidentiality failure (misconfigured) which has a policy that has LESS securityTargetBlockTypes than the block
        static const boost::regex regexMatch("        1");
        const std::string securityAcceptorPolicyBadJson = boost::regex_replace(securityAcceptorPolicyJson, regexMatch, " ");
        //std::cout << securityAcceptorPolicyBadJson << "\n";

        //security acceptor read config and decrypt bundle
        BpSecConfig_ptr bpSecConfigPtrRx = BpSecConfig::CreateFromJson(securityAcceptorPolicyBadJson);
        BOOST_REQUIRE(bpSecConfigPtrRx);
        BpSecPolicyManager bpSecPolicyManagerRx;
        BpSecPolicyProcessingContext policyProcessingCtxRx;
        BOOST_REQUIRE(bpSecPolicyManagerRx.LoadFromConfig(*bpSecConfigPtrRx));
        BOOST_REQUIRE(bpSecPolicyManagerRx.FindPolicy(THIS_EID_SECURITY_SOURCE, cbhe_eid_t(1, 1), THIS_EID_FINAL_DEST, BPSEC_ROLE::ACCEPTOR));
        BundleViewV7 bvRx;
        BOOST_REQUIRE(bvRx.CopyAndLoadBundle(encryptedBundle.data(), encryptedBundle.size()));
        BpSecBundleProcessor::ReturnResult res;
        BOOST_REQUIRE(!bpSecPolicyManagerRx.ProcessReceivedBundle(bvRx, policyProcessingCtxRx, res, THIS_EID_FINAL_DEST.nodeId)); //bundle must be dropped (payload cannot be decrypted)
        BOOST_REQUIRE(res.errorCode == BpSecBundleProcessor::BPSEC_ERROR_CODES::MISCONFIGURED);
        BOOST_REQUIRE(res.errorStringPtr);
        if (res.errorStringPtr) {
            BOOST_REQUIRE_EQUAL(*res.errorStringPtr, "BCB AES security target (1) targets a canonical block type code (1) that was unexpected per the policy");
        }
        //std::cout << *res.errorStringPtr << "\n";
    }

    { //simple confidentiality failure (missing at acceptor) which has the wrong security source
        //alter the key file (10.1 changes to 1.1)
        static const boost::regex regexMatch("ipn:10.1");
        const std::string securityAcceptorPolicyBadJson = boost::regex_replace(securityAcceptorPolicyJson, regexMatch, "ipn:20.1");
        //std::cout << securityAcceptorPolicyBadJson << "\n";

        //security acceptor read config and decrypt bundle
        BpSecConfig_ptr bpSecConfigPtrRx = BpSecConfig::CreateFromJson(securityAcceptorPolicyBadJson);
        BOOST_REQUIRE(bpSecConfigPtrRx);
        BpSecPolicyManager bpSecPolicyManagerRx;
        BpSecPolicyProcessingContext policyProcessingCtxRx;
        BOOST_REQUIRE(bpSecPolicyManagerRx.LoadFromConfig(*bpSecConfigPtrRx));
        BOOST_REQUIRE(bpSecPolicyManagerRx.FindPolicy(cbhe_eid_t(20, 1), cbhe_eid_t(1, 1), THIS_EID_FINAL_DEST, BPSEC_ROLE::ACCEPTOR));
        { //acceptor node id matches bundle final dest
            BundleViewV7 bvRx;
            BOOST_REQUIRE(bvRx.CopyAndLoadBundle(encryptedBundle.data(), encryptedBundle.size()));
            BpSecBundleProcessor::ReturnResult res;
            BOOST_REQUIRE(!bpSecPolicyManagerRx.ProcessReceivedBundle(bvRx, policyProcessingCtxRx, res, THIS_EID_FINAL_DEST.nodeId)); //bundle must be dropped (payload cannot be decrypted)
            BOOST_REQUIRE(res.errorCode == BpSecBundleProcessor::BPSEC_ERROR_CODES::MISSING);
            BOOST_REQUIRE(res.errorStringPtr);
            if (res.errorStringPtr) {
                BOOST_REQUIRE_EQUAL(*res.errorStringPtr,
                    "Bundle is at final destination but an acceptor policy could not be found for "
                    "BCB with securitySource=ipn:10.1,bundleSource=ipn:1.1,bundleFinalDest=ipn:2.1"
                );
            }
        }
        { //acceptor node id does not match bundle final dest (bundle is simply forwarded as encrypted)
            BundleViewV7 bvRx;
            BOOST_REQUIRE(bvRx.CopyAndLoadBundle(encryptedBundle.data(), encryptedBundle.size()));
            BpSecBundleProcessor::ReturnResult res;
            BOOST_REQUIRE(bpSecPolicyManagerRx.ProcessReceivedBundle(bvRx, policyProcessingCtxRx, res, THIS_EID_FINAL_DEST.nodeId + 5)); //bundle is not dropped
            BOOST_REQUIRE(res.errorCode == BpSecBundleProcessor::BPSEC_ERROR_CODES::NO_ERRORS);
        }
    }

    /////////////////////////////////////////////////
    //encrypt only custom extension block (type=4)
    /////////////////////////////////////////////////
    { 
        static const boost::regex regexMatch("        1");
        const std::string securitySourcePolicy4Json = boost::regex_replace(securitySourcePolicyJson, regexMatch, "        4");

        //security source read config and encrypt bundle
        BpSecConfig_ptr bpSecConfigPtrTx = BpSecConfig::CreateFromJson(securitySourcePolicy4Json);
        BOOST_REQUIRE(bpSecConfigPtrTx);
        BpSecPolicyManager bpSecPolicyManagerTx;
        BpSecPolicyProcessingContext policyProcessingCtxTx;
        BOOST_REQUIRE(bpSecPolicyManagerTx.LoadFromConfig(*bpSecConfigPtrTx));
        BOOST_REQUIRE(bpSecPolicyManagerTx.FindPolicy(THIS_EID_SECURITY_SOURCE, cbhe_eid_t(1, 1), THIS_EID_FINAL_DEST, BPSEC_ROLE::SOURCE));
        BundleViewV7 bvTx;
        BOOST_REQUIRE(bvTx.CopyAndLoadBundle(bundleSerializedOriginal.data(), bundleSerializedOriginal.size()));
        BOOST_REQUIRE_EQUAL(bvTx.GetNumCanonicalBlocks(), 2);
        BOOST_REQUIRE(bpSecPolicyManagerTx.FindPolicyAndProcessOutgoingBundle(bvTx, policyProcessingCtxTx, THIS_EID_SECURITY_SOURCE));
        BOOST_REQUIRE(bvTx.RenderInPlace(PaddedMallocatorConstants::PADDING_ELEMENTS_BEFORE));
        BOOST_REQUIRE_EQUAL(bvTx.GetNumCanonicalBlocks(), 3);
        BOOST_REQUIRE_GT(bvTx.m_renderedBundle.size(), bundleSerializedOriginal.size()); //bundle gets bigger with added security
        encryptedBundle.assign((const uint8_t*)bvTx.m_renderedBundle.data(), ((const uint8_t*)bvTx.m_renderedBundle.data()) + bvTx.m_renderedBundle.size());
    }

    { //simple confidentiality failure (corruption) which has a bad key at acceptor, SOp removed per sopCorruptedAtAcceptor policy
        //alter the key file (10.1 changes to 1.1)
        static const boost::regex regexMatch("ipn10.1_confidentiality.key");
        static const boost::regex regexMatch2("        1");
        std::string securityAcceptorPolicyBadJson = boost::regex_replace(securityAcceptorPolicyJson, regexMatch, "ipn1.1_confidentiality.key");
        securityAcceptorPolicyBadJson = boost::regex_replace(securityAcceptorPolicyBadJson, regexMatch2, "        4");
        //std::cout << securityAcceptorPolicyBadJson << "\n";

        //security acceptor read config and decrypt bundle
        BpSecConfig_ptr bpSecConfigPtrRx = BpSecConfig::CreateFromJson(securityAcceptorPolicyBadJson);
        BOOST_REQUIRE(bpSecConfigPtrRx);
        BpSecPolicyManager bpSecPolicyManagerRx;
        BpSecPolicyProcessingContext policyProcessingCtxRx;
        BOOST_REQUIRE(bpSecPolicyManagerRx.LoadFromConfig(*bpSecConfigPtrRx));
        BOOST_REQUIRE(bpSecPolicyManagerRx.FindPolicy(THIS_EID_SECURITY_SOURCE, cbhe_eid_t(1, 1), THIS_EID_FINAL_DEST, BPSEC_ROLE::ACCEPTOR));
        BundleViewV7 bvRx;
        BOOST_REQUIRE(bvRx.CopyAndLoadBundle(encryptedBundle.data(), encryptedBundle.size()));
        BpSecBundleProcessor::ReturnResult res;
        BOOST_REQUIRE(bpSecPolicyManagerRx.ProcessReceivedBundle(bvRx, policyProcessingCtxRx, res, THIS_EID_FINAL_DEST.nodeId)); //bundle need NOT be dropped since it doesnt target payload
        BOOST_REQUIRE(res.errorCode == BpSecBundleProcessor::BPSEC_ERROR_CODES::CORRUPTED);
        BOOST_REQUIRE_EQUAL(bvRx.GetNumCanonicalBlocks(), 3);
        BOOST_REQUIRE(bvRx.RenderInPlace(PaddedMallocatorConstants::PADDING_ELEMENTS_BEFORE));
        BOOST_REQUIRE_EQUAL(bvRx.GetNumCanonicalBlocks(), 2); //SOp removed per sopCorruptedAtAcceptor policy
        BOOST_REQUIRE(res.errorStringPtr);
        if (res.errorStringPtr) {
            BOOST_REQUIRE_EQUAL(*res.errorStringPtr, "unable to decrypt the target block number 2");
        }
    }

    { //simple confidentiality failure (corruption) which has a bad key at acceptor, SOp + Target removed per sopCorruptedAtAcceptor policy
        //alter the key file (10.1 changes to 1.1)
        static const boost::regex regexMatch("ipn10.1_confidentiality.key");
        static const boost::regex regexMatch2("        1");
        std::string securityAcceptorPolicyBadJson = boost::regex_replace(securityAcceptorPolicyJson, regexMatch, "ipn1.1_confidentiality.key");
        securityAcceptorPolicyBadJson = boost::regex_replace(securityAcceptorPolicyBadJson, regexMatch2, "        4");
        static const boost::regex regexMatch3("removeSecurityOperation");
        securityAcceptorPolicyBadJson = boost::regex_replace(securityAcceptorPolicyBadJson, regexMatch3,
            "removeSecurityOperation\", \"removeSecurityOperationTargetBlock");

        //security acceptor read config and decrypt bundle
        BpSecConfig_ptr bpSecConfigPtrRx = BpSecConfig::CreateFromJson(securityAcceptorPolicyBadJson);
        BOOST_REQUIRE(bpSecConfigPtrRx);
        BpSecPolicyManager bpSecPolicyManagerRx;
        BpSecPolicyProcessingContext policyProcessingCtxRx;
        BOOST_REQUIRE(bpSecPolicyManagerRx.LoadFromConfig(*bpSecConfigPtrRx));
        BOOST_REQUIRE(bpSecPolicyManagerRx.FindPolicy(THIS_EID_SECURITY_SOURCE, cbhe_eid_t(1, 1), THIS_EID_FINAL_DEST, BPSEC_ROLE::ACCEPTOR));
        BundleViewV7 bvRx;
        BOOST_REQUIRE(bvRx.CopyAndLoadBundle(encryptedBundle.data(), encryptedBundle.size()));
        BpSecBundleProcessor::ReturnResult res;
        BOOST_REQUIRE(bpSecPolicyManagerRx.ProcessReceivedBundle(bvRx, policyProcessingCtxRx, res, THIS_EID_FINAL_DEST.nodeId)); //bundle need NOT be dropped since it doesnt target payload
        BOOST_REQUIRE(res.errorCode == BpSecBundleProcessor::BPSEC_ERROR_CODES::CORRUPTED);
        BOOST_REQUIRE_EQUAL(bvRx.GetNumCanonicalBlocks(), 3);
        BOOST_REQUIRE(bvRx.RenderInPlace(PaddedMallocatorConstants::PADDING_ELEMENTS_BEFORE));
        BOOST_REQUIRE_EQUAL(bvRx.GetNumCanonicalBlocks(), 1); //SOp + Target removed per sopCorruptedAtAcceptor policy
        BOOST_REQUIRE(res.errorStringPtr);
        if (res.errorStringPtr) {
            BOOST_REQUIRE_EQUAL(*res.errorStringPtr, "unable to decrypt the target block number 2");
        }
    }

    { //simple confidentiality failure (corruption) which has a bad key at acceptor, drop bundle per failBundleForwarding policy
        //alter the key file (10.1 changes to 1.1)
        static const boost::regex regexMatch("ipn10.1_confidentiality.key");
        static const boost::regex regexMatch2("        1");
        std::string securityAcceptorPolicyBadJson = boost::regex_replace(securityAcceptorPolicyJson, regexMatch, "ipn1.1_confidentiality.key");
        securityAcceptorPolicyBadJson = boost::regex_replace(securityAcceptorPolicyBadJson, regexMatch2, "        4");
        static const boost::regex regexMatch3("removeSecurityOperation");
        securityAcceptorPolicyBadJson = boost::regex_replace(securityAcceptorPolicyBadJson, regexMatch3,
            "failBundleForwarding");

        //security acceptor read config and decrypt bundle
        BpSecConfig_ptr bpSecConfigPtrRx = BpSecConfig::CreateFromJson(securityAcceptorPolicyBadJson);
        BOOST_REQUIRE(bpSecConfigPtrRx);
        BpSecPolicyManager bpSecPolicyManagerRx;
        BpSecPolicyProcessingContext policyProcessingCtxRx;
        BOOST_REQUIRE(bpSecPolicyManagerRx.LoadFromConfig(*bpSecConfigPtrRx));
        BOOST_REQUIRE(bpSecPolicyManagerRx.FindPolicy(THIS_EID_SECURITY_SOURCE, cbhe_eid_t(1, 1), THIS_EID_FINAL_DEST, BPSEC_ROLE::ACCEPTOR));
        BundleViewV7 bvRx;
        BOOST_REQUIRE(bvRx.CopyAndLoadBundle(encryptedBundle.data(), encryptedBundle.size()));
        BpSecBundleProcessor::ReturnResult res;
        BOOST_REQUIRE(!bpSecPolicyManagerRx.ProcessReceivedBundle(bvRx, policyProcessingCtxRx, res, THIS_EID_FINAL_DEST.nodeId)); //drop bundle per failBundleForwarding policy
        BOOST_REQUIRE(res.errorCode == BpSecBundleProcessor::BPSEC_ERROR_CODES::CORRUPTED);
        BOOST_REQUIRE(res.errorStringPtr);
        if (res.errorStringPtr) {
            BOOST_REQUIRE_EQUAL(*res.errorStringPtr, "unable to decrypt the target block number 2");
        }
    }

    { //simple confidentiality failure (missing at acceptor) which has the wrong security source, add an sopMissingAtAcceptor policy
        static const boost::regex regexMatch("ipn:10.1");
        std::string securityAcceptorPolicyBadJson = boost::regex_replace(securityAcceptorPolicyJson, regexMatch, "ipn:20.1");
        static const boost::regex regexMatch3("sopCorruptedAtAcceptor");
        securityAcceptorPolicyBadJson = boost::regex_replace(securityAcceptorPolicyBadJson, regexMatch3,
            "sopMissingAtAcceptor");
        static const boost::regex regexMatch4("removeSecurityOperation"); //prohibited operation for missing at acceptor
        securityAcceptorPolicyBadJson = boost::regex_replace(securityAcceptorPolicyBadJson, regexMatch4,
            "removeSecurityOperationTargetBlock");

        //security acceptor read config and decrypt bundle
        BpSecConfig_ptr bpSecConfigPtrRx = BpSecConfig::CreateFromJson(securityAcceptorPolicyBadJson);
        BOOST_REQUIRE(bpSecConfigPtrRx);
        BpSecPolicyManager bpSecPolicyManagerRx;
        BpSecPolicyProcessingContext policyProcessingCtxRx;
        BOOST_REQUIRE(bpSecPolicyManagerRx.LoadFromConfig(*bpSecConfigPtrRx));
        BOOST_REQUIRE(bpSecPolicyManagerRx.FindPolicy(cbhe_eid_t(20, 1), cbhe_eid_t(1, 1), THIS_EID_FINAL_DEST, BPSEC_ROLE::ACCEPTOR));
        { //acceptor node id matches bundle final dest
            BundleViewV7 bvRx;
            BOOST_REQUIRE(bvRx.CopyAndLoadBundle(encryptedBundle.data(), encryptedBundle.size()));
            BpSecBundleProcessor::ReturnResult res;
            BOOST_REQUIRE(bpSecPolicyManagerRx.ProcessReceivedBundle(bvRx, policyProcessingCtxRx, res, THIS_EID_FINAL_DEST.nodeId)); //bundle need NOT be dropped
            BOOST_REQUIRE(res.errorCode == BpSecBundleProcessor::BPSEC_ERROR_CODES::MISSING);
            BOOST_REQUIRE(res.errorStringPtr);
            if (res.errorStringPtr) {
                BOOST_REQUIRE_EQUAL(*res.errorStringPtr,
                    "Bundle is at final destination but an acceptor policy could not be found for "
                    "BCB with securitySource=ipn:10.1,bundleSource=ipn:1.1,bundleFinalDest=ipn:2.1"
                );
            }
            BOOST_REQUIRE_EQUAL(bvRx.GetNumCanonicalBlocks(), 3);
            BOOST_REQUIRE(bvRx.RenderInPlace(PaddedMallocatorConstants::PADDING_ELEMENTS_BEFORE));
            BOOST_REQUIRE_EQUAL(bvRx.GetNumCanonicalBlocks(), 2); //SOp removed per sopMissingAtAcceptor policy
        }
    }
}

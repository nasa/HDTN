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

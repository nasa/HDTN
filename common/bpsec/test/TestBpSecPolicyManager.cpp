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
    { //bad syntax
        BpSecPolicyManager m;
        BOOST_REQUIRE(m.CreateAndGetNewPolicy("ipn:**.*", "ipn:*.*", "ipn:*.*", BPSEC_ROLE::ACCEPTOR) == NULL);
        BOOST_REQUIRE(m.CreateAndGetNewPolicy("ipn:*.*", "ipn:*.**", "ipn:*.*", BPSEC_ROLE::ACCEPTOR) == NULL);
        BOOST_REQUIRE(m.CreateAndGetNewPolicy("ipn:*.*", "ipn:*.*", "ipn:***.*", BPSEC_ROLE::ACCEPTOR) == NULL);
        BOOST_REQUIRE(m.CreateAndGetNewPolicy("ipn:*.*", "ipn:*.*", "ipn:*.*", BPSEC_ROLE::RESERVED_MAX_ROLE_TYPES) == NULL);
    }
    { //duplication
        BpSecPolicyManager m;
        BOOST_REQUIRE(m.CreateAndGetNewPolicy("ipn:*.*", "ipn:*.*", "ipn:*.*", BPSEC_ROLE::ACCEPTOR));
        BOOST_REQUIRE(m.CreateAndGetNewPolicy("ipn:*.*", "ipn:*.*", "ipn:*.*", BPSEC_ROLE::ACCEPTOR) == NULL);
        BOOST_REQUIRE(m.CreateAndGetNewPolicy("ipn:*.*", "ipn:*.*", "ipn:*.*", BPSEC_ROLE::ACCEPTOR) == NULL);
        BOOST_REQUIRE(m.CreateAndGetNewPolicy("ipn:*.*", "ipn:*.*", "ipn:*.*", BPSEC_ROLE::SOURCE));
        BOOST_REQUIRE(m.CreateAndGetNewPolicy("ipn:*.*", "ipn:*.*", "ipn:*.*", BPSEC_ROLE::SOURCE) == NULL);
        BOOST_REQUIRE(m.CreateAndGetNewPolicy("ipn:*.*", "ipn:*.*", "ipn:*.*", BPSEC_ROLE::VERIFIER));
        BOOST_REQUIRE(m.CreateAndGetNewPolicy("ipn:*.*", "ipn:*.*", "ipn:*.*", BPSEC_ROLE::VERIFIER) == NULL);
    }
    { //create and find
        BpSecPolicyManager m;
        const cbhe_eid_t ss(1, 1), bs(2, 1), bd(3, 1);
        BOOST_REQUIRE(m.FindPolicy(ss, bs, bd, BPSEC_ROLE::ACCEPTOR) == NULL);
        BOOST_REQUIRE(m.FindPolicy(ss, bs, bd, BPSEC_ROLE::ACCEPTOR) == NULL);
        BOOST_REQUIRE(m.CreateAndGetNewPolicy("ipn:*.*", "ipn:*.*", "ipn:*.*", BPSEC_ROLE::ACCEPTOR));
        const BpSecPolicy* policyAny = m.FindPolicy(ss, bs, bd, BPSEC_ROLE::ACCEPTOR);
        BOOST_REQUIRE(policyAny);
        BOOST_REQUIRE(m.FindPolicy(ss, bs, bd, BPSEC_ROLE::ACCEPTOR) == policyAny);

        {
            const BpSecPolicy* pNew = m.CreateAndGetNewPolicy("ipn:1.1", "ipn:*.*", "ipn:*.*", BPSEC_ROLE::ACCEPTOR);
            BOOST_REQUIRE(pNew);
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
            const BpSecPolicy* pNew = m.CreateAndGetNewPolicy(testCase[0], testCase[1], testCase[2], BPSEC_ROLE::ACCEPTOR);
            BOOST_REQUIRE(pNew);
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
        const cbhe_eid_t ss(1, 1), bs(2, 1), bd(3, 1);
        bool wasCacheHit;
        BOOST_REQUIRE(m.CreateAndGetNewPolicy("ipn:*.*", "ipn:*.*", "ipn:*.*", BPSEC_ROLE::ACCEPTOR));
        const BpSecPolicy* policyAny = m.FindPolicyWithThreadLocalCacheSupport(ss, bs, bd, BPSEC_ROLE::ACCEPTOR, wasCacheHit);
        BOOST_REQUIRE(policyAny);
        BOOST_REQUIRE(!wasCacheHit);
        BOOST_REQUIRE(m.FindPolicyWithThreadLocalCacheSupport(ss, bs, bd, BPSEC_ROLE::ACCEPTOR, wasCacheHit) == policyAny);
        BOOST_REQUIRE(wasCacheHit);
    }
}

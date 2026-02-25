/**
 * @file TestLtpSessionRecreationPreventer.cpp
 * @author  Brian Tomko <brian.j.tomko@nasa.gov>
 *
 * @copyright Copyright (c) 2026 United States Government as represented by
 * the National Aeronautics and Space Administration.
 * No copyright is claimed in the United States under Title 17, U.S.Code.
 * All Other Rights Reserved.
 *
 * @section LICENSE
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <boost/test/unit_test.hpp>
#include "LtpSessionRecreationPreventer.h"

BOOST_AUTO_TEST_CASE(LtpSessionRecreationPreventerTestCase)
{
    {
        const uint64_t maxSessions = 1000;
        LtpSessionRecreationPreventer srp(maxSessions);
        for (uint64_t i = 0; i < maxSessions; ++i) {
            BOOST_REQUIRE(!srp.ContainsSession(i));
            BOOST_REQUIRE(srp.AddSession(i));
            BOOST_REQUIRE(srp.ContainsSession(i));
            BOOST_REQUIRE(!srp.AddSession(i));
        }
        for (uint64_t i = 0; i < maxSessions; ++i) {
            BOOST_REQUIRE(srp.ContainsSession(i));
            BOOST_REQUIRE(!srp.AddSession(i));
            BOOST_REQUIRE(srp.ContainsSession(i));
            BOOST_REQUIRE(!srp.AddSession(i));
        }
        for (uint64_t i = 0; i < maxSessions; ++i) {
            const uint64_t newSession = i + maxSessions;
            BOOST_REQUIRE(srp.ContainsSession(i));
            BOOST_REQUIRE(!srp.ContainsSession(newSession));
            BOOST_REQUIRE(srp.AddSession(newSession));
            BOOST_REQUIRE(!srp.ContainsSession(i));
            BOOST_REQUIRE(srp.ContainsSession(newSession));
        }
        for (uint64_t i = 0; i < maxSessions; ++i) {
            BOOST_REQUIRE(!srp.ContainsSession(i));
            BOOST_REQUIRE(srp.AddSession(i));
            BOOST_REQUIRE(srp.ContainsSession(i));
            BOOST_REQUIRE(!srp.AddSession(i));
        }
        
    }

   
}

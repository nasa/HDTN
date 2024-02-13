/**
 * @file TestForwardListQueue.cpp
 * @author  Brian Tomko <brian.j.tomko@nasa.gov>
 *
 * @copyright Copyright (c) 2021 United States Government as represented by
 * the National Aeronautics and Space Administration.
 * No copyright is claimed in the United States under Title 17, U.S.Code.
 * All Other Rights Reserved.
 *
 * @section LICENSE
 * Released under the NASA Open Source Agreement (NOSA)
 * See LICENSE.md in the source root directory for more information.
 */

#include <boost/test/unit_test.hpp>
#include "ForwardListQueue.h"
#include <string>
#include "Logger.h"
#include <vector>



BOOST_AUTO_TEST_CASE(ForwardListQueueTestCase)
{
    typedef ForwardListQueue<std::string> flq_t;
    {
        flq_t flq;
        BOOST_REQUIRE(flq.empty());

        {
            std::string s1("1");
            //char* s1Ptr = s1.data();
            flq.emplace_back(std::move(s1));
            BOOST_REQUIRE(!flq.empty());
            BOOST_REQUIRE(s1.empty()); //success moved
            //BOOST_REQUIRE(flq.front().data() == s1Ptr); //invalid move test due to  small/short string optimization
            const flq_t expected({ "1" });
            BOOST_REQUIRE(flq == expected);
            BOOST_REQUIRE(!(flq != expected));
            BOOST_REQUIRE(flq.GetUnderlyingForwardListRef() == expected.GetUnderlyingForwardListRef());
        }
        {
            flq.emplace_back("2");
            const flq_t expected({ "1", "2"});
            BOOST_REQUIRE(flq == expected);
        }
        {
            flq.push_back("3");
            const flq_t expected({ "1", "2", "3"});
            BOOST_REQUIRE(flq == expected);
        }
        {
            std::string s4("4");
            flq.push_back(std::move(s4));
            BOOST_REQUIRE(s4.empty()); //success moved
            const flq_t expected({ "1", "2", "3", "4" });
            BOOST_REQUIRE(flq == expected);
            BOOST_REQUIRE(!(flq != expected));
            const flq_t unexpected({ "1", "2", "3", "5" });
            BOOST_REQUIRE(!(flq == unexpected));
            BOOST_REQUIRE(flq != unexpected);
        }
        {
            flq.emplace_front("0");
            const flq_t expected({ "0", "1", "2", "3", "4" });
            BOOST_REQUIRE(flq == expected);
        }
        {
            flq.push_back("5");
            const flq_t expected({ "0", "1", "2", "3", "4", "5"});
            BOOST_REQUIRE(flq == expected);
        }
        {
            std::string sn1("-1");
            flq.push_front(std::move(sn1));
            BOOST_REQUIRE(sn1.empty()); //success moved
            const flq_t expected({ "-1", "0", "1", "2", "3", "4", "5" });
            BOOST_REQUIRE(flq == expected);
            BOOST_REQUIRE(!(flq != expected));
        }
        {
            flq.push_back("6");
            const flq_t expected({ "-1", "0", "1", "2", "3", "4", "5", "6"});
            BOOST_REQUIRE(flq == expected);
        }
        {
            std::string sn2("-2");
            flq.push_front(std::move(sn2));
            BOOST_REQUIRE(sn2.empty()); //success moved
            const flq_t expected({ "-2", "-1", "0", "1", "2", "3", "4", "5", "6"});
            BOOST_REQUIRE(flq == expected);
            BOOST_REQUIRE(!(flq != expected));
        }
        {
            flq.push_back("7");
            const flq_t expected({ "-2", "-1", "0", "1", "2", "3", "4", "5", "6", "7"});
            BOOST_REQUIRE(flq == expected);
        }
        {
            flq.pop();
            const flq_t expected({ "-1", "0", "1", "2", "3", "4", "5", "6", "7" });
            BOOST_REQUIRE(flq == expected);
        }
        {
            flq.push_back("8");
            const flq_t expected({ "-1", "0", "1", "2", "3", "4", "5", "6", "7", "8"});
            BOOST_REQUIRE(flq == expected);
        }
        {
            BOOST_REQUIRE_EQUAL(flq.front(), "-1");
            BOOST_REQUIRE_EQUAL(flq.back(), "8");
        }
        {
            flq.pop();
            const flq_t expected({ "0", "1", "2", "3", "4", "5", "6", "7", "8" });
            BOOST_REQUIRE(flq == expected);
        }
        {
            flq.pop();
            const flq_t expected({ "1", "2", "3", "4", "5", "6", "7", "8" });
            BOOST_REQUIRE(flq == expected);
        }
        {
            flq.pop();
            const flq_t expected({ "2", "3", "4", "5", "6", "7", "8" });
            BOOST_REQUIRE(flq == expected);
        }
        {
            flq.pop();
            const flq_t expected({ "3", "4", "5", "6", "7", "8" });
            BOOST_REQUIRE(flq == expected);
        }
        {
            flq.pop();
            const flq_t expected({ "4", "5", "6", "7", "8" });
            BOOST_REQUIRE(flq == expected);
        }
        {
            flq.push_front("-3");
            const flq_t expected({ "-3", "4", "5", "6", "7", "8" });
            BOOST_REQUIRE(flq == expected);
        }
        {
            flq.pop();
            const flq_t expected({ "4", "5", "6", "7", "8" });
            BOOST_REQUIRE(flq == expected);
        }
        {
            flq.pop();
            const flq_t expected({ "5", "6", "7", "8" });
            BOOST_REQUIRE(flq == expected);
        }
        {
            flq.pop();
            const flq_t expected({ "6", "7", "8" });
            BOOST_REQUIRE(flq == expected);
        }
        {
            flq.pop();
            const flq_t expected({ "7", "8" });
            BOOST_REQUIRE(flq == expected);
        }
        {
            flq.pop();
            const flq_t expected({ "8" });
            BOOST_REQUIRE(flq == expected);
            BOOST_REQUIRE(!flq.empty());
        }
        {
            std::string sanityCheckString( 5, '9');
            BOOST_REQUIRE_EQUAL(sanityCheckString, "99999");
            flq.emplace_back(5, '9');
            BOOST_REQUIRE_EQUAL(flq.back(), "99999");
            const flq_t expected({ "8", "99999" });
            BOOST_REQUIRE(flq == expected);
        }
        {
            flq.pop();
            const flq_t expected({ "99999" });
            BOOST_REQUIRE(flq == expected);
            BOOST_REQUIRE(!flq.empty());
        }
        {
            flq.emplace_front(5, 'a');
            const flq_t expected({ "aaaaa", "99999" });
            BOOST_REQUIRE(flq == expected);
        }
        {
            flq.pop();
            const flq_t expected({ "99999" });
            BOOST_REQUIRE(flq == expected);
            BOOST_REQUIRE(!flq.empty());
        }
        {
            flq.pop();
            const flq_t expected({ });
            BOOST_REQUIRE(flq == expected);
            BOOST_REQUIRE(flq.empty());
        }
        //start off with a "front" command
        {
            flq.push_front("-4");
            const flq_t expected({ "-4"});
            BOOST_REQUIRE(flq == expected);
        }
        {
            flq.push_back("9");
            const flq_t expected({ "-4", "9"});
            BOOST_REQUIRE(flq == expected);
        }
        {
            flq.pop();
            const flq_t expected({ "9" });
            BOOST_REQUIRE(flq == expected);
        }
        {
            flq.pop();
            const flq_t expected({ });
            BOOST_REQUIRE(flq == expected);
            BOOST_REQUIRE(flq.empty());
        }
    }

    { //test remove_by_key
        flq_t flq({ "0", "1", "2", "3", "4", "5", "6", "7", "8" });
        BOOST_REQUIRE(!flq.empty());
        BOOST_REQUIRE_EQUAL(flq.back(), "8");
        BOOST_REQUIRE_EQUAL(flq.front(), "0");

        {
            BOOST_REQUIRE(flq.remove_by_key("8"));
            const flq_t expected({ "0", "1", "2", "3", "4", "5", "6", "7" });
            BOOST_REQUIRE(flq == expected);
            BOOST_REQUIRE_EQUAL(flq.back(), "7");
            BOOST_REQUIRE_EQUAL(flq.front(), "0");
        }
        {
            BOOST_REQUIRE(flq.remove_by_key("7"));
            const flq_t expected({ "0", "1", "2", "3", "4", "5", "6" });
            BOOST_REQUIRE(flq == expected);
            BOOST_REQUIRE_EQUAL(flq.back(), "6");
            BOOST_REQUIRE_EQUAL(flq.front(), "0");
            //fail remove again
            BOOST_REQUIRE(!flq.remove_by_key("7"));
            BOOST_REQUIRE(flq == expected);
            BOOST_REQUIRE_EQUAL(flq.back(), "6");
            BOOST_REQUIRE_EQUAL(flq.front(), "0");
        }
        {
            BOOST_REQUIRE(flq.remove_by_key("0"));
            const flq_t expected({ "1", "2", "3", "4", "5", "6"});
            BOOST_REQUIRE(flq == expected);
            BOOST_REQUIRE_EQUAL(flq.back(), "6");
            BOOST_REQUIRE_EQUAL(flq.front(), "1");
        }
        {
            BOOST_REQUIRE(flq.remove_by_key("1"));
            const flq_t expected({ "2", "3", "4", "5", "6" });
            BOOST_REQUIRE(flq == expected);
            BOOST_REQUIRE_EQUAL(flq.back(), "6");
            BOOST_REQUIRE_EQUAL(flq.front(), "2");
        }
        {
            BOOST_REQUIRE(flq.remove_by_key("4"));
            const flq_t expected({ "2", "3", "5", "6" });
            BOOST_REQUIRE(flq == expected);
            BOOST_REQUIRE_EQUAL(flq.back(), "6");
            BOOST_REQUIRE_EQUAL(flq.front(), "2");
        }
        {
            BOOST_REQUIRE(flq.remove_by_key("3"));
            const flq_t expected({ "2", "5", "6" });
            BOOST_REQUIRE(flq == expected);
            BOOST_REQUIRE_EQUAL(flq.back(), "6");
            BOOST_REQUIRE_EQUAL(flq.front(), "2");
        }
        {
            BOOST_REQUIRE(flq.remove_by_key("6"));
            const flq_t expected({ "2", "5" });
            BOOST_REQUIRE(flq == expected);
            BOOST_REQUIRE_EQUAL(flq.back(), "5");
            BOOST_REQUIRE_EQUAL(flq.front(), "2");
        }
        {
            BOOST_REQUIRE(flq.remove_by_key("5"));
            const flq_t expected({ "2" });
            BOOST_REQUIRE(flq == expected);
            BOOST_REQUIRE_EQUAL(flq.back(), "2");
            BOOST_REQUIRE_EQUAL(flq.front(), "2");
        }
        {
            BOOST_REQUIRE(flq.remove_by_key("2"));
            const flq_t expected({});
            BOOST_REQUIRE(flq == expected);
            BOOST_REQUIRE(flq.empty());
            //fail remove again
            BOOST_REQUIRE(!flq.remove_by_key("2"));
            BOOST_REQUIRE(flq == expected);
            BOOST_REQUIRE(flq.empty());
        }
    }
}

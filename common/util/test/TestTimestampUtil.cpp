#include <boost/test/unit_test.hpp>
#include <boost/thread.hpp>
#include "TimestampUtil.h"
#include <iostream>
#include <string>
#include <inttypes.h>
#include <vector>

BOOST_AUTO_TEST_CASE(TimestampUtilTestCase)
{
    //PtimeUtcString
    {
        const std::string startingTimestampStr("2020-02-06T20:25:11.493000Z");
        //std::cout << "time in: " << startingTimestampStr << "\n";
        boost::posix_time::ptime pt;
        BOOST_REQUIRE(TimestampUtil::SetPtimeFromUtcTimestampString(startingTimestampStr, pt));
        const std::string timeOut = TimestampUtil::GetUtcTimestampStringFromPtime(pt, false);
        //std::cout << "time out: " << timeOut << "\n";
        BOOST_REQUIRE_EQUAL(startingTimestampStr, timeOut);
    }

    //Epoch
    {
        const std::string expectedTimestampStr("2000-01-01T00:00:00.000000Z");
        const std::string timestampStr = TimestampUtil::GetUtcTimestampStringFromPtime(TimestampUtil::GetRfc5050Epoch(), false);
        BOOST_REQUIRE_EQUAL(expectedTimestampStr, timestampStr);
        //std::cout << "time: " << timestampStr << "\n";
    }

    //Dtn Time
    {
        TimestampUtil::dtn_time_t t1 = TimestampUtil::GenerateDtnTimeNow();
        boost::this_thread::sleep(boost::posix_time::milliseconds(1));
        TimestampUtil::dtn_time_t t2 = TimestampUtil::GenerateDtnTimeNow();

        BOOST_REQUIRE_NE(t1, t2);
        BOOST_REQUIRE_LE(t2.secondsSinceStartOfYear2000 - t1.secondsSinceStartOfYear2000, 1U);
        BOOST_REQUIRE_NE(t2.nanosecondsSinceStartOfIndicatedSecond - t1.nanosecondsSinceStartOfIndicatedSecond, 1U);

        TimestampUtil::dtn_time_t tPrev;
        for (unsigned int i = 1; i < 1000000; ++i) {//599616050
            t1.SetZero();
            t2.SetZero();
            const boost::posix_time::ptime pt = boost::posix_time::ptime(boost::gregorian::date(2019, 1, 1)) + boost::posix_time::seconds(50) + boost::posix_time::microseconds(i);
            t1 = TimestampUtil::PtimeToDtnTime(pt);
            const boost::posix_time::ptime pt2 = TimestampUtil::DtnTimeToPtimeLossy(t1);
            t2 = TimestampUtil::PtimeToDtnTime(pt2);
            BOOST_REQUIRE_EQUAL(t1.secondsSinceStartOfYear2000, 599616050U);
            BOOST_REQUIRE_EQUAL(t2.secondsSinceStartOfYear2000, 599616050U);
            BOOST_REQUIRE_EQUAL(t1.nanosecondsSinceStartOfIndicatedSecond, (i * 1000));
            BOOST_REQUIRE_EQUAL(t2.nanosecondsSinceStartOfIndicatedSecond, (i * 1000));
            BOOST_REQUIRE_LT(tPrev, t1);

            tPrev = t1;
            if (i == (1000000 - 1)) {
                std::cout << t1 << "\n";
                std::cout << t2 << "\n";
            }
            //BOOST_REQUIRE(pt == pt2);
        }
    }

    //Dtn Time Serialization/Deserialization
    {
        TimestampUtil::dtn_time_t t1(1000, 65537);
        std::vector<uint8_t> serialization(TimestampUtil::dtn_time_t::MAX_BUFFER_SIZE);
        const uint64_t size = t1.Serialize(&serialization[0]);
        BOOST_REQUIRE_EQUAL(size, 5);
        TimestampUtil::dtn_time_t t2;
        uint8_t numBytesTakenToDecode;
        BOOST_REQUIRE(t2.Deserialize(serialization.data(), &numBytesTakenToDecode));
        BOOST_REQUIRE_EQUAL(numBytesTakenToDecode, 5);
        BOOST_REQUIRE_EQUAL(t1, t2);
    }
}


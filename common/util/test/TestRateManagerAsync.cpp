#include <boost/test/unit_test.hpp>
#include "RateManagerAsync.h"
#include <boost/bind.hpp>
#include <boost/timer/timer.hpp>

BOOST_AUTO_TEST_CASE(RateManagerAsyncTestCase, *boost::unit_test::disabled())
{
    struct Test {
        boost::asio::io_service m_ioService;
        boost::asio::io_service::work m_work;
        boost::thread m_ioServiceThread;
        

        uint64_t m_numCallbacks;
        
        
        Test() :
            m_work(m_ioService),
            m_ioServiceThread(boost::bind(&boost::asio::io_service::run, &m_ioService))
        {
            
        }

        ~Test() {
            m_ioService.stop();
            m_ioServiceThread.join();
        }

        void PacketsSentCallback() {
            ++m_numCallbacks;
            
        }

        void DoTest(const uint64_t packetSizeBytes, const uint64_t numPacketsToSend, uint64_t rateBitsPerSec, uint64_t maxPacketsBeingSent, double rateTolerance = 20.0) {
            RateManagerAsync rateManagerAsync(m_ioService, rateBitsPerSec, maxPacketsBeingSent);
            rateManagerAsync.SetPacketsSentCallback(boost::bind(&Test::PacketsSentCallback, this));
            m_numCallbacks = 0;
            boost::posix_time::ptime t1 = boost::posix_time::microsec_clock::universal_time();
            const uint64_t totalBytesToSend = packetSizeBytes * numPacketsToSend;
            const double totalBitsToSend = totalBytesToSend * 8.0;
            
            for (std::size_t i = 0; i < numPacketsToSend; ++i) {
                rateManagerAsync.WaitForAvailabilityToSendPacket_Blocking();
                BOOST_REQUIRE(rateManagerAsync.HasAvailabilityToSendPacket());
                BOOST_REQUIRE(rateManagerAsync.SignalNewPacketDequeuedForSend(packetSizeBytes));
                boost::asio::post(m_ioService, boost::bind(&RateManagerAsync::IoServiceThreadNotifyPacketSentCallback, &rateManagerAsync, packetSizeBytes));
            }
            rateManagerAsync.WaitForAllDequeuedPacketsToFullySend_Blocking();
            boost::posix_time::ptime t2 = boost::posix_time::microsec_clock::universal_time();
            boost::posix_time::time_duration diff = t2 - t1;
            const double rate = totalBitsToSend / (diff.total_microseconds() * 1e-6);
            BOOST_REQUIRE_CLOSE(rate, rateBitsPerSec, rateTolerance);  //rate error within 20%
            BOOST_REQUIRE_GE(m_numCallbacks, numPacketsToSend / maxPacketsBeingSent);
            BOOST_REQUIRE_LE(m_numCallbacks, numPacketsToSend);
            BOOST_REQUIRE_EQUAL(rateManagerAsync.GetTotalBytesCompletelySent(), totalBytesToSend);
            BOOST_REQUIRE_EQUAL(rateManagerAsync.GetTotalPacketsCompletelySent(), numPacketsToSend);
        }

        
    };

    Test t;
    t.DoTest(1500, 500, 5000000, 5);
    t.DoTest(15000, 50, 5000000, 5);
    t.DoTest(1500, 500, 50000000, 50);
    //t.DoTest(1500, 500, 500000000, 50, 60.0);
}

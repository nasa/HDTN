#include <boost/test/unit_test.hpp>
#include "LtpTimerManager.h"
#include <boost/bind.hpp>
#include <boost/timer/timer.hpp>
#include "Ltp.h"

BOOST_AUTO_TEST_CASE(LtpTimerManagerTestCase)
{
    struct Test {
        const boost::posix_time::time_duration ONE_WAY_LIGHT_TIME;
        const boost::posix_time::time_duration ONE_WAY_MARGIN_TIME;
        boost::asio::io_service m_ioService;
        LtpTimerManager<uint64_t> m_timerManager;

        uint64_t m_numCallbacks;
        std::vector<uint64_t> m_desired_serialNumbers;
        std::vector<uint64_t> m_serialNumbersInCallback;
        int m_testNumber;

        Test() :
            ONE_WAY_LIGHT_TIME(boost::posix_time::milliseconds(100)),
            ONE_WAY_MARGIN_TIME(boost::posix_time::milliseconds(100)),
            m_timerManager(m_ioService, ONE_WAY_LIGHT_TIME, ONE_WAY_MARGIN_TIME, boost::bind(&Test::LtpTimerExpiredCallback, this, boost::placeholders::_1, boost::placeholders::_2))
        {
            
        }

        void LtpTimerExpiredCallback(uint64_t serialNumber, std::vector<uint8_t> & userData) {
            if (m_testNumber == 1) {
                BOOST_REQUIRE_EQUAL(userData.size(), 0);
                m_serialNumbersInCallback.push_back(serialNumber);
                ++m_numCallbacks;
            }
            else if (m_testNumber = 2) {
                BOOST_REQUIRE(userData == std::vector<uint8_t>({ 1,2,3 }));
                m_serialNumbersInCallback.push_back(serialNumber);
                //std::cout << "sn " << serialNumber << std::endl;
                ++m_numCallbacks;
                if (m_numCallbacks <= 3) {
                    BOOST_REQUIRE(m_timerManager.StartTimer(serialNumber, std::vector<uint8_t>({ 1,2,3 }))); //restart
                }
            }
        }

        void DoTest() {
            m_testNumber = 1;
            m_timerManager.Reset();
            m_ioService.stop();
            m_ioService.reset();
            m_numCallbacks = 0;
            m_serialNumbersInCallback.clear();
            m_desired_serialNumbers = std::vector<uint64_t>({ 5,10,15 });
            for (std::size_t i = 0; i < m_desired_serialNumbers.size(); ++i) {
                BOOST_REQUIRE(m_timerManager.StartTimer(m_desired_serialNumbers[i]));
            }
            m_ioService.run();
            
            BOOST_REQUIRE_EQUAL(m_numCallbacks, m_desired_serialNumbers.size());
            BOOST_REQUIRE(m_desired_serialNumbers == m_serialNumbersInCallback);
        }

        void DoTest2() { //readd serial numbers once
            m_testNumber = 2;
            m_timerManager.Reset();
            m_ioService.stop();
            m_ioService.reset();
            m_numCallbacks = 0;
            m_serialNumbersInCallback.clear();
            m_desired_serialNumbers = std::vector<uint64_t>({ 5,10,15 });
            for (std::size_t i = 0; i < m_desired_serialNumbers.size(); ++i) {
                BOOST_REQUIRE(m_timerManager.StartTimer(m_desired_serialNumbers[i], std::vector<uint8_t>({ 1,2,3 })));
            }
            m_ioService.run();

            BOOST_REQUIRE_EQUAL(m_numCallbacks, 6);
            BOOST_REQUIRE(m_serialNumbersInCallback == std::vector<uint64_t>({ 5,10,15,5,10,15 }));
        }

        void Test3TimerExpired(/*const boost::system::error_code& e*/) {
            BOOST_REQUIRE(m_timerManager.DeleteTimer(5)); //keep this call within the ioservice thread
        }
        void DoTest3() { //delete an active timer
            m_testNumber = 1;// 1 is valid
            m_timerManager.Reset();
            m_ioService.stop();
            m_ioService.reset();
            m_numCallbacks = 0;
            m_serialNumbersInCallback.clear();
            m_desired_serialNumbers = std::vector<uint64_t>({ 5,10,15 });
            for (std::size_t i = 0; i < m_desired_serialNumbers.size(); ++i) {
                BOOST_REQUIRE(m_timerManager.StartTimer(m_desired_serialNumbers[i]));
            }
            //boost::asio::deadline_timer dt(m_ioService);
            //dt.expires_from_now(boost::posix_time::milliseconds(0));
            //dt.async_wait(boost::bind(&Test::Test3TimerExpired, this, boost::asio::placeholders::error));
            boost::asio::post(m_ioService, boost::bind(&Test::Test3TimerExpired, this));
            m_ioService.run();

            BOOST_REQUIRE_EQUAL(m_numCallbacks, 2);
            BOOST_REQUIRE(m_serialNumbersInCallback == std::vector<uint64_t>({ 10,15 }));
        }
        void Test4TimerExpired(/*const boost::system::error_code& e*/) {
            BOOST_REQUIRE(m_timerManager.DeleteTimer(10)); //keep this call within the ioservice thread
        }
        void DoTest4() { //delete a non-active timer
            m_testNumber = 1;// 1 is valid
            m_timerManager.Reset();
            m_ioService.stop();
            m_ioService.reset();
            m_numCallbacks = 0;
            m_serialNumbersInCallback.clear();
            m_desired_serialNumbers = std::vector<uint64_t>({ 5,10,15 });
            for (std::size_t i = 0; i < m_desired_serialNumbers.size(); ++i) {
                BOOST_REQUIRE(m_timerManager.StartTimer(m_desired_serialNumbers[i]));
            }
            //boost::asio::deadline_timer dt(m_ioService);
            //dt.expires_from_now(boost::posix_time::milliseconds(0));
            //dt.async_wait(boost::bind(&Test::Test4TimerExpired, this, boost::asio::placeholders::error));
            boost::asio::post(m_ioService, boost::bind(&Test::Test4TimerExpired, this));
            m_ioService.run();

            BOOST_REQUIRE_EQUAL(m_numCallbacks, 2);
            BOOST_REQUIRE(m_serialNumbersInCallback == std::vector<uint64_t>({ 5,15 }));
        }
    };
    
    Test t;
    t.DoTest();
    t.DoTest2();
    t.DoTest3();
    t.DoTest4();

    

    struct TestWithSessionId {
        const boost::posix_time::time_duration ONE_WAY_LIGHT_TIME;
        const boost::posix_time::time_duration ONE_WAY_MARGIN_TIME;
        boost::asio::io_service m_ioService;
        LtpTimerManager<Ltp::session_id_t> m_timerManager;

        uint64_t m_numCallbacks;
        std::vector<Ltp::session_id_t> m_desired_serialNumbers;
        std::vector<Ltp::session_id_t> m_serialNumbersInCallback;
        int m_testNumber;

        TestWithSessionId() :
            ONE_WAY_LIGHT_TIME(boost::posix_time::milliseconds(100)),
            ONE_WAY_MARGIN_TIME(boost::posix_time::milliseconds(100)),
            m_timerManager(m_ioService, ONE_WAY_LIGHT_TIME, ONE_WAY_MARGIN_TIME, boost::bind(&TestWithSessionId::LtpTimerExpiredCallback, this, boost::placeholders::_1, boost::placeholders::_2))
        {

        }

        void LtpTimerExpiredCallback(Ltp::session_id_t serialNumber, std::vector<uint8_t> & userData) {
            if (m_testNumber == 1) {
                BOOST_REQUIRE_EQUAL(userData.size(), 0);
                m_serialNumbersInCallback.push_back(serialNumber);
                ++m_numCallbacks;
            }
            else if (m_testNumber = 2) {
                BOOST_REQUIRE(userData == std::vector<uint8_t>({ 1,2,3 }));
                m_serialNumbersInCallback.push_back(serialNumber);
                //std::cout << "sn " << serialNumber << std::endl;
                ++m_numCallbacks;
                if (m_numCallbacks <= 3) {
                    BOOST_REQUIRE(m_timerManager.StartTimer(serialNumber, std::vector<uint8_t>({ 1,2,3 }))); //restart
                }
            }
        }

        void DoTest() {
            m_testNumber = 1;
            m_timerManager.Reset();
            m_ioService.stop();
            m_ioService.reset();
            m_numCallbacks = 0;
            m_serialNumbersInCallback.clear();
            m_desired_serialNumbers = std::vector<Ltp::session_id_t>({ Ltp::session_id_t(5,6),Ltp::session_id_t(10,11),Ltp::session_id_t(15,16) });
            for (std::size_t i = 0; i < m_desired_serialNumbers.size(); ++i) {
                BOOST_REQUIRE(m_timerManager.StartTimer(m_desired_serialNumbers[i]));
            }
            m_ioService.run();

            BOOST_REQUIRE_EQUAL(m_numCallbacks, m_desired_serialNumbers.size());
            BOOST_REQUIRE(m_desired_serialNumbers == m_serialNumbersInCallback);
        }

        void DoTest2() { //readd serial numbers once
            m_testNumber = 2;
            m_timerManager.Reset();
            m_ioService.stop();
            m_ioService.reset();
            m_numCallbacks = 0;
            m_serialNumbersInCallback.clear();
            m_desired_serialNumbers = std::vector<Ltp::session_id_t>({ Ltp::session_id_t(5,6),Ltp::session_id_t(10,11),Ltp::session_id_t(15,16) });
            for (std::size_t i = 0; i < m_desired_serialNumbers.size(); ++i) {
                BOOST_REQUIRE(m_timerManager.StartTimer(m_desired_serialNumbers[i], std::vector<uint8_t>({ 1,2,3 })));
            }
            m_ioService.run();

            BOOST_REQUIRE_EQUAL(m_numCallbacks, 6);
            BOOST_REQUIRE(m_serialNumbersInCallback == std::vector<Ltp::session_id_t>({
                Ltp::session_id_t(5,6),Ltp::session_id_t(10,11),Ltp::session_id_t(15,16),
                Ltp::session_id_t(5, 6), Ltp::session_id_t(10, 11), Ltp::session_id_t(15, 16)}));
        }

        void Test3TimerExpired(/*const boost::system::error_code& e*/) {
            BOOST_REQUIRE(m_timerManager.DeleteTimer(Ltp::session_id_t(5, 6))); //keep this call within the ioservice thread
        }
        void DoTest3() { //delete an active timer
            m_testNumber = 1;// 1 is valid
            m_timerManager.Reset();
            m_ioService.stop();
            m_ioService.reset();
            m_numCallbacks = 0;
            m_serialNumbersInCallback.clear();
            m_desired_serialNumbers = std::vector<Ltp::session_id_t>({ Ltp::session_id_t(5,6),Ltp::session_id_t(10,11),Ltp::session_id_t(15,16) });
            for (std::size_t i = 0; i < m_desired_serialNumbers.size(); ++i) {
                BOOST_REQUIRE(m_timerManager.StartTimer(m_desired_serialNumbers[i]));
            }
            //boost::asio::deadline_timer dt(m_ioService);
            //dt.expires_from_now(boost::posix_time::milliseconds(0));
            //dt.async_wait(boost::bind(&Test::Test3TimerExpired, this, boost::asio::placeholders::error));
            boost::asio::post(m_ioService, boost::bind(&TestWithSessionId::Test3TimerExpired, this));
            m_ioService.run();

            BOOST_REQUIRE_EQUAL(m_numCallbacks, 2);
            BOOST_REQUIRE(m_serialNumbersInCallback == std::vector<Ltp::session_id_t>({ Ltp::session_id_t(10,11),Ltp::session_id_t(15,16) }));
        }
        void Test4TimerExpired(/*const boost::system::error_code& e*/) {
            BOOST_REQUIRE(m_timerManager.DeleteTimer(Ltp::session_id_t(10,11))); //keep this call within the ioservice thread
        }
        void DoTest4() { //delete a non-active timer
            m_testNumber = 1;// 1 is valid
            m_timerManager.Reset();
            m_ioService.stop();
            m_ioService.reset();
            m_numCallbacks = 0;
            m_serialNumbersInCallback.clear();
            m_desired_serialNumbers = std::vector<Ltp::session_id_t>({ Ltp::session_id_t(5,6),Ltp::session_id_t(10,11),Ltp::session_id_t(15,16) });
            for (std::size_t i = 0; i < m_desired_serialNumbers.size(); ++i) {
                BOOST_REQUIRE(m_timerManager.StartTimer(m_desired_serialNumbers[i]));
            }
            //boost::asio::deadline_timer dt(m_ioService);
            //dt.expires_from_now(boost::posix_time::milliseconds(0));
            //dt.async_wait(boost::bind(&Test::Test4TimerExpired, this, boost::asio::placeholders::error));
            boost::asio::post(m_ioService, boost::bind(&TestWithSessionId::Test4TimerExpired, this));
            m_ioService.run();

            BOOST_REQUIRE_EQUAL(m_numCallbacks, 2);
            BOOST_REQUIRE(m_serialNumbersInCallback == std::vector<Ltp::session_id_t>({ Ltp::session_id_t(5,6),Ltp::session_id_t(15,16) }));
        }
    };

    TestWithSessionId t2;
    t2.DoTest();
    t2.DoTest2();
    t2.DoTest3();
    t2.DoTest4();
}

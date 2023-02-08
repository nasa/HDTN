/**
 * @file TestLtpTimerManager.cpp
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
#include "LtpTimerManager.h"
#include <boost/bind/bind.hpp>
#include <boost/timer/timer.hpp>
#include "Ltp.h"
#include <iostream>

BOOST_AUTO_TEST_CASE(LtpTimerManagerTestCase)
{
    std::cout << "-----BEGIN LtpTimerManagerTestCase-----\n";
    struct Test {
        const boost::posix_time::time_duration ONE_WAY_LIGHT_TIME;
        const boost::posix_time::time_duration ONE_WAY_MARGIN_TIME;
        boost::posix_time::time_duration m_transmissionToAckReceivedTime;
        boost::asio::io_service m_ioService;
        boost::asio::deadline_timer m_deadlineTimer;
        LtpTimerManager<uint64_t, std::hash<uint64_t> >::LtpTimerExpiredCallback_t m_timerExpiredCallback;
        LtpTimerManager<uint64_t, std::hash<uint64_t> > m_timerManager;

        uint64_t m_numCallbacks;
        std::vector<uint64_t> m_desired_serialNumbers;
        std::vector<uint64_t> m_serialNumbersInCallback;
        int m_testNumber;

        Test() :
            ONE_WAY_LIGHT_TIME(boost::posix_time::milliseconds(100)),
            ONE_WAY_MARGIN_TIME(boost::posix_time::milliseconds(100)),
            m_transmissionToAckReceivedTime((ONE_WAY_LIGHT_TIME * 2) + (ONE_WAY_MARGIN_TIME * 2)),
            m_deadlineTimer(m_ioService),
            m_timerManager(m_deadlineTimer, m_transmissionToAckReceivedTime, 100)
        {
            m_timerExpiredCallback = boost::bind(&Test::LtpTimerExpiredCallback, this, boost::placeholders::_1, boost::placeholders::_2, boost::placeholders::_3);
        }

        void LtpTimerExpiredCallback(void* classPtr, const uint64_t & serialNumber, std::vector<uint8_t> & userData) {
            BOOST_REQUIRE(classPtr == NULL);
            ++m_numCallbacks;
            m_serialNumbersInCallback.push_back(serialNumber);

            if (m_testNumber == 1) {
                BOOST_REQUIRE_EQUAL(userData.size(), 0);
            }
            else if (m_testNumber == 2) {
                BOOST_REQUIRE(userData == std::vector<uint8_t>({ 1,2,3 }));
                //std::cout << "sn " << serialNumber << std::endl;
                if (m_numCallbacks <= 3) {
                    BOOST_REQUIRE(m_timerManager.StartTimer(NULL, serialNumber, &m_timerExpiredCallback, std::vector<uint8_t>({ 1,2,3 }))); //restart
                }
            }
            else if (m_testNumber == 3) {
                BOOST_REQUIRE(userData == std::vector<uint8_t>({ 1,2,3 }));
            }
            else if (m_testNumber == 4) {
                BOOST_REQUIRE(userData == std::vector<uint8_t>({ 1,2,3 }));
            }
        }

        void DoTest() {
            BOOST_REQUIRE_EQUAL(m_timerManager.m_userDataRecycler.GetListCapacity(), 100); //initialized from constructor above
            m_testNumber = 1;
            m_timerManager.Reset();
            m_ioService.stop();
            m_ioService.reset();
            m_numCallbacks = 0;
            m_serialNumbersInCallback.clear();
            m_desired_serialNumbers = std::vector<uint64_t>({ 5,10,15 });
            for (std::size_t i = 0; i < m_desired_serialNumbers.size(); ++i) {
                BOOST_REQUIRE(m_timerManager.StartTimer(NULL, m_desired_serialNumbers[i], &m_timerExpiredCallback));
                BOOST_REQUIRE_EQUAL(m_timerManager.m_userDataRecycler.GetListSize(), 0); //no user data
            }
            m_ioService.run();
            
            BOOST_REQUIRE_EQUAL(m_timerManager.m_userDataRecycler.GetListSize(), 0); //size 0 user data not recycled
            BOOST_REQUIRE_EQUAL(m_numCallbacks, m_desired_serialNumbers.size());
            BOOST_REQUIRE(m_desired_serialNumbers == m_serialNumbersInCallback);
        }

        void DoTest2() { //read serial numbers once
            m_testNumber = 2;
            m_timerManager.Reset();
            m_ioService.stop();
            m_ioService.reset();
            m_numCallbacks = 0;
            m_serialNumbersInCallback.clear();
            m_desired_serialNumbers = std::vector<uint64_t>({ 5,10,15 });
            for (std::size_t i = 0; i < m_desired_serialNumbers.size(); ++i) {
                BOOST_REQUIRE(m_timerManager.StartTimer(NULL, m_desired_serialNumbers[i], &m_timerExpiredCallback, std::vector<uint8_t>({ 1,2,3 })));
                BOOST_REQUIRE_EQUAL(m_timerManager.m_userDataRecycler.GetListSize(), 0); //still no recycled user data
            }
            m_ioService.run();

            BOOST_REQUIRE_EQUAL(m_numCallbacks, 6);
            BOOST_REQUIRE_EQUAL(m_timerManager.m_userDataRecycler.GetListSize(), m_numCallbacks); //recycled user data
            BOOST_REQUIRE(m_serialNumbersInCallback == std::vector<uint64_t>({ 5,10,15,5,10,15 }));
        }

        void Test3TimerExpired(/*const boost::system::error_code& e*/) {
            BOOST_REQUIRE(m_timerManager.DeleteTimer(5)); //keep this call within the io_service thread
        }
        void DoTest3() { //delete an active timer
            m_testNumber = 3;
            m_timerManager.Reset();
            m_ioService.stop();
            m_ioService.reset();
            m_numCallbacks = 0;
            m_serialNumbersInCallback.clear();
            m_desired_serialNumbers = std::vector<uint64_t>({ 5,10,15 }); //keep same as test 2
            for (std::size_t i = 0; i < m_desired_serialNumbers.size(); ++i) {
                BOOST_REQUIRE_EQUAL(m_timerManager.m_userDataRecycler.GetListSize(), 6 - i); //6 is m_numCallbacks from test 2
                std::vector<uint8_t> userData;
                m_timerManager.m_userDataRecycler.GetRecycledOrCreateNewUserData(userData);
                BOOST_REQUIRE(userData == std::vector<uint8_t>({ 1,2,3 })); //recycled from test 2
                BOOST_REQUIRE(m_timerManager.StartTimer(NULL, m_desired_serialNumbers[i], &m_timerExpiredCallback, std::move(userData)));
                BOOST_REQUIRE(userData.empty()); //verify moved
            }
            BOOST_REQUIRE_EQUAL(m_timerManager.m_userDataRecycler.GetListSize(), 6 - m_desired_serialNumbers.size());
            //boost::asio::deadline_timer dt(m_ioService);
            //dt.expires_from_now(boost::posix_time::milliseconds(0));
            //dt.async_wait(boost::bind(&Test::Test3TimerExpired, this, boost::asio::placeholders::error));
            boost::asio::post(m_ioService, boost::bind(&Test::Test3TimerExpired, this));
            m_ioService.run();

            BOOST_REQUIRE_EQUAL(m_timerManager.m_userDataRecycler.GetListSize(), 6); //auto-recycled //6 is m_numCallbacks from test 2
            BOOST_REQUIRE_EQUAL(m_numCallbacks, 2);
            BOOST_REQUIRE(m_serialNumbersInCallback == std::vector<uint64_t>({ 10,15 }));
        }
        void Test4TimerExpired(/*const boost::system::error_code& e*/) {
            BOOST_REQUIRE(m_timerManager.DeleteTimer(10)); //keep this call within the io_service thread
        }
        void DoTest4() { //delete a non-active timer
            m_testNumber = 4;
            m_timerManager.Reset();
            m_ioService.stop();
            m_ioService.reset();
            m_numCallbacks = 0;
            m_serialNumbersInCallback.clear();
            m_desired_serialNumbers = std::vector<uint64_t>({ 5,10,15 }); //keep same as test 3
            for (std::size_t i = 0; i < m_desired_serialNumbers.size(); ++i) {
                BOOST_REQUIRE_EQUAL(m_timerManager.m_userDataRecycler.GetListSize(), 6 - i); //6 is m_numCallbacks from test 2
                std::vector<uint8_t> userData;
                m_timerManager.m_userDataRecycler.GetRecycledOrCreateNewUserData(userData);
                BOOST_REQUIRE(userData == std::vector<uint8_t>({ 1,2,3 })); //recycled from test 3
                BOOST_REQUIRE(m_timerManager.StartTimer(NULL, m_desired_serialNumbers[i], &m_timerExpiredCallback, std::move(userData)));
            }
            BOOST_REQUIRE_EQUAL(m_timerManager.m_userDataRecycler.GetListSize(), 6 - m_desired_serialNumbers.size());
            //boost::asio::deadline_timer dt(m_ioService);
            //dt.expires_from_now(boost::posix_time::milliseconds(0));
            //dt.async_wait(boost::bind(&Test::Test4TimerExpired, this, boost::asio::placeholders::error));
            boost::asio::post(m_ioService, boost::bind(&Test::Test4TimerExpired, this));
            m_ioService.run();

            BOOST_REQUIRE_EQUAL(m_timerManager.m_userDataRecycler.GetListSize(), 6); //auto-recycled //6 is m_numCallbacks from test 2
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
        boost::posix_time::time_duration m_transmissionToAckReceivedTime;
        boost::asio::io_service m_ioService;
        boost::asio::deadline_timer m_deadlineTimer;
        LtpTimerManager<Ltp::session_id_t, Ltp::hash_session_id_t>::LtpTimerExpiredCallback_t m_timerExpiredCallback;
        LtpTimerManager<Ltp::session_id_t, Ltp::hash_session_id_t> m_timerManager;

        uint64_t m_numCallbacks;
        std::vector<Ltp::session_id_t> m_desired_serialNumbers;
        std::vector<Ltp::session_id_t> m_serialNumbersInCallback;
        int m_testNumber;

        TestWithSessionId() :
            ONE_WAY_LIGHT_TIME(boost::posix_time::milliseconds(100)),
            ONE_WAY_MARGIN_TIME(boost::posix_time::milliseconds(100)),
            m_transmissionToAckReceivedTime((ONE_WAY_LIGHT_TIME * 2) + (ONE_WAY_MARGIN_TIME * 2)),
            m_deadlineTimer(m_ioService),
            m_timerManager(m_deadlineTimer, m_transmissionToAckReceivedTime, 100)
        {
            m_timerExpiredCallback = boost::bind(&TestWithSessionId::LtpTimerExpiredCallback, this, boost::placeholders::_1, boost::placeholders::_2, boost::placeholders::_3);
        }

        void LtpTimerExpiredCallback(void* classPtr, const Ltp::session_id_t & serialNumber, std::vector<uint8_t> & userData) {
            BOOST_REQUIRE(classPtr == NULL);
            if (m_testNumber == 1) {
                BOOST_REQUIRE_EQUAL(userData.size(), 0);
                m_serialNumbersInCallback.push_back(serialNumber);
                ++m_numCallbacks;
            }
            else if (m_testNumber == 2) {
                BOOST_REQUIRE(userData == std::vector<uint8_t>({ 1,2,3 }));
                m_serialNumbersInCallback.push_back(serialNumber);
                //std::cout << "sn " << serialNumber << std::endl;
                ++m_numCallbacks;
                if (m_numCallbacks <= 3) {
                    BOOST_REQUIRE(m_timerManager.StartTimer(NULL, serialNumber, &m_timerExpiredCallback, std::vector<uint8_t>({ 1,2,3 }))); //restart
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
                BOOST_REQUIRE(m_timerManager.StartTimer(NULL, m_desired_serialNumbers[i], &m_timerExpiredCallback));
            }
            m_ioService.run();

            BOOST_REQUIRE_EQUAL(m_numCallbacks, m_desired_serialNumbers.size());
            BOOST_REQUIRE(m_desired_serialNumbers == m_serialNumbersInCallback);
        }

        void DoTest2() { //read serial numbers once
            m_testNumber = 2;
            m_timerManager.Reset();
            m_ioService.stop();
            m_ioService.reset();
            m_numCallbacks = 0;
            m_serialNumbersInCallback.clear();
            m_desired_serialNumbers = std::vector<Ltp::session_id_t>({ Ltp::session_id_t(5,6),Ltp::session_id_t(10,11),Ltp::session_id_t(15,16) });
            for (std::size_t i = 0; i < m_desired_serialNumbers.size(); ++i) {
                BOOST_REQUIRE(m_timerManager.StartTimer(NULL, m_desired_serialNumbers[i], &m_timerExpiredCallback, std::vector<uint8_t>({ 1,2,3 })));
            }
            m_ioService.run();

            BOOST_REQUIRE_EQUAL(m_numCallbacks, 6);
            BOOST_REQUIRE(m_serialNumbersInCallback == std::vector<Ltp::session_id_t>({
                Ltp::session_id_t(5,6),Ltp::session_id_t(10,11),Ltp::session_id_t(15,16),
                Ltp::session_id_t(5, 6), Ltp::session_id_t(10, 11), Ltp::session_id_t(15, 16)}));
        }

        void Test3TimerExpired(/*const boost::system::error_code& e*/) {
            BOOST_REQUIRE(m_timerManager.DeleteTimer(Ltp::session_id_t(5, 6))); //keep this call within the io_service thread
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
                BOOST_REQUIRE(m_timerManager.StartTimer(NULL, m_desired_serialNumbers[i], &m_timerExpiredCallback));
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
            BOOST_REQUIRE(m_timerManager.DeleteTimer(Ltp::session_id_t(10,11))); //keep this call within the io_service thread
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
                BOOST_REQUIRE(m_timerManager.StartTimer(NULL, m_desired_serialNumbers[i], &m_timerExpiredCallback));
            }
            //boost::asio::deadline_timer dt(m_ioService);
            //dt.expires_from_now(boost::posix_time::milliseconds(0));
            //dt.async_wait(boost::bind(&Test::Test4TimerExpired, this, boost::asio::placeholders::error));
            boost::asio::post(m_ioService, boost::bind(&TestWithSessionId::Test4TimerExpired, this));
            m_ioService.run();

            BOOST_REQUIRE_EQUAL(m_numCallbacks, 2);
            BOOST_REQUIRE(m_serialNumbersInCallback == std::vector<Ltp::session_id_t>({ Ltp::session_id_t(5,6),Ltp::session_id_t(15,16) }));
        }

        void Test5ChangeTimeUp(/*const boost::system::error_code& e*/) {
            //change RTT from 400ms to 1000ms
            const boost::posix_time::time_duration oldTransmissionToAckReceivedTime = m_transmissionToAckReceivedTime;
            m_transmissionToAckReceivedTime = boost::posix_time::milliseconds(1000); //this variable is referenced by all timers, so new timers will use this new value
            const boost::posix_time::time_duration diffNewMinusOld = m_transmissionToAckReceivedTime - oldTransmissionToAckReceivedTime;
            const int64_t diffMs = diffNewMinusOld.total_milliseconds();
            BOOST_REQUIRE_GE(diffMs, 599); //1000 - 400
            BOOST_REQUIRE_LE(diffMs, 601);
            std::cout << "increase time by " << diffMs << " milliseconds\n";
            m_timerManager.AdjustRunningTimers(diffNewMinusOld);
        }
        void Test5ChangeTimeDown(/*const boost::system::error_code& e*/) {
            //change RTT from 1000ms to 400ms
            const boost::posix_time::time_duration oldTransmissionToAckReceivedTime = m_transmissionToAckReceivedTime;
            m_transmissionToAckReceivedTime = boost::posix_time::milliseconds(400); //this variable is referenced by all timers, so new timers will use this new value
            const boost::posix_time::time_duration diffNewMinusOld = m_transmissionToAckReceivedTime - oldTransmissionToAckReceivedTime;
            const int64_t diffMs = diffNewMinusOld.total_milliseconds();
            BOOST_REQUIRE_LE(diffMs, -599); //1000 - 400
            BOOST_REQUIRE_GE(diffMs, -601);
            std::cout << "increase time by " << diffMs << " milliseconds\n";
            m_timerManager.AdjustRunningTimers(diffNewMinusOld);
        }
        void DoTest5() { //change the time on running timers
            //first increase time
            m_testNumber = 1;
            m_timerManager.Reset();
            m_ioService.stop();
            m_ioService.reset();
            m_numCallbacks = 0;
            m_serialNumbersInCallback.clear();
            m_desired_serialNumbers = m_desired_serialNumbers = std::vector<Ltp::session_id_t>({ Ltp::session_id_t(5,6),Ltp::session_id_t(10,11),Ltp::session_id_t(15,16) });
            for (std::size_t i = 0; i < m_desired_serialNumbers.size(); ++i) {
                BOOST_REQUIRE(m_timerManager.StartTimer(NULL, m_desired_serialNumbers[i], &m_timerExpiredCallback));
            }
            boost::asio::post(m_ioService, boost::bind(&TestWithSessionId::Test5ChangeTimeUp, this));
            m_ioService.run();

            BOOST_REQUIRE_EQUAL(m_numCallbacks, m_desired_serialNumbers.size());
            BOOST_REQUIRE(m_desired_serialNumbers == m_serialNumbersInCallback);

            //now decrease time
            m_testNumber = 1;
            m_timerManager.Reset();
            m_ioService.stop();
            m_ioService.reset();
            m_numCallbacks = 0;
            m_serialNumbersInCallback.clear();
            m_desired_serialNumbers = m_desired_serialNumbers = std::vector<Ltp::session_id_t>({ Ltp::session_id_t(5,6),Ltp::session_id_t(10,11),Ltp::session_id_t(15,16) });
            for (std::size_t i = 0; i < m_desired_serialNumbers.size(); ++i) {
                BOOST_REQUIRE(m_timerManager.StartTimer(NULL, m_desired_serialNumbers[i], &m_timerExpiredCallback));
            }
            boost::asio::post(m_ioService, boost::bind(&TestWithSessionId::Test5ChangeTimeDown, this));
            m_ioService.run();

            BOOST_REQUIRE_EQUAL(m_numCallbacks, m_desired_serialNumbers.size());
            BOOST_REQUIRE(m_desired_serialNumbers == m_serialNumbersInCallback);
        }
    };

    TestWithSessionId t2;
    t2.DoTest();
    t2.DoTest2();
    t2.DoTest3();
    t2.DoTest4();
    t2.DoTest5();
    std::cout << "-----END LtpTimerManagerTestCase-----\n";
}


BOOST_AUTO_TEST_CASE(UserDataRecyclerTestCase)
{
    UserDataRecycler udr(5);
    BOOST_REQUIRE_EQUAL(udr.GetListSize(), 0);
    BOOST_REQUIRE_EQUAL(udr.GetListCapacity(), 5);
    { //try get data from empty recycler fail
        std::vector<uint8_t> udReturned;
       
        udr.GetRecycledOrCreateNewUserData(udReturned);
        BOOST_REQUIRE_EQUAL(udReturned.size(), 0);
        BOOST_REQUIRE_EQUAL(udReturned.capacity(), 0);
        //still unchanged
        BOOST_REQUIRE_EQUAL(udr.GetListSize(), 0);
        BOOST_REQUIRE_EQUAL(udr.GetListCapacity(), 5);
    }
    { //try add empty data and fail
        std::vector<uint8_t> ud, udReturned;
        BOOST_REQUIRE_EQUAL(ud.size(), 0);
        BOOST_REQUIRE_EQUAL(ud.capacity(), 0);
        BOOST_REQUIRE(!udr.ReturnUserData(std::move(ud))); //fail
        //no change
        BOOST_REQUIRE_EQUAL(udr.GetListSize(), 0);
        BOOST_REQUIRE_EQUAL(udr.GetListCapacity(), 5);

        udr.GetRecycledOrCreateNewUserData(udReturned);
        BOOST_REQUIRE_EQUAL(udReturned.size(), 0);
        BOOST_REQUIRE_EQUAL(udReturned.capacity(), 0);
        //still unchanged
        BOOST_REQUIRE_EQUAL(udr.GetListSize(), 0);
        BOOST_REQUIRE_EQUAL(udr.GetListCapacity(), 5);
    }
    { //add empty data but has reserved space and succeed
        std::vector<uint8_t> ud, udReturned;
        ud.reserve(100);
        BOOST_REQUIRE_EQUAL(ud.size(), 0);
        BOOST_REQUIRE_NE(ud.capacity(), 0);
        BOOST_REQUIRE_GE(ud.capacity(), 100);
        BOOST_REQUIRE(udr.ReturnUserData(std::move(ud))); //success
        //changed
        BOOST_REQUIRE_EQUAL(udr.GetListSize(), 1);
        BOOST_REQUIRE_EQUAL(udr.GetListCapacity(), 5);

        udr.GetRecycledOrCreateNewUserData(udReturned);
        BOOST_REQUIRE_EQUAL(udReturned.size(), 0);
        BOOST_REQUIRE_NE(udReturned.capacity(), 0);
        BOOST_REQUIRE_GE(udReturned.capacity(), 100);
        //decrease
        BOOST_REQUIRE_EQUAL(udr.GetListSize(), 0);
        BOOST_REQUIRE_EQUAL(udr.GetListCapacity(), 5);
    }
    { //add data with size() and succeed
        std::vector<uint8_t> ud, udReturned;
        ud.resize(100);
        BOOST_REQUIRE_EQUAL(ud.size(), 100);
        BOOST_REQUIRE_NE(ud.capacity(), 0);
        BOOST_REQUIRE_GE(ud.capacity(), 100);
        BOOST_REQUIRE(udr.ReturnUserData(std::move(ud))); //success
        //changed
        BOOST_REQUIRE_EQUAL(udr.GetListSize(), 1);
        BOOST_REQUIRE_EQUAL(udr.GetListCapacity(), 5);

        udr.GetRecycledOrCreateNewUserData(udReturned);
        BOOST_REQUIRE_EQUAL(udReturned.size(), 100);
        BOOST_REQUIRE_NE(udReturned.capacity(), 0);
        BOOST_REQUIRE_GE(udReturned.capacity(), 100);
        //decrease
        BOOST_REQUIRE_EQUAL(udr.GetListSize(), 0);
        BOOST_REQUIRE_EQUAL(udr.GetListCapacity(), 5);
    }
    for (unsigned int i = 0; i < 5; ++i) {
        //add data with size() and succeed
        std::vector<uint8_t> ud;
        ud.resize(100 + i);
        BOOST_REQUIRE_EQUAL(ud.size(), 100 + i);
        BOOST_REQUIRE_NE(ud.capacity(), 0);
        BOOST_REQUIRE_GE(ud.capacity(), 100 + i);
        BOOST_REQUIRE(udr.ReturnUserData(std::move(ud))); //success
        //changed
        BOOST_REQUIRE_EQUAL(udr.GetListSize(), i + 1);
        BOOST_REQUIRE_EQUAL(udr.GetListCapacity(), 5);
    }
    for (unsigned int i = 5; i < 10; ++i) {
        //add data with size() and fail because list full
        std::vector<uint8_t> ud;
        ud.resize(100 + i);
        BOOST_REQUIRE_EQUAL(ud.size(), 100 + i);
        BOOST_REQUIRE_NE(ud.capacity(), 0);
        BOOST_REQUIRE_GE(ud.capacity(), 100 + i);
        BOOST_REQUIRE(!udr.ReturnUserData(std::move(ud))); //fail
        //unchanged
        BOOST_REQUIRE_EQUAL(udr.GetListSize(), 5);
        BOOST_REQUIRE_EQUAL(udr.GetListCapacity(), 5);
    }
    for (int i = 4; i >= 0; --i) { //underlying forward_list is FILO
        //get data with size() and succeed
        std::vector<uint8_t> udReturned;
        udr.GetRecycledOrCreateNewUserData(udReturned);
        BOOST_REQUIRE_EQUAL(udReturned.size(), 100 + i);
        BOOST_REQUIRE_NE(udReturned.capacity(), 0);
        BOOST_REQUIRE_GE(udReturned.capacity(), 100 + i);
        //decrease
        BOOST_REQUIRE_EQUAL(udr.GetListSize(), i);
        BOOST_REQUIRE_EQUAL(udr.GetListCapacity(), 5);
    }
    //verify empty
    BOOST_REQUIRE_EQUAL(udr.GetListSize(), 0);
    BOOST_REQUIRE_EQUAL(udr.GetListCapacity(), 5);
}


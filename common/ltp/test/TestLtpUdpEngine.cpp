#include <boost/test/unit_test.hpp>
#include "LtpUdpEngine.h"
#include <boost/bind.hpp>

BOOST_AUTO_TEST_CASE(LtpUdpEngineTestCase, *boost::unit_test::enabled())
{
    struct Test {
        const boost::posix_time::time_duration ONE_WAY_LIGHT_TIME;
        const boost::posix_time::time_duration ONE_WAY_MARGIN_TIME;
        const uint64_t ENGINE_ID_SRC;
        const uint64_t ENGINE_ID_DEST;
        const uint64_t CLIENT_SERVICE_ID_DEST;
        LtpUdpEngine engineSrc; 
        LtpUdpEngine engineDest;
        const std::string DESIRED_RED_DATA_TO_SEND;

        uint64_t numRedPartReceptionCallbacks;

        Test() :
            ONE_WAY_LIGHT_TIME(boost::posix_time::seconds(10)),
            ONE_WAY_MARGIN_TIME(boost::posix_time::milliseconds(2000)),
            ENGINE_ID_SRC(100),
            ENGINE_ID_DEST(200),
            CLIENT_SERVICE_ID_DEST(300),
            engineSrc(ENGINE_ID_SRC, 1, ONE_WAY_LIGHT_TIME, ONE_WAY_MARGIN_TIME, 0),//1=> 1 CHARACTER AT A TIME
            engineDest(ENGINE_ID_DEST, 1, ONE_WAY_LIGHT_TIME, ONE_WAY_MARGIN_TIME, 12345),//1=> MTU NOT USED AT THIS TIME
            DESIRED_RED_DATA_TO_SEND("The quick brown fox jumps over the lazy dog!")
        {
            engineDest.SetRedPartReceptionCallback(boost::bind(&Test::RedPartReceptionCallback, this, boost::placeholders::_1, boost::placeholders::_2, boost::placeholders::_3,
                boost::placeholders::_4, boost::placeholders::_5));
            engineSrc.Connect("localhost", "12345");
            while (!engineSrc.ReadyToForward()) {
                std::cout << "connecting\n";
                boost::this_thread::sleep(boost::posix_time::milliseconds(500));
            }
        }

        void RedPartReceptionCallback(const Ltp::session_id_t & sessionId, const std::vector<uint8_t> & clientServiceDataVec, uint64_t lengthOfRedPart, uint64_t clientServiceId, bool isEndOfBlock) {
            std::string receivedMessage(clientServiceDataVec.data(), clientServiceDataVec.data() + clientServiceDataVec.size());
            ++numRedPartReceptionCallbacks;
            BOOST_REQUIRE_EQUAL(receivedMessage, DESIRED_RED_DATA_TO_SEND);
            //std::cout << "receivedMessage: " << receivedMessage << std::endl;
            //std::cout << "here\n";
        }
        
        void DoTest() {
            engineSrc.Reset();
            engineDest.Reset();
            numRedPartReceptionCallbacks = 0;
            boost::shared_ptr<LtpEngine::transmission_request_t> tReq = boost::make_shared<LtpEngine::transmission_request_t>();
            tReq->destinationClientServiceId = CLIENT_SERVICE_ID_DEST;
            tReq->destinationLtpEngineId = ENGINE_ID_DEST;
            tReq->clientServiceDataToSend = std::vector<uint8_t>(DESIRED_RED_DATA_TO_SEND.data(), DESIRED_RED_DATA_TO_SEND.data() + DESIRED_RED_DATA_TO_SEND.size()); //copy
            tReq->lengthOfRedPart = DESIRED_RED_DATA_TO_SEND.size();
            engineSrc.TransmissionRequest_ThreadSafe(std::move(tReq));
            boost::this_thread::sleep(boost::posix_time::seconds(5));
            //std::cout << "numSrcToDestDataExchanged " << numSrcToDestDataExchanged << " numDestToSrcDataExchanged " << numDestToSrcDataExchanged << " DESIRED_RED_DATA_TO_SEND.size() " << DESIRED_RED_DATA_TO_SEND.size() << std::endl;
            
            BOOST_REQUIRE_EQUAL(numRedPartReceptionCallbacks, 1);

            BOOST_REQUIRE_EQUAL(engineSrc.m_countAsyncSendCallbackCalls, DESIRED_RED_DATA_TO_SEND.size() + 1); //+1 for Report ack
            BOOST_REQUIRE_EQUAL(engineSrc.m_countAsyncSendCallbackCalls, engineSrc.m_countAsyncSendCalls);
            BOOST_REQUIRE_EQUAL(engineDest.m_countAsyncSendCallbackCalls, 1); //1 for Report segment
            BOOST_REQUIRE_EQUAL(engineDest.m_countAsyncSendCallbackCalls, engineDest.m_countAsyncSendCalls);
        }

        
    };

    Test t;
    t.DoTest();
    
}

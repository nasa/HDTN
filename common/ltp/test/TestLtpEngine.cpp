#include <boost/test/unit_test.hpp>
#include "LtpEngine.h"
#include <boost/bind.hpp>

BOOST_AUTO_TEST_CASE(LtpEngineTestCase)
{
    struct Test {
        const uint64_t ENGINE_ID_SRC;
        const uint64_t ENGINE_ID_DEST;
        const uint64_t CLIENT_SERVICE_ID_DEST;
        LtpEngine engineSrc; 
        LtpEngine engineDest;
        const std::string DESIRED_RED_DATA_TO_SEND;

        uint64_t numRedPartReceptionCallbacks;
        uint64_t numSrcToDestDataExchanged;
        uint64_t numDestToSrcDataExchanged;

        Test() :
            ENGINE_ID_SRC(100),
            ENGINE_ID_DEST(200),
            CLIENT_SERVICE_ID_DEST(300),
            engineSrc(ENGINE_ID_SRC, 1),//1=> 1 CHARACTER AT A TIME
            engineDest(ENGINE_ID_DEST, 1),//1=> MTU NOT USED AT THIS TIME
            DESIRED_RED_DATA_TO_SEND("The quick brown fox jumps over the lazy dog!")
        {
            engineDest.SetRedPartReceptionCallback(boost::bind(&Test::RedPartReceptionCallback, this, boost::placeholders::_1, boost::placeholders::_2, boost::placeholders::_3,
                boost::placeholders::_4, boost::placeholders::_5));
        }

        void RedPartReceptionCallback(const Ltp::session_id_t & sessionId, const std::vector<uint8_t> & clientServiceDataVec, uint64_t lengthOfRedPart, uint64_t clientServiceId, bool isEndOfBlock) {
            std::string receivedMessage(clientServiceDataVec.data(), clientServiceDataVec.data() + clientServiceDataVec.size());
            ++numRedPartReceptionCallbacks;
            BOOST_REQUIRE_EQUAL(receivedMessage, DESIRED_RED_DATA_TO_SEND);
            //std::cout << "receivedMessage: " << receivedMessage << std::endl;
            //std::cout << "here\n";
        }
        static bool SendData(LtpEngine & src, LtpEngine & dest, bool simulateDrop = false) {
            std::vector<boost::asio::const_buffer> constBufferVec;
            boost::shared_ptr<std::vector<std::vector<uint8_t> > >  underlyingDataToDeleteOnSentCallback;
            if (src.NextPacketToSendRoundRobin(constBufferVec, underlyingDataToDeleteOnSentCallback)) {
                if (!simulateDrop) {
                    dest.PacketIn(constBufferVec);
                }
                return true;
            }
            return false;
        }

        //returns false when no data exchanged
        bool ExchangeData(bool simulateDropSrcToDest = false, bool simulateDropDestToSrc = false) {
            bool didSrcToDest = SendData(engineSrc, engineDest, simulateDropSrcToDest);
            bool didDestToSrc = SendData(engineDest, engineSrc, simulateDropDestToSrc);
            numSrcToDestDataExchanged += didSrcToDest;
            numDestToSrcDataExchanged += didDestToSrc;
            return (didSrcToDest || didDestToSrc);
        }
        void DoTest() {
            engineSrc.Reset();
            engineDest.Reset();
            numSrcToDestDataExchanged = 0;
            numDestToSrcDataExchanged = 0;
            numRedPartReceptionCallbacks = 0;
            engineSrc.TransmissionRequest(CLIENT_SERVICE_ID_DEST, ENGINE_ID_DEST, (uint8_t*)DESIRED_RED_DATA_TO_SEND.data(), DESIRED_RED_DATA_TO_SEND.size(), DESIRED_RED_DATA_TO_SEND.size());
            while (ExchangeData()) {
                
            }
            //std::cout << "numSrcToDestDataExchanged " << numSrcToDestDataExchanged << " numDestToSrcDataExchanged " << numDestToSrcDataExchanged << " DESIRED_RED_DATA_TO_SEND.size() " << DESIRED_RED_DATA_TO_SEND.size() << std::endl;
            BOOST_REQUIRE_EQUAL(numSrcToDestDataExchanged, DESIRED_RED_DATA_TO_SEND.size() + 1); //+1 for Report ack
            BOOST_REQUIRE_EQUAL(numDestToSrcDataExchanged, 1); //1 for Report segment
            BOOST_REQUIRE_EQUAL(numRedPartReceptionCallbacks, 1);
        }

        void DoTestOneDropSrcToDest() {
            engineSrc.Reset();
            engineDest.Reset();
            numSrcToDestDataExchanged = 0;
            numDestToSrcDataExchanged = 0;
            numRedPartReceptionCallbacks = 0;
            engineSrc.TransmissionRequest(CLIENT_SERVICE_ID_DEST, ENGINE_ID_DEST, (uint8_t*)DESIRED_RED_DATA_TO_SEND.data(), DESIRED_RED_DATA_TO_SEND.size(), DESIRED_RED_DATA_TO_SEND.size());
            unsigned int count = 0;
            while (ExchangeData(count == 10, false)) {
                ++count;
            }
            //std::cout << "numSrcToDestDataExchanged " << numSrcToDestDataExchanged << " numDestToSrcDataExchanged " << numDestToSrcDataExchanged << " DESIRED_RED_DATA_TO_SEND.size() " << DESIRED_RED_DATA_TO_SEND.size() << std::endl;
            BOOST_REQUIRE_EQUAL(numSrcToDestDataExchanged, DESIRED_RED_DATA_TO_SEND.size() + 3); //+3 for 2 Report acks and 1 resend
            BOOST_REQUIRE_EQUAL(numDestToSrcDataExchanged, 2); //2 for 2 Report segments
            BOOST_REQUIRE_EQUAL(numRedPartReceptionCallbacks, 1);
        }

        void DoTestTwoDropsSrcToDest() {
            engineSrc.Reset();
            engineDest.Reset();
            numSrcToDestDataExchanged = 0;
            numDestToSrcDataExchanged = 0;
            numRedPartReceptionCallbacks = 0;
            engineSrc.TransmissionRequest(CLIENT_SERVICE_ID_DEST, ENGINE_ID_DEST, (uint8_t*)DESIRED_RED_DATA_TO_SEND.data(), DESIRED_RED_DATA_TO_SEND.size(), DESIRED_RED_DATA_TO_SEND.size());
            unsigned int count = 0;
            while (ExchangeData((count == 10) || (count == 13), false)) {
                ++count;
            }
            //std::cout << "numSrcToDestDataExchanged " << numSrcToDestDataExchanged << " numDestToSrcDataExchanged " << numDestToSrcDataExchanged << " DESIRED_RED_DATA_TO_SEND.size() " << DESIRED_RED_DATA_TO_SEND.size() << std::endl;
            //TODO BOOST_REQUIRE_EQUAL(numSrcToDestDataExchanged, DESIRED_RED_DATA_TO_SEND.size() + 7); //+7 for 3 Report acks and 2 resends
            //TODO BOOST_REQUIRE_EQUAL(numDestToSrcDataExchanged, 2); //2 for 2 Report segments
            BOOST_REQUIRE_EQUAL(numRedPartReceptionCallbacks, 1);
        }
    };

    Test t;
    t.DoTest();
    t.DoTestOneDropSrcToDest();
    //t.DoTestTwoDropsSrcToDest();
}

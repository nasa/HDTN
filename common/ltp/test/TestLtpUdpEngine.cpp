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
        const std::string DESIRED_RED_AND_GREEN_DATA_TO_SEND;
        const std::string DESIRED_FULLY_GREEN_DATA_TO_SEND;
        boost::condition_variable cv;
        boost::mutex cvMutex;
        boost::mutex::scoped_lock cvLock;

        uint64_t numRedPartReceptionCallbacks;
        uint64_t numSessionStartSenderCallbacks;
        uint64_t numSessionStartReceiverCallbacks;
        uint64_t numGreenPartReceptionCallbacks;
        uint64_t numReceptionSessionCancelledCallbacks;
        uint64_t numTransmissionSessionCompletedCallbacks;
        uint64_t numInitialTransmissionCompletedCallbacks;
        uint64_t numTransmissionSessionCancelledCallbacks;

        CANCEL_SEGMENT_REASON_CODES lastReasonCode_receptionSessionCancelledCallback;
        CANCEL_SEGMENT_REASON_CODES lastReasonCode_transmissionSessionCancelledCallback;
        Ltp::session_id_t lastSessionId_sessionStartSenderCallback;

        Test() :
            ONE_WAY_LIGHT_TIME(boost::posix_time::milliseconds(500)),
            ONE_WAY_MARGIN_TIME(boost::posix_time::milliseconds(500)),
            ENGINE_ID_SRC(100),
            ENGINE_ID_DEST(200),
            CLIENT_SERVICE_ID_DEST(300),
            engineSrc(ENGINE_ID_SRC, 1, UINT64_MAX, ONE_WAY_LIGHT_TIME, ONE_WAY_MARGIN_TIME, 0),//1=> 1 CHARACTER AT A TIME, UINT64_MAX=> unlimited report segment size
            engineDest(ENGINE_ID_DEST, 1, UINT64_MAX, ONE_WAY_LIGHT_TIME, ONE_WAY_MARGIN_TIME, 12345),//1=> MTU NOT USED AT THIS TIME, UINT64_MAX=> unlimited report segment size
            DESIRED_RED_DATA_TO_SEND("The quick brown fox jumps over the lazy dog!"),
            DESIRED_RED_AND_GREEN_DATA_TO_SEND("The quick brown fox jumps over the lazy dog!GGE"), //G=>green data not EOB, E=>green datat EOB
            DESIRED_FULLY_GREEN_DATA_TO_SEND("GGGGGGGGGGGGGGGGGE"),
            cvLock(cvMutex)
        {
            engineDest.SetSessionStartCallback(boost::bind(&Test::SessionStartReceiverCallback, this, boost::placeholders::_1));
            engineDest.SetRedPartReceptionCallback(boost::bind(&Test::RedPartReceptionCallback, this, boost::placeholders::_1, boost::placeholders::_2, boost::placeholders::_3,
                boost::placeholders::_4, boost::placeholders::_5));
            engineDest.SetGreenPartSegmentArrivalCallback(boost::bind(&Test::GreenPartSegmentArrivalCallback, this, boost::placeholders::_1, boost::placeholders::_2, boost::placeholders::_3,
                boost::placeholders::_4, boost::placeholders::_5));
            engineDest.SetReceptionSessionCancelledCallback(boost::bind(&Test::ReceptionSessionCancelledCallback, this, boost::placeholders::_1, boost::placeholders::_2));
            engineSrc.SetSessionStartCallback(boost::bind(&Test::SessionStartSenderCallback, this, boost::placeholders::_1));
            engineSrc.SetTransmissionSessionCompletedCallback(boost::bind(&Test::TransmissionSessionCompletedCallback, this, boost::placeholders::_1));
            engineSrc.SetInitialTransmissionCompletedCallback(boost::bind(&Test::InitialTransmissionCompletedCallback, this, boost::placeholders::_1));
            engineSrc.SetTransmissionSessionCancelledCallback(boost::bind(&Test::TransmissionSessionCancelledCallback, this, boost::placeholders::_1, boost::placeholders::_2));

            engineSrc.Connect("localhost", "12345");
            while (!engineSrc.ReadyToForward()) {
                std::cout << "connecting\n";
                boost::this_thread::sleep(boost::posix_time::milliseconds(500));
            }
        }

        void SessionStartSenderCallback(const Ltp::session_id_t & sessionId) {
            ++numSessionStartSenderCallbacks;

            //On receiving this notice the client service may, for example, .. remember the
            //session ID so that the session can be canceled in the future if necessary.
            lastSessionId_sessionStartSenderCallback = sessionId;
        }
        void SessionStartReceiverCallback(const Ltp::session_id_t & sessionId) {
            ++numSessionStartReceiverCallbacks;
            BOOST_REQUIRE(sessionId == lastSessionId_sessionStartSenderCallback);
        }
        void RedPartReceptionCallback(const Ltp::session_id_t & sessionId, std::vector<uint8_t> & movableClientServiceDataVec, uint64_t lengthOfRedPart, uint64_t clientServiceId, bool isEndOfBlock) {
            std::string receivedMessage(movableClientServiceDataVec.data(), movableClientServiceDataVec.data() + movableClientServiceDataVec.size());
            ++numRedPartReceptionCallbacks;
            BOOST_REQUIRE_EQUAL(receivedMessage, DESIRED_RED_DATA_TO_SEND);
            BOOST_REQUIRE(sessionId == lastSessionId_sessionStartSenderCallback);
            cv.notify_one();
            //std::cout << "receivedMessage: " << receivedMessage << std::endl;
            //std::cout << "here\n";
        }
        void GreenPartSegmentArrivalCallback(const Ltp::session_id_t & sessionId, std::vector<uint8_t> & movableClientServiceDataVec, uint64_t offsetStartOfBlock, uint64_t clientServiceId, bool isEndOfBlock) {
            ++numGreenPartReceptionCallbacks;
            BOOST_REQUIRE_EQUAL(movableClientServiceDataVec.size(), 1);
            if (isEndOfBlock) {
                BOOST_REQUIRE_EQUAL(movableClientServiceDataVec[0], 'E');
            }
            else {
                BOOST_REQUIRE_EQUAL(movableClientServiceDataVec[0], 'G');
            }
            BOOST_REQUIRE(sessionId == lastSessionId_sessionStartSenderCallback);
            cv.notify_one();
        }
        void ReceptionSessionCancelledCallback(const Ltp::session_id_t & sessionId, CANCEL_SEGMENT_REASON_CODES reasonCode) {
            ++numReceptionSessionCancelledCallbacks;
            lastReasonCode_receptionSessionCancelledCallback = reasonCode;
            BOOST_REQUIRE(sessionId == lastSessionId_sessionStartSenderCallback);
        }
        void TransmissionSessionCompletedCallback(const Ltp::session_id_t & sessionId) {
            ++numTransmissionSessionCompletedCallbacks;
            BOOST_REQUIRE(sessionId == lastSessionId_sessionStartSenderCallback);
            cv.notify_one();
        }
        void InitialTransmissionCompletedCallback(const Ltp::session_id_t & sessionId) {
            BOOST_REQUIRE(sessionId == lastSessionId_sessionStartSenderCallback);
            ++numInitialTransmissionCompletedCallbacks;
        }
        void TransmissionSessionCancelledCallback(const Ltp::session_id_t & sessionId, CANCEL_SEGMENT_REASON_CODES reasonCode) {
            ++numTransmissionSessionCancelledCallbacks;
            lastReasonCode_transmissionSessionCancelledCallback = reasonCode;
            BOOST_REQUIRE(sessionId == lastSessionId_sessionStartSenderCallback);
        }

        void Reset() {
            engineSrc.Reset();
            engineDest.Reset();
            engineSrc.m_udpDropSimulatorFunction = LtpUdpEngine::UdpDropSimulatorFunction_t();
            engineDest.m_udpDropSimulatorFunction = LtpUdpEngine::UdpDropSimulatorFunction_t();
            numRedPartReceptionCallbacks = 0;
            numSessionStartSenderCallbacks = 0;
            numSessionStartReceiverCallbacks = 0;
            numGreenPartReceptionCallbacks = 0;
            numReceptionSessionCancelledCallbacks = 0;
            numTransmissionSessionCompletedCallbacks = 0;
            numInitialTransmissionCompletedCallbacks = 0;
            numTransmissionSessionCancelledCallbacks = 0;
            lastSessionId_sessionStartSenderCallback = 0; //sets all fields to 0
        }
        void AssertNoActiveSendersAndReceivers() {
            BOOST_REQUIRE_EQUAL(engineSrc.NumActiveSenders(), 0);
            BOOST_REQUIRE_EQUAL(engineSrc.NumActiveReceivers(), 0);
            BOOST_REQUIRE_EQUAL(engineDest.NumActiveSenders(), 0);
            BOOST_REQUIRE_EQUAL(engineDest.NumActiveReceivers(), 0);
        }
        bool HasActiveSendersAndReceivers() {
            return engineSrc.NumActiveSenders() || engineSrc.NumActiveReceivers() || engineDest.NumActiveSenders() || engineDest.NumActiveReceivers();
        }
        void TryWaitForNoActiveSendersAndReceivers() {
            for(unsigned int i=0; i<20; ++i) {
                if(!HasActiveSendersAndReceivers()) {
                    break;
                }
                boost::this_thread::sleep(boost::posix_time::milliseconds(200));
            }
            boost::this_thread::sleep(boost::posix_time::milliseconds(200)); //give extra 200ms before any non-thread-safe Reset() is called (Reset() should not be called in production)
        }

        void DoTest() {
            Reset();
            AssertNoActiveSendersAndReceivers();
            boost::shared_ptr<LtpEngine::transmission_request_t> tReq = boost::make_shared<LtpEngine::transmission_request_t>();
            tReq->destinationClientServiceId = CLIENT_SERVICE_ID_DEST;
            tReq->destinationLtpEngineId = ENGINE_ID_DEST;
            tReq->clientServiceDataToSend = std::vector<uint8_t>(DESIRED_RED_DATA_TO_SEND.data(), DESIRED_RED_DATA_TO_SEND.data() + DESIRED_RED_DATA_TO_SEND.size()); //copy
            tReq->lengthOfRedPart = DESIRED_RED_DATA_TO_SEND.size();
            engineSrc.TransmissionRequest_ThreadSafe(std::move(tReq));
            for (unsigned int i = 0; i < 10; ++i) {
                if (numRedPartReceptionCallbacks && numTransmissionSessionCompletedCallbacks) {
                    break;
                }
                cv.timed_wait(cvLock, boost::posix_time::milliseconds(200));
            }
            TryWaitForNoActiveSendersAndReceivers();
            AssertNoActiveSendersAndReceivers();
            //std::cout << "numSrcToDestDataExchanged " << numSrcToDestDataExchanged << " numDestToSrcDataExchanged " << numDestToSrcDataExchanged << " DESIRED_RED_DATA_TO_SEND.size() " << DESIRED_RED_DATA_TO_SEND.size() << std::endl;

            BOOST_REQUIRE_EQUAL(numRedPartReceptionCallbacks, 1);
            BOOST_REQUIRE_EQUAL(numSessionStartSenderCallbacks, 1);
            BOOST_REQUIRE_EQUAL(numSessionStartReceiverCallbacks, 1);
            BOOST_REQUIRE_EQUAL(numGreenPartReceptionCallbacks, 0);
            BOOST_REQUIRE_EQUAL(numReceptionSessionCancelledCallbacks, 0);
            BOOST_REQUIRE_EQUAL(numTransmissionSessionCompletedCallbacks, 1);
            BOOST_REQUIRE_EQUAL(numInitialTransmissionCompletedCallbacks, 1);
            BOOST_REQUIRE_EQUAL(numTransmissionSessionCancelledCallbacks, 0);

            BOOST_REQUIRE_EQUAL(engineSrc.m_countAsyncSendCallbackCalls, DESIRED_RED_DATA_TO_SEND.size() + 1); //+1 for Report ack
            BOOST_REQUIRE_EQUAL(engineSrc.m_countAsyncSendCallbackCalls, engineSrc.m_countAsyncSendCalls);
            BOOST_REQUIRE_EQUAL(engineDest.m_countAsyncSendCallbackCalls, 1); //1 for Report segment
            BOOST_REQUIRE_EQUAL(engineDest.m_countAsyncSendCallbackCalls, engineDest.m_countAsyncSendCalls);
        }

        void DoTestRedAndGreenData() {
            Reset();
            AssertNoActiveSendersAndReceivers();
            boost::shared_ptr<LtpEngine::transmission_request_t> tReq = boost::make_shared<LtpEngine::transmission_request_t>();
            tReq->destinationClientServiceId = CLIENT_SERVICE_ID_DEST;
            tReq->destinationLtpEngineId = ENGINE_ID_DEST;
            tReq->clientServiceDataToSend = std::vector<uint8_t>(DESIRED_RED_AND_GREEN_DATA_TO_SEND.data(), DESIRED_RED_AND_GREEN_DATA_TO_SEND.data() + DESIRED_RED_AND_GREEN_DATA_TO_SEND.size()); //copy
            tReq->lengthOfRedPart = DESIRED_RED_DATA_TO_SEND.size();
            engineSrc.TransmissionRequest_ThreadSafe(std::move(tReq));
            for (unsigned int i = 0; i < 10; ++i) {
                if (numRedPartReceptionCallbacks && numTransmissionSessionCompletedCallbacks && (numGreenPartReceptionCallbacks >= 3)) {
                    break;
                }
                cv.timed_wait(cvLock, boost::posix_time::milliseconds(200));
            }
            TryWaitForNoActiveSendersAndReceivers();
            AssertNoActiveSendersAndReceivers();
            //std::cout << "numSrcToDestDataExchanged " << numSrcToDestDataExchanged << " numDestToSrcDataExchanged " << numDestToSrcDataExchanged << " DESIRED_RED_DATA_TO_SEND.size() " << DESIRED_RED_DATA_TO_SEND.size() << std::endl;

            BOOST_REQUIRE_EQUAL(numRedPartReceptionCallbacks, 1);
            BOOST_REQUIRE_EQUAL(numSessionStartSenderCallbacks, 1);
            BOOST_REQUIRE_EQUAL(numSessionStartReceiverCallbacks, 1);
            BOOST_REQUIRE_EQUAL(numGreenPartReceptionCallbacks, 3);
            BOOST_REQUIRE_EQUAL(numReceptionSessionCancelledCallbacks, 0);
            BOOST_REQUIRE_EQUAL(numTransmissionSessionCompletedCallbacks, 1);
            BOOST_REQUIRE_EQUAL(numInitialTransmissionCompletedCallbacks, 1);
            BOOST_REQUIRE_EQUAL(numTransmissionSessionCancelledCallbacks, 0);

            BOOST_REQUIRE_EQUAL(engineSrc.m_countAsyncSendCallbackCalls, DESIRED_RED_AND_GREEN_DATA_TO_SEND.size() + 1); //+1 for Report ack
            BOOST_REQUIRE_EQUAL(engineSrc.m_countAsyncSendCallbackCalls, engineSrc.m_countAsyncSendCalls);
            BOOST_REQUIRE_EQUAL(engineDest.m_countAsyncSendCallbackCalls, 1); //1 for Report segment
            BOOST_REQUIRE_EQUAL(engineDest.m_countAsyncSendCallbackCalls, engineDest.m_countAsyncSendCalls);
        }

        void DoTestFullyGreenData() {
            Reset();
            AssertNoActiveSendersAndReceivers();
            boost::shared_ptr<LtpEngine::transmission_request_t> tReq = boost::make_shared<LtpEngine::transmission_request_t>();
            tReq->destinationClientServiceId = CLIENT_SERVICE_ID_DEST;
            tReq->destinationLtpEngineId = ENGINE_ID_DEST;
            tReq->clientServiceDataToSend = std::vector<uint8_t>(DESIRED_FULLY_GREEN_DATA_TO_SEND.data(), DESIRED_FULLY_GREEN_DATA_TO_SEND.data() + DESIRED_FULLY_GREEN_DATA_TO_SEND.size()); //copy
            tReq->lengthOfRedPart = 0;
            engineSrc.TransmissionRequest_ThreadSafe(std::move(tReq));
            for (unsigned int i = 0; i < 10; ++i) {
                if (numTransmissionSessionCompletedCallbacks && (numGreenPartReceptionCallbacks >= DESIRED_FULLY_GREEN_DATA_TO_SEND.size())) {
                    break;
                }
                cv.timed_wait(cvLock, boost::posix_time::milliseconds(200));
            }
            TryWaitForNoActiveSendersAndReceivers();
            AssertNoActiveSendersAndReceivers();
            //std::cout << "numSrcToDestDataExchanged " << numSrcToDestDataExchanged << " numDestToSrcDataExchanged " << numDestToSrcDataExchanged << " DESIRED_RED_DATA_TO_SEND.size() " << DESIRED_RED_DATA_TO_SEND.size() << std::endl;

            BOOST_REQUIRE_EQUAL(numRedPartReceptionCallbacks, 0);
            BOOST_REQUIRE_EQUAL(numSessionStartSenderCallbacks, 1);
            BOOST_REQUIRE_EQUAL(numSessionStartReceiverCallbacks, 1);
            BOOST_REQUIRE_EQUAL(numGreenPartReceptionCallbacks, DESIRED_FULLY_GREEN_DATA_TO_SEND.size());
            BOOST_REQUIRE_EQUAL(numReceptionSessionCancelledCallbacks, 0);
            BOOST_REQUIRE_EQUAL(numTransmissionSessionCompletedCallbacks, 1);
            BOOST_REQUIRE_EQUAL(numInitialTransmissionCompletedCallbacks, 1);
            BOOST_REQUIRE_EQUAL(numTransmissionSessionCancelledCallbacks, 0);

            BOOST_REQUIRE_EQUAL(engineSrc.m_countAsyncSendCallbackCalls, DESIRED_FULLY_GREEN_DATA_TO_SEND.size());
            BOOST_REQUIRE_EQUAL(engineSrc.m_countAsyncSendCallbackCalls, engineSrc.m_countAsyncSendCalls);
            BOOST_REQUIRE_EQUAL(engineDest.m_countAsyncSendCallbackCalls, 0);
            BOOST_REQUIRE_EQUAL(engineDest.m_countAsyncSendCallbackCalls, engineDest.m_countAsyncSendCalls);
        }

        
        void DoTestOneDropDataSegmentSrcToDest() {
            struct DropOneSimulation {
                int count;
                DropOneSimulation() : count(0) {}
                bool DoSim(const uint8_t ltpHeaderByte) {
                    const LTP_SEGMENT_TYPE_FLAGS type = static_cast<LTP_SEGMENT_TYPE_FLAGS>(ltpHeaderByte);
                    if (type == LTP_SEGMENT_TYPE_FLAGS::REDDATA) {
                        if (++count == 10) {
                            //std::cout << "drop\n";
                            return true;
                        }
                    }
                    return false;
                }
            };
            Reset();
            AssertNoActiveSendersAndReceivers();
            DropOneSimulation sim;
            engineSrc.m_udpDropSimulatorFunction = boost::bind(&DropOneSimulation::DoSim, &sim, boost::placeholders::_1);
            boost::shared_ptr<LtpEngine::transmission_request_t> tReq = boost::make_shared<LtpEngine::transmission_request_t>();
            tReq->destinationClientServiceId = CLIENT_SERVICE_ID_DEST;
            tReq->destinationLtpEngineId = ENGINE_ID_DEST;
            tReq->clientServiceDataToSend = std::vector<uint8_t>(DESIRED_RED_DATA_TO_SEND.data(), DESIRED_RED_DATA_TO_SEND.data() + DESIRED_RED_DATA_TO_SEND.size()); //copy
            tReq->lengthOfRedPart = DESIRED_RED_DATA_TO_SEND.size();
            engineSrc.TransmissionRequest_ThreadSafe(std::move(tReq));
            for (unsigned int i = 0; i < 10; ++i) {
                if (numRedPartReceptionCallbacks && numTransmissionSessionCompletedCallbacks) {
                    break;
                }
                cv.timed_wait(cvLock, boost::posix_time::milliseconds(200));
            }
            TryWaitForNoActiveSendersAndReceivers();
            AssertNoActiveSendersAndReceivers();
            //std::cout << "numSrcToDestDataExchanged " << numSrcToDestDataExchanged << " numDestToSrcDataExchanged " << numDestToSrcDataExchanged << " DESIRED_RED_DATA_TO_SEND.size() " << DESIRED_RED_DATA_TO_SEND.size() << std::endl;
            BOOST_REQUIRE_EQUAL(engineSrc.m_countAsyncSendCallbackCalls, DESIRED_RED_DATA_TO_SEND.size() + 3); //+3 for 2 Report acks and 1 resend
            BOOST_REQUIRE_EQUAL(engineSrc.m_countAsyncSendCallbackCalls, engineSrc.m_countAsyncSendCalls);
            BOOST_REQUIRE_EQUAL(engineDest.m_countAsyncSendCallbackCalls, 2); //2 for 2 Report segments
            BOOST_REQUIRE_EQUAL(engineDest.m_countAsyncSendCallbackCalls, engineDest.m_countAsyncSendCalls);
            BOOST_REQUIRE_EQUAL(numRedPartReceptionCallbacks, 1);
            BOOST_REQUIRE_EQUAL(numSessionStartSenderCallbacks, 1);
            BOOST_REQUIRE_EQUAL(numSessionStartReceiverCallbacks, 1);
            BOOST_REQUIRE_EQUAL(numGreenPartReceptionCallbacks, 0);
            BOOST_REQUIRE_EQUAL(numReceptionSessionCancelledCallbacks, 0);
            BOOST_REQUIRE_EQUAL(numTransmissionSessionCompletedCallbacks, 1);
            BOOST_REQUIRE_EQUAL(numInitialTransmissionCompletedCallbacks, 1);
            BOOST_REQUIRE_EQUAL(numTransmissionSessionCancelledCallbacks, 0);
        }


        
        void DoTestTwoDropDataSegmentSrcToDest() {
            struct DropTwoSimulation {
                int count;
                DropTwoSimulation() : count(0) {}
                bool DoSim(const uint8_t ltpHeaderByte) {
                    const LTP_SEGMENT_TYPE_FLAGS type = static_cast<LTP_SEGMENT_TYPE_FLAGS>(ltpHeaderByte);
                    if (type == LTP_SEGMENT_TYPE_FLAGS::REDDATA) {
                        ++count;
                        if ((count == 10) || (count == 13)) {
                            //std::cout << "drop\n";
                            return true;
                        }
                    }
                    return false;
                }
            };
            Reset();
            AssertNoActiveSendersAndReceivers();
            DropTwoSimulation sim;
            engineSrc.m_udpDropSimulatorFunction = boost::bind(&DropTwoSimulation::DoSim, &sim, boost::placeholders::_1);
            boost::shared_ptr<LtpEngine::transmission_request_t> tReq = boost::make_shared<LtpEngine::transmission_request_t>();
            tReq->destinationClientServiceId = CLIENT_SERVICE_ID_DEST;
            tReq->destinationLtpEngineId = ENGINE_ID_DEST;
            tReq->clientServiceDataToSend = std::vector<uint8_t>(DESIRED_RED_DATA_TO_SEND.data(), DESIRED_RED_DATA_TO_SEND.data() + DESIRED_RED_DATA_TO_SEND.size()); //copy
            tReq->lengthOfRedPart = DESIRED_RED_DATA_TO_SEND.size();
            engineSrc.TransmissionRequest_ThreadSafe(std::move(tReq));
            for (unsigned int i = 0; i < 10; ++i) {
                if (numRedPartReceptionCallbacks && numTransmissionSessionCompletedCallbacks) {
                    break;
                }
                cv.timed_wait(cvLock, boost::posix_time::milliseconds(200));
            }
            TryWaitForNoActiveSendersAndReceivers();
            AssertNoActiveSendersAndReceivers();
            //std::cout << "numSrcToDestDataExchanged " << numSrcToDestDataExchanged << " numDestToSrcDataExchanged " << numDestToSrcDataExchanged << " DESIRED_RED_DATA_TO_SEND.size() " << DESIRED_RED_DATA_TO_SEND.size() << std::endl;
            BOOST_REQUIRE_EQUAL(engineSrc.m_countAsyncSendCallbackCalls, DESIRED_RED_DATA_TO_SEND.size() + 4); //+4 for 2 Report acks and 2 resends
            BOOST_REQUIRE_EQUAL(engineSrc.m_countAsyncSendCallbackCalls, engineSrc.m_countAsyncSendCalls);
            BOOST_REQUIRE_EQUAL(engineDest.m_countAsyncSendCallbackCalls, 2); //2 for 2 Report segments
            BOOST_REQUIRE_EQUAL(engineDest.m_countAsyncSendCallbackCalls, engineDest.m_countAsyncSendCalls);
            BOOST_REQUIRE_EQUAL(numRedPartReceptionCallbacks, 1);
            BOOST_REQUIRE_EQUAL(numSessionStartSenderCallbacks, 1);
            BOOST_REQUIRE_EQUAL(numSessionStartReceiverCallbacks, 1);
            BOOST_REQUIRE_EQUAL(numGreenPartReceptionCallbacks, 0);
            BOOST_REQUIRE_EQUAL(numReceptionSessionCancelledCallbacks, 0);
            BOOST_REQUIRE_EQUAL(numTransmissionSessionCompletedCallbacks, 1);
            BOOST_REQUIRE_EQUAL(numInitialTransmissionCompletedCallbacks, 1);
            BOOST_REQUIRE_EQUAL(numTransmissionSessionCancelledCallbacks, 0);
        }

        void DoTestTwoDropDataSegmentSrcToDestRegularCheckpoints() {
            struct DropTwoSimulation {
                int count;
                DropTwoSimulation() : count(0) {}
                bool DoSim(const uint8_t ltpHeaderByte) {
                    const LTP_SEGMENT_TYPE_FLAGS type = static_cast<LTP_SEGMENT_TYPE_FLAGS>(ltpHeaderByte);
                    if (type == LTP_SEGMENT_TYPE_FLAGS::REDDATA) { //dont skip checkpoints
                        ++count;
                        if ((count == 7) || (count == 13)) {
                            //std::cout << "drop\n";
                            return true;
                        }
                    }
                    return false;
                }
            };
            Reset();
            AssertNoActiveSendersAndReceivers();
            DropTwoSimulation sim;
            engineSrc.m_udpDropSimulatorFunction = boost::bind(&DropTwoSimulation::DoSim, &sim, boost::placeholders::_1);
            boost::shared_ptr<LtpEngine::transmission_request_t> tReq = boost::make_shared<LtpEngine::transmission_request_t>();
            tReq->destinationClientServiceId = CLIENT_SERVICE_ID_DEST;
            tReq->destinationLtpEngineId = ENGINE_ID_DEST;
            tReq->clientServiceDataToSend = std::vector<uint8_t>(DESIRED_RED_DATA_TO_SEND.data(), DESIRED_RED_DATA_TO_SEND.data() + DESIRED_RED_DATA_TO_SEND.size()); //copy
            tReq->lengthOfRedPart = DESIRED_RED_DATA_TO_SEND.size();
            engineSrc.SetCheckpointEveryNthDataPacketForSenders(5);
            engineSrc.TransmissionRequest_ThreadSafe(std::move(tReq));
            for (unsigned int i = 0; i < 10; ++i) {
                if (numRedPartReceptionCallbacks && numTransmissionSessionCompletedCallbacks) {
                    break;
                }
                cv.timed_wait(cvLock, boost::posix_time::milliseconds(200));
            }
            TryWaitForNoActiveSendersAndReceivers();
            AssertNoActiveSendersAndReceivers();
            //std::cout << "numSrcToDestDataExchanged " << numSrcToDestDataExchanged << " numDestToSrcDataExchanged " << numDestToSrcDataExchanged << " DESIRED_RED_DATA_TO_SEND.size() " << DESIRED_RED_DATA_TO_SEND.size() << std::endl;
            BOOST_REQUIRE_EQUAL(engineSrc.m_countAsyncSendCallbackCalls, DESIRED_RED_DATA_TO_SEND.size() + 13); //+13 for 11 Report acks (see next line) and 2 resends
            BOOST_REQUIRE_EQUAL(engineSrc.m_countAsyncSendCallbackCalls, engineSrc.m_countAsyncSendCalls);

            //primary first LB: 0, UB: 5
            //primary subsequent LB : 5, UB : 10
            //primary subsequent LB : 10, UB : 15
            //secondary LB : 5, UB : 8
            //primary subsequent LB : 15, UB : 20
            //primary subsequent LB : 20, UB : 25
            //secondary LB : 15, UB : 16
            //primary subsequent LB : 25, UB : 30
            //primary subsequent LB : 30, UB : 35
            //primary subsequent LB : 35, UB : 40
            //primary subsequent LB : 40, UB : 44
            BOOST_REQUIRE_EQUAL(engineDest.m_countAsyncSendCallbackCalls, 11); // 44/5=8 + (1 eobCp at 44) + 2 retrans report 

            BOOST_REQUIRE_EQUAL(engineDest.m_countAsyncSendCallbackCalls, engineDest.m_countAsyncSendCalls);
            BOOST_REQUIRE_EQUAL(numRedPartReceptionCallbacks, 1);
            BOOST_REQUIRE_EQUAL(numSessionStartSenderCallbacks, 1);
            BOOST_REQUIRE_EQUAL(numSessionStartReceiverCallbacks, 1);
            BOOST_REQUIRE_EQUAL(numGreenPartReceptionCallbacks, 0);
            BOOST_REQUIRE_EQUAL(numReceptionSessionCancelledCallbacks, 0);
            BOOST_REQUIRE_EQUAL(numTransmissionSessionCompletedCallbacks, 1);
            BOOST_REQUIRE_EQUAL(numInitialTransmissionCompletedCallbacks, 1);
            BOOST_REQUIRE_EQUAL(numTransmissionSessionCancelledCallbacks, 0);
        }

        //this test essentially doesn't do anything new the above does.  The skipped checkpoint is settled at the next checkpoint
        //and the transmission is completed before the timer expires, cancelling it
        void DoTestDropOneCheckpointDataSegmentSrcToDest() {
            struct DropSimulation {
                int count;
                DropSimulation() : count(0) {}
                bool DoSim(const uint8_t ltpHeaderByte) {
                    const LTP_SEGMENT_TYPE_FLAGS type = static_cast<LTP_SEGMENT_TYPE_FLAGS>(ltpHeaderByte);
                    if (type == LTP_SEGMENT_TYPE_FLAGS::REDDATA_CHECKPOINT) { //skip only non-EORP-EOB checkpoints
                        ++count;
                        if (count == 2) {
                            std::cout << "drop\n";
                            return true;
                        }
                    }
                    return false;
                }
            };
            Reset();
            AssertNoActiveSendersAndReceivers();
            DropSimulation sim;
            engineSrc.m_udpDropSimulatorFunction = boost::bind(&DropSimulation::DoSim, &sim, boost::placeholders::_1);
            boost::shared_ptr<LtpEngine::transmission_request_t> tReq = boost::make_shared<LtpEngine::transmission_request_t>();
            tReq->destinationClientServiceId = CLIENT_SERVICE_ID_DEST;
            tReq->destinationLtpEngineId = ENGINE_ID_DEST;
            tReq->clientServiceDataToSend = std::vector<uint8_t>(DESIRED_RED_DATA_TO_SEND.data(), DESIRED_RED_DATA_TO_SEND.data() + DESIRED_RED_DATA_TO_SEND.size()); //copy
            tReq->lengthOfRedPart = DESIRED_RED_DATA_TO_SEND.size();
            engineSrc.SetCheckpointEveryNthDataPacketForSenders(5);
            engineSrc.TransmissionRequest_ThreadSafe(std::move(tReq));
            for (unsigned int i = 0; i < 50; ++i) {
                if (numRedPartReceptionCallbacks && numTransmissionSessionCompletedCallbacks) {
                    break;
                }
                cv.timed_wait(cvLock, boost::posix_time::milliseconds(200));
            }
            TryWaitForNoActiveSendersAndReceivers();
            AssertNoActiveSendersAndReceivers();
            //std::cout << "numSrcToDestDataExchanged " << numSrcToDestDataExchanged << " numDestToSrcDataExchanged " << numDestToSrcDataExchanged << " DESIRED_RED_DATA_TO_SEND.size() " << DESIRED_RED_DATA_TO_SEND.size() << std::endl;
            BOOST_REQUIRE_EQUAL(engineSrc.m_countAsyncSendCallbackCalls, DESIRED_RED_DATA_TO_SEND.size() + 10); //+10 for 9 Report acks (see next line) and 1 resend
            BOOST_REQUIRE_EQUAL(engineSrc.m_countAsyncSendCallbackCalls, engineSrc.m_countAsyncSendCalls);

            //primary first LB: 0, UB: 5
            //primary subsequent LB : 5, UB : 15
            //primary subsequent LB : 15, UB : 20
            //secondary LB : 5, UB : 10
            //primary subsequent LB : 20, UB : 25
            //primary subsequent LB : 25, UB : 30
            //primary subsequent LB : 30, UB : 35
            //primary subsequent LB : 35, UB : 40
            //primary subsequent LB : 40, UB : 44
            BOOST_REQUIRE_EQUAL(engineDest.m_countAsyncSendCallbackCalls, 9); // 44/5-1=7 + (1 eobCp at 44) + 1 retrans report 

            BOOST_REQUIRE_EQUAL(engineDest.m_countAsyncSendCallbackCalls, engineDest.m_countAsyncSendCalls);
            BOOST_REQUIRE_EQUAL(numRedPartReceptionCallbacks, 1);
            BOOST_REQUIRE_EQUAL(numSessionStartSenderCallbacks, 1);
            BOOST_REQUIRE_EQUAL(numSessionStartReceiverCallbacks, 1);
            BOOST_REQUIRE_EQUAL(numGreenPartReceptionCallbacks, 0);
            BOOST_REQUIRE_EQUAL(numReceptionSessionCancelledCallbacks, 0);
            BOOST_REQUIRE_EQUAL(numTransmissionSessionCompletedCallbacks, 1);
            BOOST_REQUIRE_EQUAL(numInitialTransmissionCompletedCallbacks, 1);
            BOOST_REQUIRE_EQUAL(numTransmissionSessionCancelledCallbacks, 0);
        }

        void DoTestDropEOBCheckpointDataSegmentSrcToDest() {
            struct DropSimulation {
                int count;
                DropSimulation() : count(0) {}
                bool DoSim(const uint8_t ltpHeaderByte) {
                    const LTP_SEGMENT_TYPE_FLAGS type = static_cast<LTP_SEGMENT_TYPE_FLAGS>(ltpHeaderByte);
                    if ((type == LTP_SEGMENT_TYPE_FLAGS::REDDATA_CHECKPOINT_ENDOFREDPART) || (type == LTP_SEGMENT_TYPE_FLAGS::REDDATA_CHECKPOINT_ENDOFREDPART_ENDOFBLOCK)) {
                        ++count;
                        if (count == 1) {
                            return true;
                        }
                    }
                    return false;
                }
            };
            Reset();
            AssertNoActiveSendersAndReceivers();
            DropSimulation sim;
            engineSrc.m_udpDropSimulatorFunction = boost::bind(&DropSimulation::DoSim, &sim, boost::placeholders::_1);
            boost::shared_ptr<LtpEngine::transmission_request_t> tReq = boost::make_shared<LtpEngine::transmission_request_t>();
            tReq->destinationClientServiceId = CLIENT_SERVICE_ID_DEST;
            tReq->destinationLtpEngineId = ENGINE_ID_DEST;
            tReq->clientServiceDataToSend = std::vector<uint8_t>(DESIRED_RED_DATA_TO_SEND.data(), DESIRED_RED_DATA_TO_SEND.data() + DESIRED_RED_DATA_TO_SEND.size()); //copy
            tReq->lengthOfRedPart = DESIRED_RED_DATA_TO_SEND.size();
            engineSrc.TransmissionRequest_ThreadSafe(std::move(tReq));
            for (unsigned int i = 0; i < 50; ++i) {
                if (numRedPartReceptionCallbacks && numTransmissionSessionCompletedCallbacks) {
                    break;
                }
                cv.timed_wait(cvLock, boost::posix_time::milliseconds(200));
            }
            TryWaitForNoActiveSendersAndReceivers();
            AssertNoActiveSendersAndReceivers();
            //std::cout << "numSrcToDestDataExchanged " << numSrcToDestDataExchanged << " numDestToSrcDataExchanged " << numDestToSrcDataExchanged << " DESIRED_RED_DATA_TO_SEND.size() " << DESIRED_RED_DATA_TO_SEND.size() << std::endl;
            BOOST_REQUIRE_EQUAL(engineSrc.m_countAsyncSendCallbackCalls, DESIRED_RED_DATA_TO_SEND.size() + 2); //+2 for 1 Report ack and 1 resend CP_EOB
            BOOST_REQUIRE_EQUAL(engineSrc.m_countAsyncSendCallbackCalls, engineSrc.m_countAsyncSendCalls);


            BOOST_REQUIRE_EQUAL(engineDest.m_countAsyncSendCallbackCalls, 1); //1 for 1 Report segment

            BOOST_REQUIRE_EQUAL(engineDest.m_countAsyncSendCallbackCalls, engineDest.m_countAsyncSendCalls);
            BOOST_REQUIRE_EQUAL(numRedPartReceptionCallbacks, 1);
            BOOST_REQUIRE_EQUAL(numSessionStartSenderCallbacks, 1);
            BOOST_REQUIRE_EQUAL(numSessionStartReceiverCallbacks, 1);
            BOOST_REQUIRE_EQUAL(numGreenPartReceptionCallbacks, 0);
            BOOST_REQUIRE_EQUAL(numReceptionSessionCancelledCallbacks, 0);
            BOOST_REQUIRE_EQUAL(numTransmissionSessionCompletedCallbacks, 1);
            BOOST_REQUIRE_EQUAL(numInitialTransmissionCompletedCallbacks, 1);
            BOOST_REQUIRE_EQUAL(numTransmissionSessionCancelledCallbacks, 0);

            BOOST_REQUIRE_EQUAL(engineDest.m_numTimerExpiredCallbacks, 0);
            BOOST_REQUIRE_EQUAL(engineSrc.m_numTimerExpiredCallbacks, 1);
        }

        void DoTestDropRaSrcToDest() {
            struct DropSimulation {
                int count;
                DropSimulation() : count(0) {}
                bool DoSim(const uint8_t ltpHeaderByte) {
                    const LTP_SEGMENT_TYPE_FLAGS type = static_cast<LTP_SEGMENT_TYPE_FLAGS>(ltpHeaderByte);
                    if ((type == LTP_SEGMENT_TYPE_FLAGS::REPORT_ACK_SEGMENT)) {
                        ++count;
                        if (count == 1) {
                            std::cout << "drop\n";
                            return true;
                        }
                    }
                    return false;
                }
            };
            Reset();
            AssertNoActiveSendersAndReceivers();
            DropSimulation sim;
            engineSrc.m_udpDropSimulatorFunction = boost::bind(&DropSimulation::DoSim, &sim, boost::placeholders::_1);
            boost::shared_ptr<LtpEngine::transmission_request_t> tReq = boost::make_shared<LtpEngine::transmission_request_t>();
            tReq->destinationClientServiceId = CLIENT_SERVICE_ID_DEST;
            tReq->destinationLtpEngineId = ENGINE_ID_DEST;
            tReq->clientServiceDataToSend = std::vector<uint8_t>(DESIRED_RED_DATA_TO_SEND.data(), DESIRED_RED_DATA_TO_SEND.data() + DESIRED_RED_DATA_TO_SEND.size()); //copy
            tReq->lengthOfRedPart = DESIRED_RED_DATA_TO_SEND.size();
            engineSrc.TransmissionRequest_ThreadSafe(std::move(tReq));
            for (unsigned int i = 0; i < 50; ++i) {
                if (numRedPartReceptionCallbacks && numTransmissionSessionCompletedCallbacks && (engineDest.m_numTimerExpiredCallbacks == 1)) {
                    break;
                }
                cv.timed_wait(cvLock, boost::posix_time::milliseconds(200));
            }
            TryWaitForNoActiveSendersAndReceivers();
            AssertNoActiveSendersAndReceivers();
            //std::cout << "numSrcToDestDataExchanged " << numSrcToDestDataExchanged << " numDestToSrcDataExchanged " << numDestToSrcDataExchanged << " DESIRED_RED_DATA_TO_SEND.size() " << DESIRED_RED_DATA_TO_SEND.size() << std::endl;
            BOOST_REQUIRE_EQUAL(engineSrc.m_countAsyncSendCallbackCalls, DESIRED_RED_DATA_TO_SEND.size() + 2); //+2 for 1 Report ack and 1 resend Report Ack
            BOOST_REQUIRE_EQUAL(engineSrc.m_countAsyncSendCallbackCalls, engineSrc.m_countAsyncSendCalls);


            BOOST_REQUIRE_EQUAL(engineDest.m_countAsyncSendCallbackCalls, 2); //2 for 1 Report segment + 1 Resend Report Segment

            BOOST_REQUIRE_EQUAL(engineDest.m_countAsyncSendCallbackCalls, engineDest.m_countAsyncSendCalls);
            BOOST_REQUIRE_EQUAL(numRedPartReceptionCallbacks, 1);
            BOOST_REQUIRE_EQUAL(numSessionStartSenderCallbacks, 1);
            BOOST_REQUIRE_EQUAL(numSessionStartReceiverCallbacks, 1);
            BOOST_REQUIRE_EQUAL(numGreenPartReceptionCallbacks, 0);
            BOOST_REQUIRE_EQUAL(numReceptionSessionCancelledCallbacks, 0);
            BOOST_REQUIRE_EQUAL(numTransmissionSessionCompletedCallbacks, 1);
            BOOST_REQUIRE_EQUAL(numInitialTransmissionCompletedCallbacks, 1);
            BOOST_REQUIRE_EQUAL(numTransmissionSessionCancelledCallbacks, 0);

            BOOST_REQUIRE_EQUAL(engineDest.m_numTimerExpiredCallbacks, 1);
            BOOST_REQUIRE_EQUAL(engineSrc.m_numTimerExpiredCallbacks, 0);
        }
        /*
        void DoTestDropReportSegmentDestToSrc() {
            struct DropSimulationSecondEob {
                int count;
                DropSimulationSecondEob() : count(0) {}
                bool DoSim(const uint8_t ltpHeaderByte) {
                    const LTP_SEGMENT_TYPE_FLAGS type = static_cast<LTP_SEGMENT_TYPE_FLAGS>(ltpHeaderByte);
                    if ((type == LTP_SEGMENT_TYPE_FLAGS::REDDATA_CHECKPOINT_ENDOFREDPART) || (type == LTP_SEGMENT_TYPE_FLAGS::REDDATA_CHECKPOINT_ENDOFREDPART_ENDOFBLOCK)) {
                        ++count;
                        if (count == 2) {
                            return true;
                        }
                    }
                    return false;
                }
            };
            struct DropSimulationFirstRs {
                int count;
                DropSimulationFirstRs() : count(0) {}
                bool DoSim(const uint8_t ltpHeaderByte) {
                    const LTP_SEGMENT_TYPE_FLAGS type = static_cast<LTP_SEGMENT_TYPE_FLAGS>(ltpHeaderByte);
                    if (type == LTP_SEGMENT_TYPE_FLAGS::REPORT_SEGMENT) {
                        ++count;
                        if (count == 1) {
                            return true;
                        }
                    }
                    return false;
                }
            };
            Reset();
            AssertNoActiveSendersAndReceivers();
            DropSimulationSecondEob simEob;
            DropSimulationFirstRs simRs;
            engineSrc.m_udpDropSimulatorFunction = boost::bind(&DropSimulationSecondEob::DoSim, &simEob, boost::placeholders::_1);
            engineDest.m_udpDropSimulatorFunction = boost::bind(&DropSimulationFirstRs::DoSim, &simRs, boost::placeholders::_1);
            boost::shared_ptr<LtpEngine::transmission_request_t> tReq = boost::make_shared<LtpEngine::transmission_request_t>();
            tReq->destinationClientServiceId = CLIENT_SERVICE_ID_DEST;
            tReq->destinationLtpEngineId = ENGINE_ID_DEST;
            tReq->clientServiceDataToSend = std::vector<uint8_t>(DESIRED_RED_DATA_TO_SEND.data(), DESIRED_RED_DATA_TO_SEND.data() + DESIRED_RED_DATA_TO_SEND.size()); //copy
            tReq->lengthOfRedPart = DESIRED_RED_DATA_TO_SEND.size();
            engineSrc.TransmissionRequest_ThreadSafe(std::move(tReq));
            for (unsigned int i = 0; i < 50; ++i) {
                if (numRedPartReceptionCallbacks && numTransmissionSessionCompletedCallbacks) {
                    break;
                }
                cv.timed_wait(cvLock, boost::posix_time::milliseconds(200));
                //engineSrc.SignalReadyForSend_ThreadSafe();
                //engineDest.SignalReadyForSend_ThreadSafe();
            }
            boost::this_thread::sleep(boost::posix_time::milliseconds(500));
            AssertNoActiveSendersAndReceivers();
            //std::cout << "numSrcToDestDataExchanged " << numSrcToDestDataExchanged << " numDestToSrcDataExchanged " << numDestToSrcDataExchanged << " DESIRED_RED_DATA_TO_SEND.size() " << DESIRED_RED_DATA_TO_SEND.size() << std::endl;
            BOOST_REQUIRE_EQUAL(engineSrc.m_countAsyncSendCallbackCalls, DESIRED_RED_DATA_TO_SEND.size() + 2); //+2 for 1 Report ack and 1 resend CP_EOB
            BOOST_REQUIRE_EQUAL(engineSrc.m_countAsyncSendCallbackCalls, engineSrc.m_countAsyncSendCalls);


            BOOST_REQUIRE_EQUAL(engineDest.m_countAsyncSendCallbackCalls, 1); //1 for 1 Report segment

            BOOST_REQUIRE_EQUAL(engineDest.m_countAsyncSendCallbackCalls, engineDest.m_countAsyncSendCalls);
            BOOST_REQUIRE_EQUAL(numRedPartReceptionCallbacks, 1);
            BOOST_REQUIRE_EQUAL(numReceptionSessionCancelledCallbacks, 0);
            BOOST_REQUIRE_EQUAL(numTransmissionSessionCompletedCallbacks, 1);
            BOOST_REQUIRE_EQUAL(numInitialTransmissionCompletedCallbacks, 1);
            BOOST_REQUIRE_EQUAL(numTransmissionSessionCancelledCallbacks, 0);

            BOOST_REQUIRE_EQUAL(engineDest.m_numTimerExpiredCallbacks, 0);
            BOOST_REQUIRE_EQUAL(engineSrc.m_numTimerExpiredCallbacks, 1);
        }
        */


        //src checkpoint should expire until limit then send cancel segment to receiver
        void DoTestDropEOBAlwaysCheckpointDataSegmentSrcToDest() {
            struct DropSimulation {
                DropSimulation() {}
                bool DoSim(const uint8_t ltpHeaderByte) {
                    const LTP_SEGMENT_TYPE_FLAGS type = static_cast<LTP_SEGMENT_TYPE_FLAGS>(ltpHeaderByte);
                    return ((type == LTP_SEGMENT_TYPE_FLAGS::REDDATA_CHECKPOINT_ENDOFREDPART) || (type == LTP_SEGMENT_TYPE_FLAGS::REDDATA_CHECKPOINT_ENDOFREDPART_ENDOFBLOCK));
                }
            };
            Reset();
            AssertNoActiveSendersAndReceivers();
            DropSimulation sim;
            engineSrc.m_udpDropSimulatorFunction = boost::bind(&DropSimulation::DoSim, &sim, boost::placeholders::_1);
            boost::shared_ptr<LtpEngine::transmission_request_t> tReq = boost::make_shared<LtpEngine::transmission_request_t>();
            tReq->destinationClientServiceId = CLIENT_SERVICE_ID_DEST;
            tReq->destinationLtpEngineId = ENGINE_ID_DEST;
            tReq->clientServiceDataToSend = std::vector<uint8_t>(DESIRED_RED_DATA_TO_SEND.data(), DESIRED_RED_DATA_TO_SEND.data() + DESIRED_RED_DATA_TO_SEND.size()); //copy
            tReq->lengthOfRedPart = DESIRED_RED_DATA_TO_SEND.size();
            engineSrc.TransmissionRequest_ThreadSafe(std::move(tReq));
            for (unsigned int i = 0; i < 100; ++i) {
                if (numReceptionSessionCancelledCallbacks && numTransmissionSessionCancelledCallbacks) {
                    break;
                }
                cv.timed_wait(cvLock, boost::posix_time::milliseconds(500));
            }
            TryWaitForNoActiveSendersAndReceivers();
            AssertNoActiveSendersAndReceivers();
          
            BOOST_REQUIRE_EQUAL(engineSrc.m_countAsyncSendCallbackCalls, DESIRED_RED_DATA_TO_SEND.size() + 6); //+6 for 5 resend EOB and 1 cancel segment
            BOOST_REQUIRE_EQUAL(engineSrc.m_countAsyncSendCallbackCalls, engineSrc.m_countAsyncSendCalls);

            
            BOOST_REQUIRE_EQUAL(engineDest.m_countAsyncSendCallbackCalls, 1); // 1 for cancel ack

            BOOST_REQUIRE_EQUAL(engineDest.m_countAsyncSendCallbackCalls, engineDest.m_countAsyncSendCalls);
            BOOST_REQUIRE_EQUAL(numRedPartReceptionCallbacks, 0);
            BOOST_REQUIRE_EQUAL(numSessionStartSenderCallbacks, 1);
            BOOST_REQUIRE_EQUAL(numSessionStartReceiverCallbacks, 1);
            BOOST_REQUIRE_EQUAL(numReceptionSessionCancelledCallbacks, 1);
            BOOST_REQUIRE(lastReasonCode_receptionSessionCancelledCallback == CANCEL_SEGMENT_REASON_CODES::RLEXC);
            BOOST_REQUIRE_EQUAL(numTransmissionSessionCompletedCallbacks, 0);
            BOOST_REQUIRE_EQUAL(numInitialTransmissionCompletedCallbacks, 1);
            BOOST_REQUIRE_EQUAL(numTransmissionSessionCancelledCallbacks, 1);
            BOOST_REQUIRE(lastReasonCode_transmissionSessionCancelledCallback == CANCEL_SEGMENT_REASON_CODES::RLEXC);

            BOOST_REQUIRE_EQUAL(engineDest.m_numTimerExpiredCallbacks, 0);
            BOOST_REQUIRE_EQUAL(engineSrc.m_numTimerExpiredCallbacks, 6);
        }

        //dest RS timer should expire until limit then send cancel segment to sender
        void DoTestDropRaAlwaysSrcToDest() {
            struct DropSimulation {
                DropSimulation() {}
                bool DoSim(const uint8_t ltpHeaderByte) {
                    const LTP_SEGMENT_TYPE_FLAGS type = static_cast<LTP_SEGMENT_TYPE_FLAGS>(ltpHeaderByte);
                    return ((type == LTP_SEGMENT_TYPE_FLAGS::REPORT_ACK_SEGMENT));
                }
            };
            Reset();
            AssertNoActiveSendersAndReceivers();
            DropSimulation sim;
            engineSrc.m_udpDropSimulatorFunction = boost::bind(&DropSimulation::DoSim, &sim, boost::placeholders::_1);
            boost::shared_ptr<LtpEngine::transmission_request_t> tReq = boost::make_shared<LtpEngine::transmission_request_t>();
            tReq->destinationClientServiceId = CLIENT_SERVICE_ID_DEST;
            tReq->destinationLtpEngineId = ENGINE_ID_DEST;
            tReq->clientServiceDataToSend = std::vector<uint8_t>(DESIRED_RED_DATA_TO_SEND.data(), DESIRED_RED_DATA_TO_SEND.data() + DESIRED_RED_DATA_TO_SEND.size()); //copy
            tReq->lengthOfRedPart = DESIRED_RED_DATA_TO_SEND.size();
            engineSrc.TransmissionRequest_ThreadSafe(std::move(tReq));
            for (unsigned int i = 0; i < 100; ++i) {
                if (numReceptionSessionCancelledCallbacks && numTransmissionSessionCompletedCallbacks) {
                    break;
                }
                cv.timed_wait(cvLock, boost::posix_time::milliseconds(500));
            }
            TryWaitForNoActiveSendersAndReceivers();
            AssertNoActiveSendersAndReceivers();

            BOOST_REQUIRE_EQUAL(engineSrc.m_countAsyncSendCallbackCalls, DESIRED_RED_DATA_TO_SEND.size() + 7); //+7 for 6 RA and 1 CA
            BOOST_REQUIRE_EQUAL(engineSrc.m_countAsyncSendCallbackCalls, engineSrc.m_countAsyncSendCalls);


            BOOST_REQUIRE_EQUAL(engineDest.m_countAsyncSendCallbackCalls, 7); //7 for 1 RS, 5 resend RS, and 1 cancel segment

            BOOST_REQUIRE_EQUAL(engineDest.m_countAsyncSendCallbackCalls, engineDest.m_countAsyncSendCalls);
            BOOST_REQUIRE_EQUAL(numRedPartReceptionCallbacks, 1);
            BOOST_REQUIRE_EQUAL(numSessionStartSenderCallbacks, 1);
            BOOST_REQUIRE_EQUAL(numSessionStartReceiverCallbacks, 1);
            BOOST_REQUIRE_EQUAL(numReceptionSessionCancelledCallbacks, 1);
            BOOST_REQUIRE(lastReasonCode_receptionSessionCancelledCallback == CANCEL_SEGMENT_REASON_CODES::RLEXC);
            BOOST_REQUIRE_EQUAL(numTransmissionSessionCompletedCallbacks, 1);
            BOOST_REQUIRE_EQUAL(numInitialTransmissionCompletedCallbacks, 1);
            BOOST_REQUIRE_EQUAL(numTransmissionSessionCancelledCallbacks, 0);

            BOOST_REQUIRE_EQUAL(engineDest.m_numTimerExpiredCallbacks, 6);
            BOOST_REQUIRE_EQUAL(engineSrc.m_numTimerExpiredCallbacks, 0);
        }

        //src checkpoint should never make it to receiver, giving receiver time to cancel session
        void DoTestReceiverCancelSession() {
            struct DropSimulation {
                DropSimulation() {}
                bool DoSim(const uint8_t ltpHeaderByte) {
                    const LTP_SEGMENT_TYPE_FLAGS type = static_cast<LTP_SEGMENT_TYPE_FLAGS>(ltpHeaderByte);
                    return ((type == LTP_SEGMENT_TYPE_FLAGS::REDDATA_CHECKPOINT_ENDOFREDPART) || (type == LTP_SEGMENT_TYPE_FLAGS::REDDATA_CHECKPOINT_ENDOFREDPART_ENDOFBLOCK));
                }
            };
            Reset();
            AssertNoActiveSendersAndReceivers();
            DropSimulation sim;
            engineSrc.m_udpDropSimulatorFunction = boost::bind(&DropSimulation::DoSim, &sim, boost::placeholders::_1);
            boost::shared_ptr<LtpEngine::transmission_request_t> tReq = boost::make_shared<LtpEngine::transmission_request_t>();
            tReq->destinationClientServiceId = CLIENT_SERVICE_ID_DEST;
            tReq->destinationLtpEngineId = ENGINE_ID_DEST;
            tReq->clientServiceDataToSend = std::vector<uint8_t>(DESIRED_RED_DATA_TO_SEND.data(), DESIRED_RED_DATA_TO_SEND.data() + DESIRED_RED_DATA_TO_SEND.size()); //copy
            tReq->lengthOfRedPart = DESIRED_RED_DATA_TO_SEND.size();
            engineSrc.TransmissionRequest_ThreadSafe(std::move(tReq));
            for (unsigned int i = 0; i < 10; ++i) {
                if (numInitialTransmissionCompletedCallbacks) {
                    break;
                }
                cv.timed_wait(cvLock, boost::posix_time::milliseconds(250));
            }
            engineDest.CancellationRequest_ThreadSafe(lastSessionId_sessionStartSenderCallback); //cancel from receiver
            TryWaitForNoActiveSendersAndReceivers();
            AssertNoActiveSendersAndReceivers();

            BOOST_REQUIRE_EQUAL(engineSrc.m_countAsyncSendCallbackCalls, DESIRED_RED_DATA_TO_SEND.size() + 1); //+1 cancel ack
            BOOST_REQUIRE_EQUAL(engineSrc.m_countAsyncSendCallbackCalls, engineSrc.m_countAsyncSendCalls);


            BOOST_REQUIRE_EQUAL(engineDest.m_countAsyncSendCallbackCalls, 1); //1 cancel segment
            BOOST_REQUIRE_EQUAL(engineDest.m_countAsyncSendCallbackCalls, engineDest.m_countAsyncSendCalls);

            BOOST_REQUIRE_EQUAL(numRedPartReceptionCallbacks, 0);
            BOOST_REQUIRE_EQUAL(numSessionStartSenderCallbacks, 1);
            BOOST_REQUIRE_EQUAL(numSessionStartReceiverCallbacks, 1);
            BOOST_REQUIRE_EQUAL(numReceptionSessionCancelledCallbacks, 0);
            //BOOST_REQUIRE(lastReasonCode_receptionSessionCancelledCallback == CANCEL_SEGMENT_REASON_CODES::RLEXC);
            BOOST_REQUIRE_EQUAL(numTransmissionSessionCompletedCallbacks, 0);
            BOOST_REQUIRE_EQUAL(numInitialTransmissionCompletedCallbacks, 1);
            BOOST_REQUIRE_EQUAL(numTransmissionSessionCancelledCallbacks, 1);
            BOOST_REQUIRE(lastReasonCode_transmissionSessionCancelledCallback == CANCEL_SEGMENT_REASON_CODES::USER_CANCELLED);

            BOOST_REQUIRE_EQUAL(engineDest.m_numTimerExpiredCallbacks, 0);
            BOOST_REQUIRE_EQUAL(engineSrc.m_numTimerExpiredCallbacks, 0);
        }

        //src checkpoint should never make it to receiver, giving sender time to cancel session
        void DoTestSenderCancelSession() {
            struct DropSimulation {
                DropSimulation() {}
                bool DoSim(const uint8_t ltpHeaderByte) {
                    const LTP_SEGMENT_TYPE_FLAGS type = static_cast<LTP_SEGMENT_TYPE_FLAGS>(ltpHeaderByte);
                    return ((type == LTP_SEGMENT_TYPE_FLAGS::REDDATA_CHECKPOINT_ENDOFREDPART) || (type == LTP_SEGMENT_TYPE_FLAGS::REDDATA_CHECKPOINT_ENDOFREDPART_ENDOFBLOCK));
                }
            };
            Reset();
            AssertNoActiveSendersAndReceivers();
            DropSimulation sim;
            engineSrc.m_udpDropSimulatorFunction = boost::bind(&DropSimulation::DoSim, &sim, boost::placeholders::_1);
            boost::shared_ptr<LtpEngine::transmission_request_t> tReq = boost::make_shared<LtpEngine::transmission_request_t>();
            tReq->destinationClientServiceId = CLIENT_SERVICE_ID_DEST;
            tReq->destinationLtpEngineId = ENGINE_ID_DEST;
            tReq->clientServiceDataToSend = std::vector<uint8_t>(DESIRED_RED_DATA_TO_SEND.data(), DESIRED_RED_DATA_TO_SEND.data() + DESIRED_RED_DATA_TO_SEND.size()); //copy
            tReq->lengthOfRedPart = DESIRED_RED_DATA_TO_SEND.size();
            engineSrc.TransmissionRequest_ThreadSafe(std::move(tReq));
            for (unsigned int i = 0; i < 10; ++i) {
                if (numInitialTransmissionCompletedCallbacks) {
                    break;
                }
                cv.timed_wait(cvLock, boost::posix_time::milliseconds(250));
            }
            engineSrc.CancellationRequest_ThreadSafe(lastSessionId_sessionStartSenderCallback); //cancel from sender
            TryWaitForNoActiveSendersAndReceivers();
            AssertNoActiveSendersAndReceivers();

            BOOST_REQUIRE_EQUAL(engineSrc.m_countAsyncSendCallbackCalls, DESIRED_RED_DATA_TO_SEND.size() + 1); //+1 cancel req
            BOOST_REQUIRE_EQUAL(engineSrc.m_countAsyncSendCallbackCalls, engineSrc.m_countAsyncSendCalls);


            BOOST_REQUIRE_EQUAL(engineDest.m_countAsyncSendCallbackCalls, 1); //1 cancel ack
            BOOST_REQUIRE_EQUAL(engineDest.m_countAsyncSendCallbackCalls, engineDest.m_countAsyncSendCalls);

            BOOST_REQUIRE_EQUAL(numRedPartReceptionCallbacks, 0);
            BOOST_REQUIRE_EQUAL(numSessionStartSenderCallbacks, 1);
            BOOST_REQUIRE_EQUAL(numSessionStartReceiverCallbacks, 1);
            BOOST_REQUIRE_EQUAL(numReceptionSessionCancelledCallbacks, 1);
            BOOST_REQUIRE(lastReasonCode_receptionSessionCancelledCallback == CANCEL_SEGMENT_REASON_CODES::USER_CANCELLED);
            BOOST_REQUIRE_EQUAL(numTransmissionSessionCompletedCallbacks, 0);
            BOOST_REQUIRE_EQUAL(numInitialTransmissionCompletedCallbacks, 1);
            BOOST_REQUIRE_EQUAL(numTransmissionSessionCancelledCallbacks, 0);
            //BOOST_REQUIRE(lastReasonCode_transmissionSessionCancelledCallback == CANCEL_SEGMENT_REASON_CODES::USER_CANCELLED);

            BOOST_REQUIRE_EQUAL(engineDest.m_numTimerExpiredCallbacks, 0);
            BOOST_REQUIRE_EQUAL(engineSrc.m_numTimerExpiredCallbacks, 0);
        }

        void DoTestDropOddDataSegmentWithRsMtu() {
            struct DropSimulation {
                int count;
                DropSimulation() : count(0) {}
                bool DoSim(const uint8_t ltpHeaderByte) {
                    const LTP_SEGMENT_TYPE_FLAGS type = static_cast<LTP_SEGMENT_TYPE_FLAGS>(ltpHeaderByte);
                    if (type == LTP_SEGMENT_TYPE_FLAGS::REDDATA) {
                        ++count;
                        if ((count < 30) && (count & 1)) {
                            //std::cout << "drop odd\n";
                            return true;
                        }
                    }
                    return false;
                }
            };
            //expect:
            //  max reception claims = 3
            //  drop odd (printed 15x)                
            //  splitting 1 report segment with 15 reception claims into 5 report segments with no more than 3 reception claims per report segment
            Reset();
            engineDest.SetMtuReportSegment(110); // 110 bytes will result in 3 reception claims max
            AssertNoActiveSendersAndReceivers();
            DropSimulation sim;
            engineSrc.m_udpDropSimulatorFunction = boost::bind(&DropSimulation::DoSim, &sim, boost::placeholders::_1);
            boost::shared_ptr<LtpEngine::transmission_request_t> tReq = boost::make_shared<LtpEngine::transmission_request_t>();
            tReq->destinationClientServiceId = CLIENT_SERVICE_ID_DEST;
            tReq->destinationLtpEngineId = ENGINE_ID_DEST;
            tReq->clientServiceDataToSend = std::vector<uint8_t>(DESIRED_RED_DATA_TO_SEND.data(), DESIRED_RED_DATA_TO_SEND.data() + DESIRED_RED_DATA_TO_SEND.size()); //copy
            tReq->lengthOfRedPart = DESIRED_RED_DATA_TO_SEND.size();
            engineSrc.TransmissionRequest_ThreadSafe(std::move(tReq));
            for (unsigned int i = 0; i < 10; ++i) {
                if (numRedPartReceptionCallbacks && numTransmissionSessionCompletedCallbacks) {
                    break;
                }
                cv.timed_wait(cvLock, boost::posix_time::milliseconds(200));
            }
            TryWaitForNoActiveSendersAndReceivers();
            AssertNoActiveSendersAndReceivers();
            //std::cout << "numSrcToDestDataExchanged " << numSrcToDestDataExchanged << " numDestToSrcDataExchanged " << numDestToSrcDataExchanged << " DESIRED_RED_DATA_TO_SEND.size() " << DESIRED_RED_DATA_TO_SEND.size() << std::endl;
            BOOST_REQUIRE_EQUAL(engineSrc.m_countAsyncSendCallbackCalls, DESIRED_RED_DATA_TO_SEND.size() + 25); //+25 for 10 Report acks (see below) and 15 resends
            BOOST_REQUIRE_EQUAL(engineSrc.m_countAsyncSendCallbackCalls, engineSrc.m_countAsyncSendCalls);
            BOOST_REQUIRE_EQUAL(engineDest.m_countAsyncSendCallbackCalls, 10); //10 for 5 initial separately sentReport segments + 5 report segments of data complete as response to resends
            BOOST_REQUIRE_EQUAL(engineDest.m_countAsyncSendCallbackCalls, engineDest.m_countAsyncSendCalls);
            BOOST_REQUIRE_EQUAL(numRedPartReceptionCallbacks, 1);
            BOOST_REQUIRE_EQUAL(numSessionStartSenderCallbacks, 1);
            BOOST_REQUIRE_EQUAL(numSessionStartReceiverCallbacks, 1);
            BOOST_REQUIRE_EQUAL(numGreenPartReceptionCallbacks, 0);
            BOOST_REQUIRE_EQUAL(numReceptionSessionCancelledCallbacks, 0);
            BOOST_REQUIRE_EQUAL(numTransmissionSessionCompletedCallbacks, 1);
            BOOST_REQUIRE_EQUAL(numInitialTransmissionCompletedCallbacks, 1);
            BOOST_REQUIRE_EQUAL(numTransmissionSessionCancelledCallbacks, 0);

            engineDest.SetMtuReportSegment(UINT64_MAX); //restore to default unlimited reception claims
        }
        
    };

    Test t;
    t.DoTest();
    t.DoTestRedAndGreenData();
    t.DoTestFullyGreenData();
    t.DoTestOneDropDataSegmentSrcToDest();
    t.DoTestTwoDropDataSegmentSrcToDest();
    t.DoTestTwoDropDataSegmentSrcToDestRegularCheckpoints();
    t.DoTestDropOneCheckpointDataSegmentSrcToDest();
    t.DoTestDropEOBCheckpointDataSegmentSrcToDest();
    t.DoTestDropRaSrcToDest();
    t.DoTestDropEOBAlwaysCheckpointDataSegmentSrcToDest();
    t.DoTestDropRaAlwaysSrcToDest();
    t.DoTestReceiverCancelSession();
    t.DoTestSenderCancelSession();
    t.DoTestDropOddDataSegmentWithRsMtu();
}

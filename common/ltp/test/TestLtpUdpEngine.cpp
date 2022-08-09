/**
 * @file TestLtpUdpEngine.cpp
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
#include "LtpUdpEngineManager.h"
#include <boost/bind/bind.hpp>

BOOST_AUTO_TEST_CASE(LtpUdpEngineTestCase, *boost::unit_test::enabled())
{
    struct MyTransmissionUserData : public LtpTransmissionRequestUserData {
        MyTransmissionUserData() : m_data(0) {}
        MyTransmissionUserData(const unsigned int data) : m_data(data) {}
        virtual ~MyTransmissionUserData() {}

        unsigned int m_data;
    };

    struct Test {
        const boost::posix_time::time_duration ONE_WAY_LIGHT_TIME;
        const boost::posix_time::time_duration ONE_WAY_MARGIN_TIME;
        const uint64_t ENGINE_ID_SRC;
        const uint64_t ENGINE_ID_DEST;
        const uint64_t EXPECTED_SESSION_ORIGINATOR_ENGINE_ID;
        const uint64_t CLIENT_SERVICE_ID_DEST;
        const uint16_t BOUND_UDP_PORT_SRC;
        const uint16_t BOUND_UDP_PORT_DEST;
        std::shared_ptr<LtpUdpEngineManager> ltpUdpEngineManagerSrcPtr;
        std::shared_ptr<LtpUdpEngineManager> ltpUdpEngineManagerDestPtr;
        LtpUdpEngine * ltpUdpEngineSrcPtr;
        LtpUdpEngine * ltpUdpEngineDestPtr;
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

        volatile bool removeCallbackCalled;

        CANCEL_SEGMENT_REASON_CODES lastReasonCode_receptionSessionCancelledCallback;
        CANCEL_SEGMENT_REASON_CODES lastReasonCode_transmissionSessionCancelledCallback;
        Ltp::session_id_t lastSessionId_sessionStartSenderCallback;

        Test() :
            ONE_WAY_LIGHT_TIME(boost::posix_time::milliseconds(250)),
            ONE_WAY_MARGIN_TIME(boost::posix_time::milliseconds(250)),
            ENGINE_ID_SRC(100),
            ENGINE_ID_DEST(200),
            EXPECTED_SESSION_ORIGINATOR_ENGINE_ID(ENGINE_ID_SRC),
            CLIENT_SERVICE_ID_DEST(300),
            BOUND_UDP_PORT_SRC(12345),
            BOUND_UDP_PORT_DEST(1113),
            ltpUdpEngineManagerSrcPtr(LtpUdpEngineManager::GetOrCreateInstance(BOUND_UDP_PORT_SRC, true)),
            ltpUdpEngineManagerDestPtr(LtpUdpEngineManager::GetOrCreateInstance(BOUND_UDP_PORT_DEST, true)),
            //engineSrc(ENGINE_ID_SRC, 1, UINT64_MAX, ONE_WAY_LIGHT_TIME, ONE_WAY_MARGIN_TIME, 0, false, true),//1=> 1 CHARACTER AT A TIME, UINT64_MAX=> unlimited report segment size
            //engineDest(ENGINE_ID_DEST, 1, UINT64_MAX, ONE_WAY_LIGHT_TIME, ONE_WAY_MARGIN_TIME, 12345, true, true),//1=> MTU NOT USED AT THIS TIME, UINT64_MAX=> unlimited report segment size
            DESIRED_RED_DATA_TO_SEND("The quick brown fox jumps over the lazy dog!"),
            DESIRED_RED_AND_GREEN_DATA_TO_SEND("The quick brown fox jumps over the lazy dog!GGE"), //G=>green data not EOB, E=>green datat EOB
            DESIRED_FULLY_GREEN_DATA_TO_SEND("GGGGGGGGGGGGGGGGGE"),
            cvLock(cvMutex)
        {
            BOOST_REQUIRE(ltpUdpEngineManagerSrcPtr->StartIfNotAlreadyRunning()); //already running from constructor so should do nothing and return true 
            BOOST_REQUIRE(ltpUdpEngineManagerSrcPtr->StartIfNotAlreadyRunning()); //still already running so should do nothing and return true 

            ltpUdpEngineDestPtr = ltpUdpEngineManagerDestPtr->GetLtpUdpEnginePtrByRemoteEngineId(EXPECTED_SESSION_ORIGINATOR_ENGINE_ID, true); //sessionOriginatorEngineId is the remote engine id in the case of an induct
            if (ltpUdpEngineDestPtr == NULL) {
                ltpUdpEngineManagerDestPtr->AddLtpUdpEngine(ENGINE_ID_DEST, EXPECTED_SESSION_ORIGINATOR_ENGINE_ID, true, 1, UINT64_MAX, ONE_WAY_LIGHT_TIME, ONE_WAY_MARGIN_TIME, //1=> MTU NOT USED AT THIS TIME, UINT64_MAX=> unlimited report segment size
                    "localhost", BOUND_UDP_PORT_SRC, 100, 0, 10000000, 0, 5, false, 0, 5, 1000);
                ltpUdpEngineDestPtr = ltpUdpEngineManagerDestPtr->GetLtpUdpEnginePtrByRemoteEngineId(EXPECTED_SESSION_ORIGINATOR_ENGINE_ID, true);
            }
            ltpUdpEngineDestPtr->SetSessionStartCallback(boost::bind(&Test::SessionStartReceiverCallback, this, boost::placeholders::_1));
            ltpUdpEngineDestPtr->SetRedPartReceptionCallback(boost::bind(&Test::RedPartReceptionCallback, this, boost::placeholders::_1, boost::placeholders::_2, boost::placeholders::_3,
                boost::placeholders::_4, boost::placeholders::_5));
            ltpUdpEngineDestPtr->SetGreenPartSegmentArrivalCallback(boost::bind(&Test::GreenPartSegmentArrivalCallback, this, boost::placeholders::_1, boost::placeholders::_2, boost::placeholders::_3,
                boost::placeholders::_4, boost::placeholders::_5));
            ltpUdpEngineDestPtr->SetReceptionSessionCancelledCallback(boost::bind(&Test::ReceptionSessionCancelledCallback, this, boost::placeholders::_1, boost::placeholders::_2));

            ltpUdpEngineSrcPtr = ltpUdpEngineManagerSrcPtr->GetLtpUdpEnginePtrByRemoteEngineId(ENGINE_ID_DEST, false);
            if (ltpUdpEngineSrcPtr == NULL) {
                ltpUdpEngineManagerSrcPtr->AddLtpUdpEngine(ENGINE_ID_SRC, ENGINE_ID_DEST, false, 1, UINT64_MAX, ONE_WAY_LIGHT_TIME, ONE_WAY_MARGIN_TIME, //1=> MTU NOT USED AT THIS TIME, UINT64_MAX=> unlimited report segment size
                    "localhost", BOUND_UDP_PORT_DEST, 100, 0, 0, 0, 5, false, 0, 5, 0);
                ltpUdpEngineSrcPtr = ltpUdpEngineManagerSrcPtr->GetLtpUdpEnginePtrByRemoteEngineId(ENGINE_ID_DEST, false);
            }

            ltpUdpEngineSrcPtr->SetSessionStartCallback(boost::bind(&Test::SessionStartSenderCallback, this, boost::placeholders::_1));
            ltpUdpEngineSrcPtr->SetTransmissionSessionCompletedCallback(boost::bind(&Test::TransmissionSessionCompletedCallback, this, boost::placeholders::_1, boost::placeholders::_2));
            ltpUdpEngineSrcPtr->SetInitialTransmissionCompletedCallback(boost::bind(&Test::InitialTransmissionCompletedCallback, this, boost::placeholders::_1, boost::placeholders::_2));
            ltpUdpEngineSrcPtr->SetTransmissionSessionCancelledCallback(boost::bind(&Test::TransmissionSessionCancelledCallback, this, boost::placeholders::_1, boost::placeholders::_2, boost::placeholders::_3));

        }

        void RemoveCallback() {
            removeCallbackCalled = true;
        }

        ~Test() {
            removeCallbackCalled = false;
            //sessionOriginatorEngineId is the remote engine id in the case of an induct
            ltpUdpEngineManagerDestPtr->RemoveLtpUdpEngineByRemoteEngineId_ThreadSafe(EXPECTED_SESSION_ORIGINATOR_ENGINE_ID, true, boost::bind(&Test::RemoveCallback, this));
            boost::this_thread::sleep(boost::posix_time::milliseconds(100));
            for (unsigned int attempt = 0; attempt < 20; ++attempt) {
                if (removeCallbackCalled) {
                    break;
                }
                std::cout << "waiting to remove ltp dest (induct) for remote engine id " << EXPECTED_SESSION_ORIGINATOR_ENGINE_ID << std::endl;
                boost::this_thread::sleep(boost::posix_time::milliseconds(100));
            }
            std::cout << "removed ltp dest (induct) for remote engine id " << EXPECTED_SESSION_ORIGINATOR_ENGINE_ID << std::endl;

            removeCallbackCalled = false;
            ltpUdpEngineManagerSrcPtr->RemoveLtpUdpEngineByRemoteEngineId_ThreadSafe(ENGINE_ID_DEST, false, boost::bind(&Test::RemoveCallback, this));
            boost::this_thread::sleep(boost::posix_time::milliseconds(100));
            for (unsigned int attempt = 0; attempt < 20; ++attempt) {
                if (removeCallbackCalled) {
                    break;
                }
                std::cout << "waiting to remove ltp src (outduct) for remote engine id " << ENGINE_ID_DEST << std::endl;
                boost::this_thread::sleep(boost::posix_time::milliseconds(100));
            }
            std::cout << "removed ltp src (outduct) for remote engine id " << ENGINE_ID_DEST << std::endl;
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
        void RedPartReceptionCallback(const Ltp::session_id_t & sessionId, padded_vector_uint8_t & movableClientServiceDataVec, uint64_t lengthOfRedPart, uint64_t clientServiceId, bool isEndOfBlock) {
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
            BOOST_REQUIRE_EQUAL(movableClientServiceDataVec[0], (isEndOfBlock) ? 'E' : 'G');

            BOOST_REQUIRE(sessionId == lastSessionId_sessionStartSenderCallback);
            cv.notify_one();
        }
        void ReceptionSessionCancelledCallback(const Ltp::session_id_t & sessionId, CANCEL_SEGMENT_REASON_CODES reasonCode) {
            ++numReceptionSessionCancelledCallbacks;
            lastReasonCode_receptionSessionCancelledCallback = reasonCode;
            BOOST_REQUIRE(sessionId == lastSessionId_sessionStartSenderCallback);
        }
        void TransmissionSessionCompletedCallback(const Ltp::session_id_t & sessionId, std::shared_ptr<LtpTransmissionRequestUserData> & userDataPtr) {
            ++numTransmissionSessionCompletedCallbacks;
            BOOST_REQUIRE_EQUAL(userDataPtr.use_count(), 2); //ltp session sender copy + user copy
            MyTransmissionUserData * myUserData = dynamic_cast<MyTransmissionUserData*>(userDataPtr.get());
            BOOST_REQUIRE(myUserData);
            BOOST_REQUIRE_EQUAL(myUserData->m_data, 123);
            BOOST_REQUIRE(sessionId == lastSessionId_sessionStartSenderCallback);
            cv.notify_one();
        }
        void InitialTransmissionCompletedCallback(const Ltp::session_id_t & sessionId, std::shared_ptr<LtpTransmissionRequestUserData> & userDataPtr) {
            BOOST_REQUIRE_EQUAL(userDataPtr.use_count(), 2); //ltp session sender copy + user copy
            MyTransmissionUserData * myUserData = dynamic_cast<MyTransmissionUserData*>(userDataPtr.get());
            BOOST_REQUIRE(myUserData);
            BOOST_REQUIRE_EQUAL(myUserData->m_data, 123);
            BOOST_REQUIRE(sessionId == lastSessionId_sessionStartSenderCallback);
            ++numInitialTransmissionCompletedCallbacks;
        }
        void TransmissionSessionCancelledCallback(const Ltp::session_id_t & sessionId, CANCEL_SEGMENT_REASON_CODES reasonCode, std::shared_ptr<LtpTransmissionRequestUserData> & userDataPtr) {
            BOOST_REQUIRE_EQUAL(userDataPtr.use_count(), 2); //ltp session sender copy + user copy
            MyTransmissionUserData * myUserData = dynamic_cast<MyTransmissionUserData*>(userDataPtr.get());
            BOOST_REQUIRE(myUserData);
            BOOST_REQUIRE_EQUAL(myUserData->m_data, 123);
            ++numTransmissionSessionCancelledCallbacks;
            lastReasonCode_transmissionSessionCancelledCallback = reasonCode;
            BOOST_REQUIRE(sessionId == lastSessionId_sessionStartSenderCallback);
        }

        void Reset() {
            ltpUdpEngineSrcPtr->Reset();
            ltpUdpEngineDestPtr->Reset();
            ltpUdpEngineSrcPtr->SetCheckpointEveryNthDataPacketForSenders(0);
            ltpUdpEngineDestPtr->SetCheckpointEveryNthDataPacketForSenders(0);
            ltpUdpEngineSrcPtr->m_udpDropSimulatorFunction = LtpUdpEngine::UdpDropSimulatorFunction_t();
            ltpUdpEngineDestPtr->m_udpDropSimulatorFunction = LtpUdpEngine::UdpDropSimulatorFunction_t();
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
            BOOST_REQUIRE_EQUAL(ltpUdpEngineSrcPtr->NumActiveSenders(), 0);
            BOOST_REQUIRE_EQUAL(ltpUdpEngineSrcPtr->NumActiveReceivers(), 0);
            BOOST_REQUIRE_EQUAL(ltpUdpEngineDestPtr->NumActiveSenders(), 0);
            BOOST_REQUIRE_EQUAL(ltpUdpEngineDestPtr->NumActiveReceivers(), 0);
        }
        bool HasActiveSendersAndReceivers() {
            return ltpUdpEngineSrcPtr->NumActiveSenders() || ltpUdpEngineSrcPtr->NumActiveReceivers() || ltpUdpEngineDestPtr->NumActiveSenders() || ltpUdpEngineDestPtr->NumActiveReceivers();
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
            std::shared_ptr<MyTransmissionUserData> myUserData = std::make_shared<MyTransmissionUserData>(123);
            tReq->userDataPtr = myUserData; //keep a copy
            ltpUdpEngineSrcPtr->TransmissionRequest_ThreadSafe(std::move(tReq));
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

            BOOST_REQUIRE_EQUAL(ltpUdpEngineSrcPtr->m_countAsyncSendCallbackCalls, DESIRED_RED_DATA_TO_SEND.size() + 1); //+1 for Report ack
            BOOST_REQUIRE_EQUAL(ltpUdpEngineSrcPtr->m_countAsyncSendCallbackCalls, ltpUdpEngineSrcPtr->m_countAsyncSendCalls);
            BOOST_REQUIRE_EQUAL(ltpUdpEngineDestPtr->m_countAsyncSendCallbackCalls, 1); //1 for Report segment
            BOOST_REQUIRE_EQUAL(ltpUdpEngineDestPtr->m_countAsyncSendCallbackCalls, ltpUdpEngineDestPtr->m_countAsyncSendCalls);
        }

        void DoTestRedAndGreenData() {
            Reset();
            AssertNoActiveSendersAndReceivers();
            boost::shared_ptr<LtpEngine::transmission_request_t> tReq = boost::make_shared<LtpEngine::transmission_request_t>();
            tReq->destinationClientServiceId = CLIENT_SERVICE_ID_DEST;
            tReq->destinationLtpEngineId = ENGINE_ID_DEST;
            tReq->clientServiceDataToSend = std::vector<uint8_t>(DESIRED_RED_AND_GREEN_DATA_TO_SEND.data(), DESIRED_RED_AND_GREEN_DATA_TO_SEND.data() + DESIRED_RED_AND_GREEN_DATA_TO_SEND.size()); //copy
            tReq->lengthOfRedPart = DESIRED_RED_DATA_TO_SEND.size();
            std::shared_ptr<MyTransmissionUserData> myUserData = std::make_shared<MyTransmissionUserData>(123);
            tReq->userDataPtr = myUserData; //keep a copy
            ltpUdpEngineSrcPtr->TransmissionRequest_ThreadSafe(std::move(tReq));
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

            BOOST_REQUIRE_EQUAL(ltpUdpEngineSrcPtr->m_countAsyncSendCallbackCalls, DESIRED_RED_AND_GREEN_DATA_TO_SEND.size() + 1); //+1 for Report ack
            BOOST_REQUIRE_EQUAL(ltpUdpEngineSrcPtr->m_countAsyncSendCallbackCalls, ltpUdpEngineSrcPtr->m_countAsyncSendCalls);
            BOOST_REQUIRE_EQUAL(ltpUdpEngineDestPtr->m_countAsyncSendCallbackCalls, 1); //1 for Report segment
            BOOST_REQUIRE_EQUAL(ltpUdpEngineDestPtr->m_countAsyncSendCallbackCalls, ltpUdpEngineDestPtr->m_countAsyncSendCalls);
        }

        void DoTestFullyGreenData() {
            Reset();
            AssertNoActiveSendersAndReceivers();
            boost::shared_ptr<LtpEngine::transmission_request_t> tReq = boost::make_shared<LtpEngine::transmission_request_t>();
            tReq->destinationClientServiceId = CLIENT_SERVICE_ID_DEST;
            tReq->destinationLtpEngineId = ENGINE_ID_DEST;
            tReq->clientServiceDataToSend = std::vector<uint8_t>(DESIRED_FULLY_GREEN_DATA_TO_SEND.data(), DESIRED_FULLY_GREEN_DATA_TO_SEND.data() + DESIRED_FULLY_GREEN_DATA_TO_SEND.size()); //copy
            tReq->lengthOfRedPart = 0;
            std::shared_ptr<MyTransmissionUserData> myUserData = std::make_shared<MyTransmissionUserData>(123);
            tReq->userDataPtr = myUserData; //keep a copy
            ltpUdpEngineSrcPtr->TransmissionRequest_ThreadSafe(std::move(tReq));
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

            BOOST_REQUIRE_EQUAL(ltpUdpEngineSrcPtr->m_countAsyncSendCallbackCalls, DESIRED_FULLY_GREEN_DATA_TO_SEND.size());
            BOOST_REQUIRE_EQUAL(ltpUdpEngineSrcPtr->m_countAsyncSendCallbackCalls, ltpUdpEngineSrcPtr->m_countAsyncSendCalls);
            BOOST_REQUIRE_EQUAL(ltpUdpEngineDestPtr->m_countAsyncSendCallbackCalls, 0);
            BOOST_REQUIRE_EQUAL(ltpUdpEngineDestPtr->m_countAsyncSendCallbackCalls, ltpUdpEngineDestPtr->m_countAsyncSendCalls);
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
            ltpUdpEngineSrcPtr->m_udpDropSimulatorFunction = boost::bind(&DropOneSimulation::DoSim, &sim, boost::placeholders::_1);
            boost::shared_ptr<LtpEngine::transmission_request_t> tReq = boost::make_shared<LtpEngine::transmission_request_t>();
            tReq->destinationClientServiceId = CLIENT_SERVICE_ID_DEST;
            tReq->destinationLtpEngineId = ENGINE_ID_DEST;
            tReq->clientServiceDataToSend = std::vector<uint8_t>(DESIRED_RED_DATA_TO_SEND.data(), DESIRED_RED_DATA_TO_SEND.data() + DESIRED_RED_DATA_TO_SEND.size()); //copy
            tReq->lengthOfRedPart = DESIRED_RED_DATA_TO_SEND.size();
            std::shared_ptr<MyTransmissionUserData> myUserData = std::make_shared<MyTransmissionUserData>(123);
            tReq->userDataPtr = myUserData; //keep a copy
            ltpUdpEngineSrcPtr->TransmissionRequest_ThreadSafe(std::move(tReq));
            for (unsigned int i = 0; i < 10; ++i) {
                if (numRedPartReceptionCallbacks && numTransmissionSessionCompletedCallbacks) {
                    break;
                }
                cv.timed_wait(cvLock, boost::posix_time::milliseconds(200));
            }
            TryWaitForNoActiveSendersAndReceivers();
            AssertNoActiveSendersAndReceivers();
            //std::cout << "numSrcToDestDataExchanged " << numSrcToDestDataExchanged << " numDestToSrcDataExchanged " << numDestToSrcDataExchanged << " DESIRED_RED_DATA_TO_SEND.size() " << DESIRED_RED_DATA_TO_SEND.size() << std::endl;
            BOOST_REQUIRE_EQUAL(ltpUdpEngineSrcPtr->m_countAsyncSendCallbackCalls, DESIRED_RED_DATA_TO_SEND.size() + 3); //+3 for 2 Report acks and 1 resend
            BOOST_REQUIRE_EQUAL(ltpUdpEngineSrcPtr->m_countAsyncSendCallbackCalls, ltpUdpEngineSrcPtr->m_countAsyncSendCalls);
            BOOST_REQUIRE_EQUAL(ltpUdpEngineDestPtr->m_countAsyncSendCallbackCalls, 2); //2 for 2 Report segments
            BOOST_REQUIRE_EQUAL(ltpUdpEngineDestPtr->m_countAsyncSendCallbackCalls, ltpUdpEngineDestPtr->m_countAsyncSendCalls);
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
            ltpUdpEngineSrcPtr->m_udpDropSimulatorFunction = boost::bind(&DropTwoSimulation::DoSim, &sim, boost::placeholders::_1);
            boost::shared_ptr<LtpEngine::transmission_request_t> tReq = boost::make_shared<LtpEngine::transmission_request_t>();
            tReq->destinationClientServiceId = CLIENT_SERVICE_ID_DEST;
            tReq->destinationLtpEngineId = ENGINE_ID_DEST;
            tReq->clientServiceDataToSend = std::vector<uint8_t>(DESIRED_RED_DATA_TO_SEND.data(), DESIRED_RED_DATA_TO_SEND.data() + DESIRED_RED_DATA_TO_SEND.size()); //copy
            tReq->lengthOfRedPart = DESIRED_RED_DATA_TO_SEND.size();
            std::shared_ptr<MyTransmissionUserData> myUserData = std::make_shared<MyTransmissionUserData>(123);
            tReq->userDataPtr = myUserData; //keep a copy
            ltpUdpEngineSrcPtr->TransmissionRequest_ThreadSafe(std::move(tReq));
            for (unsigned int i = 0; i < 10; ++i) {
                if (numRedPartReceptionCallbacks && numTransmissionSessionCompletedCallbacks) {
                    break;
                }
                cv.timed_wait(cvLock, boost::posix_time::milliseconds(200));
            }
            TryWaitForNoActiveSendersAndReceivers();
            AssertNoActiveSendersAndReceivers();
            //std::cout << "numSrcToDestDataExchanged " << numSrcToDestDataExchanged << " numDestToSrcDataExchanged " << numDestToSrcDataExchanged << " DESIRED_RED_DATA_TO_SEND.size() " << DESIRED_RED_DATA_TO_SEND.size() << std::endl;
            BOOST_REQUIRE_EQUAL(ltpUdpEngineSrcPtr->m_countAsyncSendCallbackCalls, DESIRED_RED_DATA_TO_SEND.size() + 4); //+4 for 2 Report acks and 2 resends
            BOOST_REQUIRE_EQUAL(ltpUdpEngineSrcPtr->m_countAsyncSendCallbackCalls, ltpUdpEngineSrcPtr->m_countAsyncSendCalls);
            BOOST_REQUIRE_EQUAL(ltpUdpEngineDestPtr->m_countAsyncSendCallbackCalls, 2); //2 for 2 Report segments
            BOOST_REQUIRE_EQUAL(ltpUdpEngineDestPtr->m_countAsyncSendCallbackCalls, ltpUdpEngineDestPtr->m_countAsyncSendCalls);
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
            ltpUdpEngineSrcPtr->m_udpDropSimulatorFunction = boost::bind(&DropTwoSimulation::DoSim, &sim, boost::placeholders::_1);
            boost::shared_ptr<LtpEngine::transmission_request_t> tReq = boost::make_shared<LtpEngine::transmission_request_t>();
            tReq->destinationClientServiceId = CLIENT_SERVICE_ID_DEST;
            tReq->destinationLtpEngineId = ENGINE_ID_DEST;
            tReq->clientServiceDataToSend = std::vector<uint8_t>(DESIRED_RED_DATA_TO_SEND.data(), DESIRED_RED_DATA_TO_SEND.data() + DESIRED_RED_DATA_TO_SEND.size()); //copy
            tReq->lengthOfRedPart = DESIRED_RED_DATA_TO_SEND.size();
            std::shared_ptr<MyTransmissionUserData> myUserData = std::make_shared<MyTransmissionUserData>(123);
            tReq->userDataPtr = myUserData; //keep a copy
            ltpUdpEngineSrcPtr->SetCheckpointEveryNthDataPacketForSenders(5);
            ltpUdpEngineSrcPtr->TransmissionRequest_ThreadSafe(std::move(tReq));
            for (unsigned int i = 0; i < 10; ++i) {
                if (numRedPartReceptionCallbacks && numTransmissionSessionCompletedCallbacks) {
                    break;
                }
                cv.timed_wait(cvLock, boost::posix_time::milliseconds(200));
            }
            TryWaitForNoActiveSendersAndReceivers();
            AssertNoActiveSendersAndReceivers();
            //std::cout << "numSrcToDestDataExchanged " << numSrcToDestDataExchanged << " numDestToSrcDataExchanged " << numDestToSrcDataExchanged << " DESIRED_RED_DATA_TO_SEND.size() " << DESIRED_RED_DATA_TO_SEND.size() << std::endl;
            BOOST_REQUIRE_EQUAL(ltpUdpEngineSrcPtr->m_countAsyncSendCallbackCalls, DESIRED_RED_DATA_TO_SEND.size() + 13); //+13 for 11 Report acks (see next line) and 2 resends
            BOOST_REQUIRE_EQUAL(ltpUdpEngineSrcPtr->m_countAsyncSendCallbackCalls, ltpUdpEngineSrcPtr->m_countAsyncSendCalls);

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
            BOOST_REQUIRE_EQUAL(ltpUdpEngineDestPtr->m_countAsyncSendCallbackCalls, 11); // 44/5=8 + (1 eobCp at 44) + 2 retrans report 

            BOOST_REQUIRE_EQUAL(ltpUdpEngineDestPtr->m_countAsyncSendCallbackCalls, ltpUdpEngineDestPtr->m_countAsyncSendCalls);
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
            ltpUdpEngineSrcPtr->m_udpDropSimulatorFunction = boost::bind(&DropSimulation::DoSim, &sim, boost::placeholders::_1);
            boost::shared_ptr<LtpEngine::transmission_request_t> tReq = boost::make_shared<LtpEngine::transmission_request_t>();
            tReq->destinationClientServiceId = CLIENT_SERVICE_ID_DEST;
            tReq->destinationLtpEngineId = ENGINE_ID_DEST;
            tReq->clientServiceDataToSend = std::vector<uint8_t>(DESIRED_RED_DATA_TO_SEND.data(), DESIRED_RED_DATA_TO_SEND.data() + DESIRED_RED_DATA_TO_SEND.size()); //copy
            tReq->lengthOfRedPart = DESIRED_RED_DATA_TO_SEND.size();
            std::shared_ptr<MyTransmissionUserData> myUserData = std::make_shared<MyTransmissionUserData>(123);
            tReq->userDataPtr = myUserData; //keep a copy
            ltpUdpEngineSrcPtr->SetCheckpointEveryNthDataPacketForSenders(5);
            ltpUdpEngineSrcPtr->TransmissionRequest_ThreadSafe(std::move(tReq));
            for (unsigned int i = 0; i < 50; ++i) {
                if (numRedPartReceptionCallbacks && numTransmissionSessionCompletedCallbacks) {
                    break;
                }
                cv.timed_wait(cvLock, boost::posix_time::milliseconds(200));
            }
            TryWaitForNoActiveSendersAndReceivers();
            AssertNoActiveSendersAndReceivers();
            //std::cout << "numSrcToDestDataExchanged " << numSrcToDestDataExchanged << " numDestToSrcDataExchanged " << numDestToSrcDataExchanged << " DESIRED_RED_DATA_TO_SEND.size() " << DESIRED_RED_DATA_TO_SEND.size() << std::endl;
            BOOST_REQUIRE_EQUAL(ltpUdpEngineSrcPtr->m_countAsyncSendCallbackCalls, DESIRED_RED_DATA_TO_SEND.size() + 10); //+10 for 9 Report acks (see next line) and 1 resend
            BOOST_REQUIRE_EQUAL(ltpUdpEngineSrcPtr->m_countAsyncSendCallbackCalls, ltpUdpEngineSrcPtr->m_countAsyncSendCalls);

            //primary first LB: 0, UB: 5
            //primary subsequent LB : 5, UB : 15
            //primary subsequent LB : 15, UB : 20
            //secondary LB : 5, UB : 10
            //primary subsequent LB : 20, UB : 25
            //primary subsequent LB : 25, UB : 30
            //primary subsequent LB : 30, UB : 35
            //primary subsequent LB : 35, UB : 40
            //primary subsequent LB : 40, UB : 44
            BOOST_REQUIRE_EQUAL(ltpUdpEngineDestPtr->m_countAsyncSendCallbackCalls, 9); // 44/5-1=7 + (1 eobCp at 44) + 1 retrans report 

            BOOST_REQUIRE_EQUAL(ltpUdpEngineDestPtr->m_countAsyncSendCallbackCalls, ltpUdpEngineDestPtr->m_countAsyncSendCalls);
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
            ltpUdpEngineSrcPtr->m_udpDropSimulatorFunction = boost::bind(&DropSimulation::DoSim, &sim, boost::placeholders::_1);
            boost::shared_ptr<LtpEngine::transmission_request_t> tReq = boost::make_shared<LtpEngine::transmission_request_t>();
            tReq->destinationClientServiceId = CLIENT_SERVICE_ID_DEST;
            tReq->destinationLtpEngineId = ENGINE_ID_DEST;
            tReq->clientServiceDataToSend = std::vector<uint8_t>(DESIRED_RED_DATA_TO_SEND.data(), DESIRED_RED_DATA_TO_SEND.data() + DESIRED_RED_DATA_TO_SEND.size()); //copy
            tReq->lengthOfRedPart = DESIRED_RED_DATA_TO_SEND.size();
            std::shared_ptr<MyTransmissionUserData> myUserData = std::make_shared<MyTransmissionUserData>(123);
            tReq->userDataPtr = myUserData; //keep a copy
            ltpUdpEngineSrcPtr->TransmissionRequest_ThreadSafe(std::move(tReq));
            for (unsigned int i = 0; i < 50; ++i) {
                if (numRedPartReceptionCallbacks && numTransmissionSessionCompletedCallbacks) {
                    break;
                }
                cv.timed_wait(cvLock, boost::posix_time::milliseconds(200));
            }
            TryWaitForNoActiveSendersAndReceivers();
            AssertNoActiveSendersAndReceivers();
            //std::cout << "numSrcToDestDataExchanged " << numSrcToDestDataExchanged << " numDestToSrcDataExchanged " << numDestToSrcDataExchanged << " DESIRED_RED_DATA_TO_SEND.size() " << DESIRED_RED_DATA_TO_SEND.size() << std::endl;
            BOOST_REQUIRE_EQUAL(ltpUdpEngineSrcPtr->m_countAsyncSendCallbackCalls, DESIRED_RED_DATA_TO_SEND.size() + 2); //+2 for 1 Report ack and 1 resend CP_EOB
            BOOST_REQUIRE_EQUAL(ltpUdpEngineSrcPtr->m_countAsyncSendCallbackCalls, ltpUdpEngineSrcPtr->m_countAsyncSendCalls);


            BOOST_REQUIRE_EQUAL(ltpUdpEngineDestPtr->m_countAsyncSendCallbackCalls, 1); //1 for 1 Report segment

            BOOST_REQUIRE_EQUAL(ltpUdpEngineDestPtr->m_countAsyncSendCallbackCalls, ltpUdpEngineDestPtr->m_countAsyncSendCalls);
            BOOST_REQUIRE_EQUAL(numRedPartReceptionCallbacks, 1);
            BOOST_REQUIRE_EQUAL(numSessionStartSenderCallbacks, 1);
            BOOST_REQUIRE_EQUAL(numSessionStartReceiverCallbacks, 1);
            BOOST_REQUIRE_EQUAL(numGreenPartReceptionCallbacks, 0);
            BOOST_REQUIRE_EQUAL(numReceptionSessionCancelledCallbacks, 0);
            BOOST_REQUIRE_EQUAL(numTransmissionSessionCompletedCallbacks, 1);
            BOOST_REQUIRE_EQUAL(numInitialTransmissionCompletedCallbacks, 1);
            BOOST_REQUIRE_EQUAL(numTransmissionSessionCancelledCallbacks, 0);

            BOOST_REQUIRE_EQUAL(ltpUdpEngineDestPtr->m_numReportSegmentTimerExpiredCallbacks, 0);
            BOOST_REQUIRE_EQUAL(ltpUdpEngineDestPtr->m_numCheckpointTimerExpiredCallbacks, 0);
            BOOST_REQUIRE_EQUAL(ltpUdpEngineSrcPtr->m_numReportSegmentTimerExpiredCallbacks, 0);
            BOOST_REQUIRE_EQUAL(ltpUdpEngineSrcPtr->m_numCheckpointTimerExpiredCallbacks, 1);
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
            ltpUdpEngineSrcPtr->m_udpDropSimulatorFunction = boost::bind(&DropSimulation::DoSim, &sim, boost::placeholders::_1);
            boost::shared_ptr<LtpEngine::transmission_request_t> tReq = boost::make_shared<LtpEngine::transmission_request_t>();
            tReq->destinationClientServiceId = CLIENT_SERVICE_ID_DEST;
            tReq->destinationLtpEngineId = ENGINE_ID_DEST;
            tReq->clientServiceDataToSend = std::vector<uint8_t>(DESIRED_RED_DATA_TO_SEND.data(), DESIRED_RED_DATA_TO_SEND.data() + DESIRED_RED_DATA_TO_SEND.size()); //copy
            tReq->lengthOfRedPart = DESIRED_RED_DATA_TO_SEND.size();
            std::shared_ptr<MyTransmissionUserData> myUserData = std::make_shared<MyTransmissionUserData>(123);
            tReq->userDataPtr = myUserData; //keep a copy
            ltpUdpEngineSrcPtr->TransmissionRequest_ThreadSafe(std::move(tReq));
            for (unsigned int i = 0; i < 50; ++i) {
                if (numRedPartReceptionCallbacks && numTransmissionSessionCompletedCallbacks && (ltpUdpEngineDestPtr->m_numReportSegmentTimerExpiredCallbacks == 1)) {
                    break;
                }
                cv.timed_wait(cvLock, boost::posix_time::milliseconds(200));
            }
            TryWaitForNoActiveSendersAndReceivers();
            AssertNoActiveSendersAndReceivers();
            //std::cout << "numSrcToDestDataExchanged " << numSrcToDestDataExchanged << " numDestToSrcDataExchanged " << numDestToSrcDataExchanged << " DESIRED_RED_DATA_TO_SEND.size() " << DESIRED_RED_DATA_TO_SEND.size() << std::endl;
            BOOST_REQUIRE_EQUAL(ltpUdpEngineSrcPtr->m_countAsyncSendCallbackCalls, DESIRED_RED_DATA_TO_SEND.size() + 2); //+2 for 1 Report ack and 1 resend Report Ack
            BOOST_REQUIRE_EQUAL(ltpUdpEngineSrcPtr->m_countAsyncSendCallbackCalls, ltpUdpEngineSrcPtr->m_countAsyncSendCalls);


            BOOST_REQUIRE_EQUAL(ltpUdpEngineDestPtr->m_countAsyncSendCallbackCalls, 2); //2 for 1 Report segment + 1 Resend Report Segment

            BOOST_REQUIRE_EQUAL(ltpUdpEngineDestPtr->m_countAsyncSendCallbackCalls, ltpUdpEngineDestPtr->m_countAsyncSendCalls);
            BOOST_REQUIRE_EQUAL(numRedPartReceptionCallbacks, 1);
            BOOST_REQUIRE_EQUAL(numSessionStartSenderCallbacks, 1);
            BOOST_REQUIRE_EQUAL(numSessionStartReceiverCallbacks, 1);
            BOOST_REQUIRE_EQUAL(numGreenPartReceptionCallbacks, 0);
            BOOST_REQUIRE_EQUAL(numReceptionSessionCancelledCallbacks, 0);
            BOOST_REQUIRE_EQUAL(numTransmissionSessionCompletedCallbacks, 1);
            BOOST_REQUIRE_EQUAL(numInitialTransmissionCompletedCallbacks, 1);
            BOOST_REQUIRE_EQUAL(numTransmissionSessionCancelledCallbacks, 0);

            BOOST_REQUIRE_EQUAL(ltpUdpEngineDestPtr->m_numReportSegmentTimerExpiredCallbacks, 1);
            BOOST_REQUIRE_EQUAL(ltpUdpEngineDestPtr->m_numCheckpointTimerExpiredCallbacks, 0);
            BOOST_REQUIRE_EQUAL(ltpUdpEngineSrcPtr->m_numReportSegmentTimerExpiredCallbacks, 0);
            BOOST_REQUIRE_EQUAL(ltpUdpEngineSrcPtr->m_numCheckpointTimerExpiredCallbacks, 0);
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
            ltpUdpEngineSrcPtr->m_udpDropSimulatorFunction = boost::bind(&DropSimulationSecondEob::DoSim, &simEob, boost::placeholders::_1);
            ltpUdpEngineDestPtr->m_udpDropSimulatorFunction = boost::bind(&DropSimulationFirstRs::DoSim, &simRs, boost::placeholders::_1);
            boost::shared_ptr<LtpEngine::transmission_request_t> tReq = boost::make_shared<LtpEngine::transmission_request_t>();
            tReq->destinationClientServiceId = CLIENT_SERVICE_ID_DEST;
            tReq->destinationLtpEngineId = ENGINE_ID_DEST;
            tReq->clientServiceDataToSend = std::vector<uint8_t>(DESIRED_RED_DATA_TO_SEND.data(), DESIRED_RED_DATA_TO_SEND.data() + DESIRED_RED_DATA_TO_SEND.size()); //copy
            tReq->lengthOfRedPart = DESIRED_RED_DATA_TO_SEND.size();
            ltpUdpEngineSrcPtr->TransmissionRequest_ThreadSafe(std::move(tReq));
            for (unsigned int i = 0; i < 50; ++i) {
                if (numRedPartReceptionCallbacks && numTransmissionSessionCompletedCallbacks) {
                    break;
                }
                cv.timed_wait(cvLock, boost::posix_time::milliseconds(200));
                //ltpUdpEngineSrcPtr->SignalReadyForSend_ThreadSafe();
                //ltpUdpEngineDestPtr->SignalReadyForSend_ThreadSafe();
            }
            boost::this_thread::sleep(boost::posix_time::milliseconds(500));
            AssertNoActiveSendersAndReceivers();
            //std::cout << "numSrcToDestDataExchanged " << numSrcToDestDataExchanged << " numDestToSrcDataExchanged " << numDestToSrcDataExchanged << " DESIRED_RED_DATA_TO_SEND.size() " << DESIRED_RED_DATA_TO_SEND.size() << std::endl;
            BOOST_REQUIRE_EQUAL(ltpUdpEngineSrcPtr->m_countAsyncSendCallbackCalls, DESIRED_RED_DATA_TO_SEND.size() + 2); //+2 for 1 Report ack and 1 resend CP_EOB
            BOOST_REQUIRE_EQUAL(ltpUdpEngineSrcPtr->m_countAsyncSendCallbackCalls, ltpUdpEngineSrcPtr->m_countAsyncSendCalls);


            BOOST_REQUIRE_EQUAL(ltpUdpEngineDestPtr->m_countAsyncSendCallbackCalls, 1); //1 for 1 Report segment

            BOOST_REQUIRE_EQUAL(ltpUdpEngineDestPtr->m_countAsyncSendCallbackCalls, ltpUdpEngineDestPtr->m_countAsyncSendCalls);
            BOOST_REQUIRE_EQUAL(numRedPartReceptionCallbacks, 1);
            BOOST_REQUIRE_EQUAL(numReceptionSessionCancelledCallbacks, 0);
            BOOST_REQUIRE_EQUAL(numTransmissionSessionCompletedCallbacks, 1);
            BOOST_REQUIRE_EQUAL(numInitialTransmissionCompletedCallbacks, 1);
            BOOST_REQUIRE_EQUAL(numTransmissionSessionCancelledCallbacks, 0);

            BOOST_REQUIRE_EQUAL(ltpUdpEngineDestPtr->m_numTimerExpiredCallbacks, 0);
            BOOST_REQUIRE_EQUAL(ltpUdpEngineSrcPtr->m_numTimerExpiredCallbacks, 1);
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
            ltpUdpEngineSrcPtr->m_udpDropSimulatorFunction = boost::bind(&DropSimulation::DoSim, &sim, boost::placeholders::_1);
            boost::shared_ptr<LtpEngine::transmission_request_t> tReq = boost::make_shared<LtpEngine::transmission_request_t>();
            tReq->destinationClientServiceId = CLIENT_SERVICE_ID_DEST;
            tReq->destinationLtpEngineId = ENGINE_ID_DEST;
            tReq->clientServiceDataToSend = std::vector<uint8_t>(DESIRED_RED_DATA_TO_SEND.data(), DESIRED_RED_DATA_TO_SEND.data() + DESIRED_RED_DATA_TO_SEND.size()); //copy
            tReq->lengthOfRedPart = DESIRED_RED_DATA_TO_SEND.size();
            std::shared_ptr<MyTransmissionUserData> myUserData = std::make_shared<MyTransmissionUserData>(123);
            tReq->userDataPtr = myUserData; //keep a copy
            ltpUdpEngineSrcPtr->TransmissionRequest_ThreadSafe(std::move(tReq));
            for (unsigned int i = 0; i < 100; ++i) {
                if (numReceptionSessionCancelledCallbacks && numTransmissionSessionCancelledCallbacks) {
                    break;
                }
                cv.timed_wait(cvLock, boost::posix_time::milliseconds(500));
            }
            TryWaitForNoActiveSendersAndReceivers();
            AssertNoActiveSendersAndReceivers();
          
            BOOST_REQUIRE_EQUAL(ltpUdpEngineSrcPtr->m_countAsyncSendCallbackCalls, DESIRED_RED_DATA_TO_SEND.size() + 6); //+6 for 5 resend EOB and 1 cancel segment
            BOOST_REQUIRE_EQUAL(ltpUdpEngineSrcPtr->m_countAsyncSendCallbackCalls, ltpUdpEngineSrcPtr->m_countAsyncSendCalls);

            
            BOOST_REQUIRE_EQUAL(ltpUdpEngineDestPtr->m_countAsyncSendCallbackCalls, 1); // 1 for cancel ack

            BOOST_REQUIRE_EQUAL(ltpUdpEngineDestPtr->m_countAsyncSendCallbackCalls, ltpUdpEngineDestPtr->m_countAsyncSendCalls);
            BOOST_REQUIRE_EQUAL(numRedPartReceptionCallbacks, 0);
            BOOST_REQUIRE_EQUAL(numSessionStartSenderCallbacks, 1);
            BOOST_REQUIRE_EQUAL(numSessionStartReceiverCallbacks, 1);
            BOOST_REQUIRE_EQUAL(numReceptionSessionCancelledCallbacks, 1);
            BOOST_REQUIRE(lastReasonCode_receptionSessionCancelledCallback == CANCEL_SEGMENT_REASON_CODES::RLEXC);
            BOOST_REQUIRE_EQUAL(numTransmissionSessionCompletedCallbacks, 0);
            BOOST_REQUIRE_EQUAL(numInitialTransmissionCompletedCallbacks, 1);
            BOOST_REQUIRE_EQUAL(numTransmissionSessionCancelledCallbacks, 1);
            BOOST_REQUIRE(lastReasonCode_transmissionSessionCancelledCallback == CANCEL_SEGMENT_REASON_CODES::RLEXC);

            BOOST_REQUIRE_EQUAL(ltpUdpEngineDestPtr->m_numReportSegmentTimerExpiredCallbacks, 0);
            BOOST_REQUIRE_EQUAL(ltpUdpEngineDestPtr->m_numCheckpointTimerExpiredCallbacks, 0);
            BOOST_REQUIRE_EQUAL(ltpUdpEngineSrcPtr->m_numReportSegmentTimerExpiredCallbacks, 0);
            BOOST_REQUIRE_EQUAL(ltpUdpEngineSrcPtr->m_numCheckpointTimerExpiredCallbacks, 6);
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
            ltpUdpEngineSrcPtr->m_udpDropSimulatorFunction = boost::bind(&DropSimulation::DoSim, &sim, boost::placeholders::_1);
            boost::shared_ptr<LtpEngine::transmission_request_t> tReq = boost::make_shared<LtpEngine::transmission_request_t>();
            tReq->destinationClientServiceId = CLIENT_SERVICE_ID_DEST;
            tReq->destinationLtpEngineId = ENGINE_ID_DEST;
            tReq->clientServiceDataToSend = std::vector<uint8_t>(DESIRED_RED_DATA_TO_SEND.data(), DESIRED_RED_DATA_TO_SEND.data() + DESIRED_RED_DATA_TO_SEND.size()); //copy
            tReq->lengthOfRedPart = DESIRED_RED_DATA_TO_SEND.size();
            std::shared_ptr<MyTransmissionUserData> myUserData = std::make_shared<MyTransmissionUserData>(123);
            tReq->userDataPtr = myUserData; //keep a copy
            ltpUdpEngineSrcPtr->TransmissionRequest_ThreadSafe(std::move(tReq));
            for (unsigned int i = 0; i < 100; ++i) {
                if (numReceptionSessionCancelledCallbacks && numTransmissionSessionCompletedCallbacks) {
                    break;
                }
                cv.timed_wait(cvLock, boost::posix_time::milliseconds(500));
            }
            TryWaitForNoActiveSendersAndReceivers();
            AssertNoActiveSendersAndReceivers();

            BOOST_REQUIRE_EQUAL(ltpUdpEngineSrcPtr->m_countAsyncSendCallbackCalls, DESIRED_RED_DATA_TO_SEND.size() + 7); //+7 for 6 RA and 1 CA
            BOOST_REQUIRE_EQUAL(ltpUdpEngineSrcPtr->m_countAsyncSendCallbackCalls, ltpUdpEngineSrcPtr->m_countAsyncSendCalls);


            BOOST_REQUIRE_EQUAL(ltpUdpEngineDestPtr->m_countAsyncSendCallbackCalls, 7); //7 for 1 RS, 5 resend RS, and 1 cancel segment

            BOOST_REQUIRE_EQUAL(ltpUdpEngineDestPtr->m_countAsyncSendCallbackCalls, ltpUdpEngineDestPtr->m_countAsyncSendCalls);
            BOOST_REQUIRE_EQUAL(numRedPartReceptionCallbacks, 1);
            BOOST_REQUIRE_EQUAL(numSessionStartSenderCallbacks, 1);
            BOOST_REQUIRE_EQUAL(numSessionStartReceiverCallbacks, 1);
            BOOST_REQUIRE_EQUAL(numReceptionSessionCancelledCallbacks, 1);
            BOOST_REQUIRE(lastReasonCode_receptionSessionCancelledCallback == CANCEL_SEGMENT_REASON_CODES::RLEXC);
            BOOST_REQUIRE_EQUAL(numTransmissionSessionCompletedCallbacks, 1);
            BOOST_REQUIRE_EQUAL(numInitialTransmissionCompletedCallbacks, 1);
            BOOST_REQUIRE_EQUAL(numTransmissionSessionCancelledCallbacks, 0);

            BOOST_REQUIRE_EQUAL(ltpUdpEngineDestPtr->m_numReportSegmentTimerExpiredCallbacks, 6);
            BOOST_REQUIRE_EQUAL(ltpUdpEngineDestPtr->m_numCheckpointTimerExpiredCallbacks, 0);
            BOOST_REQUIRE_EQUAL(ltpUdpEngineSrcPtr->m_numReportSegmentTimerExpiredCallbacks, 0);
            BOOST_REQUIRE_EQUAL(ltpUdpEngineSrcPtr->m_numCheckpointTimerExpiredCallbacks, 0);
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
            ltpUdpEngineSrcPtr->m_udpDropSimulatorFunction = boost::bind(&DropSimulation::DoSim, &sim, boost::placeholders::_1);
            boost::shared_ptr<LtpEngine::transmission_request_t> tReq = boost::make_shared<LtpEngine::transmission_request_t>();
            tReq->destinationClientServiceId = CLIENT_SERVICE_ID_DEST;
            tReq->destinationLtpEngineId = ENGINE_ID_DEST;
            tReq->clientServiceDataToSend = std::vector<uint8_t>(DESIRED_RED_DATA_TO_SEND.data(), DESIRED_RED_DATA_TO_SEND.data() + DESIRED_RED_DATA_TO_SEND.size()); //copy
            tReq->lengthOfRedPart = DESIRED_RED_DATA_TO_SEND.size();
            std::shared_ptr<MyTransmissionUserData> myUserData = std::make_shared<MyTransmissionUserData>(123);
            tReq->userDataPtr = myUserData; //keep a copy
            ltpUdpEngineSrcPtr->TransmissionRequest_ThreadSafe(std::move(tReq));
            for (unsigned int i = 0; i < 10; ++i) {
                if (numInitialTransmissionCompletedCallbacks) {
                    break;
                }
                cv.timed_wait(cvLock, boost::posix_time::milliseconds(250));
            }
            ltpUdpEngineDestPtr->CancellationRequest_ThreadSafe(lastSessionId_sessionStartSenderCallback); //cancel from receiver
            TryWaitForNoActiveSendersAndReceivers();
            AssertNoActiveSendersAndReceivers();

            BOOST_REQUIRE_EQUAL(ltpUdpEngineSrcPtr->m_countAsyncSendCallbackCalls, DESIRED_RED_DATA_TO_SEND.size() + 1); //+1 cancel ack
            BOOST_REQUIRE_EQUAL(ltpUdpEngineSrcPtr->m_countAsyncSendCallbackCalls, ltpUdpEngineSrcPtr->m_countAsyncSendCalls);


            BOOST_REQUIRE_EQUAL(ltpUdpEngineDestPtr->m_countAsyncSendCallbackCalls, 1); //1 cancel segment
            BOOST_REQUIRE_EQUAL(ltpUdpEngineDestPtr->m_countAsyncSendCallbackCalls, ltpUdpEngineDestPtr->m_countAsyncSendCalls);

            BOOST_REQUIRE_EQUAL(numRedPartReceptionCallbacks, 0);
            BOOST_REQUIRE_EQUAL(numSessionStartSenderCallbacks, 1);
            BOOST_REQUIRE_EQUAL(numSessionStartReceiverCallbacks, 1);
            BOOST_REQUIRE_EQUAL(numReceptionSessionCancelledCallbacks, 0);
            //BOOST_REQUIRE(lastReasonCode_receptionSessionCancelledCallback == CANCEL_SEGMENT_REASON_CODES::RLEXC);
            BOOST_REQUIRE_EQUAL(numTransmissionSessionCompletedCallbacks, 0);
            BOOST_REQUIRE_EQUAL(numInitialTransmissionCompletedCallbacks, 1);
            BOOST_REQUIRE_EQUAL(numTransmissionSessionCancelledCallbacks, 1);
            BOOST_REQUIRE(lastReasonCode_transmissionSessionCancelledCallback == CANCEL_SEGMENT_REASON_CODES::USER_CANCELLED);

            BOOST_REQUIRE_EQUAL(ltpUdpEngineDestPtr->m_numReportSegmentTimerExpiredCallbacks, 0);
            BOOST_REQUIRE_EQUAL(ltpUdpEngineDestPtr->m_numCheckpointTimerExpiredCallbacks, 0);
            BOOST_REQUIRE_EQUAL(ltpUdpEngineSrcPtr->m_numReportSegmentTimerExpiredCallbacks, 0);
            BOOST_REQUIRE_EQUAL(ltpUdpEngineSrcPtr->m_numCheckpointTimerExpiredCallbacks, 0);
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
            ltpUdpEngineSrcPtr->m_udpDropSimulatorFunction = boost::bind(&DropSimulation::DoSim, &sim, boost::placeholders::_1);
            boost::shared_ptr<LtpEngine::transmission_request_t> tReq = boost::make_shared<LtpEngine::transmission_request_t>();
            tReq->destinationClientServiceId = CLIENT_SERVICE_ID_DEST;
            tReq->destinationLtpEngineId = ENGINE_ID_DEST;
            tReq->clientServiceDataToSend = std::vector<uint8_t>(DESIRED_RED_DATA_TO_SEND.data(), DESIRED_RED_DATA_TO_SEND.data() + DESIRED_RED_DATA_TO_SEND.size()); //copy
            tReq->lengthOfRedPart = DESIRED_RED_DATA_TO_SEND.size();
            std::shared_ptr<MyTransmissionUserData> myUserData = std::make_shared<MyTransmissionUserData>(123);
            tReq->userDataPtr = myUserData; //keep a copy
            ltpUdpEngineSrcPtr->TransmissionRequest_ThreadSafe(std::move(tReq));
            for (unsigned int i = 0; i < 10; ++i) {
                if (numInitialTransmissionCompletedCallbacks) {
                    break;
                }
                cv.timed_wait(cvLock, boost::posix_time::milliseconds(250));
            }
            ltpUdpEngineSrcPtr->CancellationRequest_ThreadSafe(lastSessionId_sessionStartSenderCallback); //cancel from sender
            TryWaitForNoActiveSendersAndReceivers();
            AssertNoActiveSendersAndReceivers();

            BOOST_REQUIRE_EQUAL(ltpUdpEngineSrcPtr->m_countAsyncSendCallbackCalls, DESIRED_RED_DATA_TO_SEND.size() + 1); //+1 cancel req
            BOOST_REQUIRE_EQUAL(ltpUdpEngineSrcPtr->m_countAsyncSendCallbackCalls, ltpUdpEngineSrcPtr->m_countAsyncSendCalls);


            BOOST_REQUIRE_EQUAL(ltpUdpEngineDestPtr->m_countAsyncSendCallbackCalls, 1); //1 cancel ack
            BOOST_REQUIRE_EQUAL(ltpUdpEngineDestPtr->m_countAsyncSendCallbackCalls, ltpUdpEngineDestPtr->m_countAsyncSendCalls);

            BOOST_REQUIRE_EQUAL(numRedPartReceptionCallbacks, 0);
            BOOST_REQUIRE_EQUAL(numSessionStartSenderCallbacks, 1);
            BOOST_REQUIRE_EQUAL(numSessionStartReceiverCallbacks, 1);
            BOOST_REQUIRE_EQUAL(numReceptionSessionCancelledCallbacks, 1);
            BOOST_REQUIRE(lastReasonCode_receptionSessionCancelledCallback == CANCEL_SEGMENT_REASON_CODES::USER_CANCELLED);
            BOOST_REQUIRE_EQUAL(numTransmissionSessionCompletedCallbacks, 0);
            BOOST_REQUIRE_EQUAL(numInitialTransmissionCompletedCallbacks, 1);
            BOOST_REQUIRE_EQUAL(numTransmissionSessionCancelledCallbacks, 0);
            //BOOST_REQUIRE(lastReasonCode_transmissionSessionCancelledCallback == CANCEL_SEGMENT_REASON_CODES::USER_CANCELLED);

            BOOST_REQUIRE_EQUAL(ltpUdpEngineDestPtr->m_numReportSegmentTimerExpiredCallbacks, 0);
            BOOST_REQUIRE_EQUAL(ltpUdpEngineDestPtr->m_numCheckpointTimerExpiredCallbacks, 0);
            BOOST_REQUIRE_EQUAL(ltpUdpEngineSrcPtr->m_numReportSegmentTimerExpiredCallbacks, 0);
            BOOST_REQUIRE_EQUAL(ltpUdpEngineSrcPtr->m_numCheckpointTimerExpiredCallbacks, 0);
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
            ltpUdpEngineDestPtr->SetMtuReportSegment(110); // 110 bytes will result in 3 reception claims max
            AssertNoActiveSendersAndReceivers();
            DropSimulation sim;
            ltpUdpEngineSrcPtr->m_udpDropSimulatorFunction = boost::bind(&DropSimulation::DoSim, &sim, boost::placeholders::_1);
            boost::shared_ptr<LtpEngine::transmission_request_t> tReq = boost::make_shared<LtpEngine::transmission_request_t>();
            tReq->destinationClientServiceId = CLIENT_SERVICE_ID_DEST;
            tReq->destinationLtpEngineId = ENGINE_ID_DEST;
            tReq->clientServiceDataToSend = std::vector<uint8_t>(DESIRED_RED_DATA_TO_SEND.data(), DESIRED_RED_DATA_TO_SEND.data() + DESIRED_RED_DATA_TO_SEND.size()); //copy
            tReq->lengthOfRedPart = DESIRED_RED_DATA_TO_SEND.size();
            std::shared_ptr<MyTransmissionUserData> myUserData = std::make_shared<MyTransmissionUserData>(123);
            tReq->userDataPtr = myUserData; //keep a copy
            ltpUdpEngineSrcPtr->TransmissionRequest_ThreadSafe(std::move(tReq));
            for (unsigned int i = 0; i < 10; ++i) {
                if (numRedPartReceptionCallbacks && numTransmissionSessionCompletedCallbacks) {
                    break;
                }
                cv.timed_wait(cvLock, boost::posix_time::milliseconds(200));
            }
            TryWaitForNoActiveSendersAndReceivers();
            AssertNoActiveSendersAndReceivers();
            //std::cout << "numSrcToDestDataExchanged " << numSrcToDestDataExchanged << " numDestToSrcDataExchanged " << numDestToSrcDataExchanged << " DESIRED_RED_DATA_TO_SEND.size() " << DESIRED_RED_DATA_TO_SEND.size() << std::endl;
            BOOST_REQUIRE_EQUAL(ltpUdpEngineSrcPtr->m_countAsyncSendCallbackCalls, DESIRED_RED_DATA_TO_SEND.size() + 25); //+25 for 10 Report acks (see below) and 15 resends
            BOOST_REQUIRE_EQUAL(ltpUdpEngineSrcPtr->m_countAsyncSendCallbackCalls, ltpUdpEngineSrcPtr->m_countAsyncSendCalls);
            BOOST_REQUIRE_EQUAL(ltpUdpEngineDestPtr->m_countAsyncSendCallbackCalls, 10); //10 for 5 initial separately sentReport segments + 5 report segments of data complete as response to resends
            BOOST_REQUIRE_EQUAL(ltpUdpEngineDestPtr->m_countAsyncSendCallbackCalls, ltpUdpEngineDestPtr->m_countAsyncSendCalls);
            BOOST_REQUIRE_EQUAL(numRedPartReceptionCallbacks, 1);
            BOOST_REQUIRE_EQUAL(numSessionStartSenderCallbacks, 1);
            BOOST_REQUIRE_EQUAL(numSessionStartReceiverCallbacks, 1);
            BOOST_REQUIRE_EQUAL(numGreenPartReceptionCallbacks, 0);
            BOOST_REQUIRE_EQUAL(numReceptionSessionCancelledCallbacks, 0);
            BOOST_REQUIRE_EQUAL(numTransmissionSessionCompletedCallbacks, 1);
            BOOST_REQUIRE_EQUAL(numInitialTransmissionCompletedCallbacks, 1);
            BOOST_REQUIRE_EQUAL(numTransmissionSessionCancelledCallbacks, 0);

            ltpUdpEngineDestPtr->SetMtuReportSegment(UINT64_MAX); //restore to default unlimited reception claims
        }
        
        //test receiver stagnant session timeout
        void DoTestDropGreenEobSrcToDest() {
            struct DropOneSimulation {
                int count;
                DropOneSimulation() : count(0) {}
                bool DoSim(const uint8_t ltpHeaderByte) {
                    const LTP_SEGMENT_TYPE_FLAGS type = static_cast<LTP_SEGMENT_TYPE_FLAGS>(ltpHeaderByte);
                    if (type == LTP_SEGMENT_TYPE_FLAGS::GREENDATA_ENDOFBLOCK) {
                        //std::cout << "drop green eob\n";
                        return true;
                    }
                    return false;
                }
            };
            Reset();
            AssertNoActiveSendersAndReceivers();
            DropOneSimulation sim;
            ltpUdpEngineSrcPtr->m_udpDropSimulatorFunction = boost::bind(&DropOneSimulation::DoSim, &sim, boost::placeholders::_1);
            boost::shared_ptr<LtpEngine::transmission_request_t> tReq = boost::make_shared<LtpEngine::transmission_request_t>();
            tReq->destinationClientServiceId = CLIENT_SERVICE_ID_DEST;
            tReq->destinationLtpEngineId = ENGINE_ID_DEST;
            tReq->clientServiceDataToSend = std::vector<uint8_t>(DESIRED_FULLY_GREEN_DATA_TO_SEND.data(), DESIRED_FULLY_GREEN_DATA_TO_SEND.data() + DESIRED_FULLY_GREEN_DATA_TO_SEND.size()); //copy
            tReq->lengthOfRedPart = 0;
            std::shared_ptr<MyTransmissionUserData> myUserData = std::make_shared<MyTransmissionUserData>(123);
            tReq->userDataPtr = myUserData; //keep a copy
            ltpUdpEngineSrcPtr->TransmissionRequest_ThreadSafe(std::move(tReq));
            for (unsigned int i = 0; i < 60; ++i) {
                if (numTransmissionSessionCompletedCallbacks && numReceptionSessionCancelledCallbacks && (numGreenPartReceptionCallbacks >= (DESIRED_FULLY_GREEN_DATA_TO_SEND.size() - 1))) {
                    break;
                }
                //std::cout << "delay" << i << "\n";
                cv.timed_wait(cvLock, boost::posix_time::milliseconds(200));
            }
            TryWaitForNoActiveSendersAndReceivers();
            AssertNoActiveSendersAndReceivers();
            BOOST_REQUIRE_EQUAL(numRedPartReceptionCallbacks, 0);
            BOOST_REQUIRE_EQUAL(numSessionStartSenderCallbacks, 1);
            BOOST_REQUIRE_EQUAL(numSessionStartReceiverCallbacks, 1);
            BOOST_REQUIRE_EQUAL(numGreenPartReceptionCallbacks, DESIRED_FULLY_GREEN_DATA_TO_SEND.size() - 1);
            BOOST_REQUIRE_EQUAL(numReceptionSessionCancelledCallbacks, 1);
            BOOST_REQUIRE(lastReasonCode_receptionSessionCancelledCallback == CANCEL_SEGMENT_REASON_CODES::USER_CANCELLED);
            BOOST_REQUIRE_EQUAL(numTransmissionSessionCompletedCallbacks, 1);
            BOOST_REQUIRE_EQUAL(numInitialTransmissionCompletedCallbacks, 1);
            BOOST_REQUIRE_EQUAL(numTransmissionSessionCancelledCallbacks, 0);

            BOOST_REQUIRE_EQUAL(ltpUdpEngineSrcPtr->m_countAsyncSendCallbackCalls, DESIRED_FULLY_GREEN_DATA_TO_SEND.size() + 1); //+1 for 1 (last) dropped packet
            BOOST_REQUIRE_EQUAL(ltpUdpEngineSrcPtr->m_countAsyncSendCallbackCalls, ltpUdpEngineSrcPtr->m_countAsyncSendCalls);
            BOOST_REQUIRE_EQUAL(ltpUdpEngineDestPtr->m_countAsyncSendCallbackCalls, 1); //1 for housekeeping sending CANCEL_SEGMENT_REASON_CODES::USER_CANCELLED 
            BOOST_REQUIRE_EQUAL(ltpUdpEngineDestPtr->m_countAsyncSendCallbackCalls, ltpUdpEngineDestPtr->m_countAsyncSendCalls);
        }

    };

    LtpUdpEngineManager::SetMaxUdpRxPacketSizeBytesForAllLtp(UINT16_MAX); //MUST BE CALLED BEFORE Test Constructor
    Test t;
    t.DoTest();
    t.DoTestRedAndGreenData();
    t.DoTestFullyGreenData();
    std::cout << "-----START LONG TEST (STAGNANT GREEN LTP DROPS EOB)---------\n";
    t.DoTestDropGreenEobSrcToDest();
    std::cout << "-----END LONG TEST (STAGNANT GREEN LTP DROPS EOB)---------\n";
    t.DoTestOneDropDataSegmentSrcToDest();
    t.DoTestTwoDropDataSegmentSrcToDest();
    t.DoTestTwoDropDataSegmentSrcToDestRegularCheckpoints();
    t.DoTestDropOneCheckpointDataSegmentSrcToDest();
    t.DoTestDropEOBCheckpointDataSegmentSrcToDest();
    t.DoTestDropRaSrcToDest();
    std::cout << "-----START LONG TEST (RED LTP ALWAYS DROPS EOB)---------\n";
    t.DoTestDropEOBAlwaysCheckpointDataSegmentSrcToDest();
    std::cout << "-----END LONG TEST (RED LTP ALWAYS DROPS EOB)---------\n";
    t.DoTestDropRaAlwaysSrcToDest();
    t.DoTestReceiverCancelSession();
    t.DoTestSenderCancelSession();
    t.DoTestDropOddDataSegmentWithRsMtu();
}

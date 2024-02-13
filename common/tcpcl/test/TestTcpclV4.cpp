/**
 * @file TestTcpclV4.cpp
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
#include "TcpclV4.h"
#include <boost/bind/bind.hpp>
#include <boost/endian/conversion.hpp>


BOOST_AUTO_TEST_CASE(TcpclV4FullTestCase)
{
    struct Test {
        TcpclV4 m_tcpcl;
        bool m_useTls;
        uint16_t m_keepAliveInterval;
        uint64_t m_segmentMru;
        uint64_t m_transferMru;
        uint64_t m_transferId;
        const std::string m_remoteNodeEidUri;
        TcpclV4::tcpclv4_extensions_t m_sessionExtensions;
        TcpclV4::tcpclv4_extensions_t m_transferExtensions;
        bool m_ackIsStart;
        bool m_ackIsEnd;
        uint64_t m_ackTransferId;
        uint64_t m_ackBytesAcknowledged;
        uint64_t m_bundleRefusalTransferId;
        TCPCLV4_SESSION_TERMINATION_REASON_CODES m_sessionTerminationReasonCode;
        bool m_isAckOfAnEarlierSessionTerminationMessage;
        const std::string m_bundleDataToSendNoFragment;
        unsigned int m_numContactHeaderCallbackCount;
        unsigned int m_numSessionInitCallbackCount;
        uint64_t m_numSessionExtensionsProcessed;
        uint64_t m_numTransferExtensionsProcessed;
        unsigned int m_numDataSegmentCallbackCountNoFragment;
        unsigned int m_numDataSegmentCallbackCountWithFragments;
        unsigned int m_numAckCallbackCount;
        unsigned int m_numBundleRefusalCallbackCount;
        unsigned int m_numMessageRejectCallbackCount;
        unsigned int m_numKeepAliveCallbackCount;
        unsigned int m_numSessionTerminationMessageCallbackCount;
        uint64_t m_lastBundleLength;
        uint64_t m_expectedBundleLength;
        std::string m_fragmentedBundleRxConcat;
        Test() :
            m_useTls(false),
            m_keepAliveInterval(0x1234),
            m_segmentMru(1000000),
            m_transferMru(2000000),
            m_transferId(0),
            m_remoteNodeEidUri("test Eid String!"),
            m_ackIsStart(true),
            m_ackIsEnd(true),
            m_ackTransferId(12345678910),
            m_ackBytesAcknowledged(234567891011),
            m_bundleRefusalTransferId(111111111111),
            m_sessionTerminationReasonCode(TCPCLV4_SESSION_TERMINATION_REASON_CODES::RESOURCE_EXHAUSTION),
            m_isAckOfAnEarlierSessionTerminationMessage(true),
            m_bundleDataToSendNoFragment("this is a test bundle"),
            m_numContactHeaderCallbackCount(0),
            m_numSessionInitCallbackCount(0),
            m_numSessionExtensionsProcessed(0),
            m_numTransferExtensionsProcessed(0),
            m_numDataSegmentCallbackCountNoFragment(0),
            m_numDataSegmentCallbackCountWithFragments(0),
            m_numAckCallbackCount(0),
            m_numBundleRefusalCallbackCount(0),
            m_numMessageRejectCallbackCount(0),
            m_numKeepAliveCallbackCount(0),
            m_numSessionTerminationMessageCallbackCount(0),
            m_fragmentedBundleRxConcat("")
        {

        }

        void DoRxContactHeader() {
            m_tcpcl.SetContactHeaderReadCallback(boost::bind(&Test::ContactHeaderCallback, this, boost::placeholders::_1));

            std::vector<uint8_t> hdr;
            TcpclV4::GenerateContactHeader(hdr, m_useTls);
            m_tcpcl.HandleReceivedChars(hdr.data(), hdr.size());
        }

        void DoSessionInit()
        {
            m_tcpcl.SetSessionInitReadCallback(boost::bind(&Test::SessionInitReadCallback, this, boost::placeholders::_1,
                boost::placeholders::_2, boost::placeholders::_3, boost::placeholders::_4, boost::placeholders::_5));

            std::vector<uint8_t> msg;
            BOOST_REQUIRE(TcpclV4::GenerateSessionInitMessage(msg, m_keepAliveInterval, m_segmentMru, m_transferMru, m_remoteNodeEidUri, m_sessionExtensions));
            m_tcpcl.HandleReceivedChars(msg.data(), msg.size());
        }
        
        uint64_t DoAck(bool doCharByChar, bool doSweep = false) {
            m_tcpcl.SetAckSegmentReadCallback(boost::bind(&Test::AckCallback, this, boost::placeholders::_1));

            std::vector<uint8_t> ackSegment;
            BOOST_REQUIRE(TcpclV4::GenerateAckSegment(ackSegment, m_ackIsStart, m_ackIsEnd, m_ackTransferId, m_ackBytesAcknowledged));
            if (doSweep) {
                for (std::size_t off = 0; off < ackSegment.size(); ++off) {
                    m_tcpcl.HandleReceivedChars(ackSegment.data(), off);
                    m_tcpcl.HandleReceivedChars(ackSegment.data() + off, ackSegment.size() - off);
                    /*if (off) {
                        m_tcpcl.HandleReceivedChars(ackSegment.data(), off);
                    }
                    for (std::size_t i = off; i < ackSegment.size(); ++i) {
                        m_tcpcl.HandleReceivedChar(ackSegment[i]);
                    }*/
                    BOOST_REQUIRE(m_tcpcl.m_mainRxState == TCPCLV4_MAIN_RX_STATE::READ_MESSAGE_TYPE_BYTE);
                }
            }
            else if (doCharByChar) {
                //skip tcpcl state machine shortcut optimizations
                for (std::size_t i = 0; i < ackSegment.size(); ++i) {
                    m_tcpcl.HandleReceivedChar(ackSegment[i]);
                }
            }
            else {
                m_tcpcl.HandleReceivedChars(ackSegment.data(), ackSegment.size());
            }
            return ackSegment.size();
        }

        void DoBundleRefusal() {
            m_tcpcl.SetBundleRefusalCallback(boost::bind(&Test::BundleRefusalCallback, this, boost::placeholders::_1, boost::placeholders::_2));

            std::vector<uint8_t> bundleRefusalSegment;
            TcpclV4::GenerateBundleRefusal(bundleRefusalSegment, TCPCLV4_TRANSFER_REFUSE_REASON_CODES::REFUSAL_REASON_EXTENSION_FAILURE, m_bundleRefusalTransferId);
            m_tcpcl.HandleReceivedChars(bundleRefusalSegment.data(), bundleRefusalSegment.size());
        }

        void DoMessageReject() {
            m_tcpcl.SetMessageRejectCallback(boost::bind(&Test::MessageRejectCallback, this, boost::placeholders::_1, boost::placeholders::_2));

            std::vector<uint8_t> messageRejectSegment;
            TcpclV4::GenerateMessageRejection(messageRejectSegment, TCPCLV4_MESSAGE_REJECT_REASON_CODES::MESSAGE_TYPE_UNKNOWN, 253);
            m_tcpcl.HandleReceivedChars(messageRejectSegment.data(), messageRejectSegment.size());
        }

        

        void DoKeepAlive() {
            m_tcpcl.SetKeepAliveCallback(boost::bind(&Test::KeepAliveCallback, this));

            std::vector<uint8_t> keepAliveSegment;
            TcpclV4::GenerateKeepAliveMessage(keepAliveSegment);
            m_tcpcl.HandleReceivedChars(keepAliveSegment.data(), keepAliveSegment.size());
        }

        void DoSessionTermination() {
            m_tcpcl.SetSessionTerminationMessageCallback(boost::bind(&Test::SessionTerminationMessageCallback, this, boost::placeholders::_1, boost::placeholders::_2));

            std::vector<uint8_t> sessionTerminationMessage;
            TcpclV4::GenerateSessionTerminationMessage(sessionTerminationMessage, m_sessionTerminationReasonCode, m_isAckOfAnEarlierSessionTerminationMessage);

            m_tcpcl.HandleReceivedChars(sessionTerminationMessage.data(), sessionTerminationMessage.size());
        }

        

        uint64_t DoDataSegmentNoFragment(bool doCharByChar, bool doXferExtensions, bool doSweep = false) {
            std::vector<uint8_t> bundleSegment;
            m_tcpcl.SetDataSegmentContentsReadCallback(boost::bind(&Test::DataSegmentCallbackNoFragment, this, boost::placeholders::_1, boost::placeholders::_2, boost::placeholders::_3,
                boost::placeholders::_4, boost::placeholders::_5));
            if (doXferExtensions) {
                BOOST_REQUIRE(TcpclV4::GenerateNonFragmentedDataSegment(bundleSegment, m_transferId, (const uint8_t*)m_bundleDataToSendNoFragment.data(), m_bundleDataToSendNoFragment.size(), m_transferExtensions));
            }
            else {
                BOOST_REQUIRE(TcpclV4::GenerateNonFragmentedDataSegment(bundleSegment, m_transferId, (const uint8_t*)m_bundleDataToSendNoFragment.data(), m_bundleDataToSendNoFragment.size()));
            }
            if (doSweep) {
                for (std::size_t off = 0; off < bundleSegment.size(); ++off) {
                    m_tcpcl.HandleReceivedChars(bundleSegment.data(), off);
                    m_tcpcl.HandleReceivedChars(bundleSegment.data() + off, bundleSegment.size() - off);
                    /*if (off) {
                        m_tcpcl.HandleReceivedChars(bundleSegment.data(), off);
                    }
                    for (std::size_t i = off; i < bundleSegment.size(); ++i) {
                        m_tcpcl.HandleReceivedChar(bundleSegment[i]);
                    }*/
                    BOOST_REQUIRE(m_tcpcl.m_mainRxState == TCPCLV4_MAIN_RX_STATE::READ_MESSAGE_TYPE_BYTE);
                }
            }
            else if (doCharByChar) {
                //skip tcpcl state machine shortcut optimizations
                for (std::size_t i = 0; i < bundleSegment.size(); ++i) {
                    m_tcpcl.HandleReceivedChar(bundleSegment[i]);
                }
            }
            else {
                m_tcpcl.HandleReceivedChars(bundleSegment.data(), bundleSegment.size());
            }
            return bundleSegment.size();
        }


        

        void DoDataSegmentThreeFragments(bool doCharByChar, bool doLengthExtension) {
            m_tcpcl.SetDataSegmentContentsReadCallback(boost::bind(&Test::DataSegmentCallbackWithFragments, this, boost::placeholders::_1, boost::placeholders::_2, boost::placeholders::_3,
                boost::placeholders::_4, boost::placeholders::_5));
            std::vector<uint8_t> bundleSegment;
            static const TcpclV4::tcpclv4_extensions_t emptyExtensions;
            static const std::string f1 = "fragOne ";
            static const std::string f2 = "fragTwo ";
            static const std::string f3 = "fragThree";
            m_expectedBundleLength = f1.size() + f2.size() + f3.size();

            BOOST_REQUIRE(m_tcpcl.m_mainRxState == TCPCLV4_MAIN_RX_STATE::READ_MESSAGE_TYPE_BYTE);
            m_fragmentedBundleRxConcat.clear();
            BOOST_REQUIRE_EQUAL(m_fragmentedBundleRxConcat, std::string(""));
            BOOST_REQUIRE_EQUAL(m_numDataSegmentCallbackCountWithFragments, 0);
            
            if (doLengthExtension) {
                BOOST_REQUIRE(TcpclV4::GenerateFragmentedStartDataSegmentWithLengthExtension(bundleSegment, m_transferId, (const uint8_t*)f1.data(), f1.size(), m_expectedBundleLength));
            }
            else {
                BOOST_REQUIRE(TcpclV4::GenerateStartDataSegment(bundleSegment, false, m_transferId, (const uint8_t*)f1.data(), f1.size(), emptyExtensions));
            }
            
            if (doCharByChar) {
                //skip tcpcl state machine shortcut optimizations
                for (std::size_t i = 0; i < bundleSegment.size(); ++i) {
                    m_tcpcl.HandleReceivedChar(bundleSegment[i]);
                }
            }
            else {
                m_tcpcl.HandleReceivedChars(bundleSegment.data(), bundleSegment.size());
            }

            BOOST_REQUIRE(m_tcpcl.m_mainRxState == TCPCLV4_MAIN_RX_STATE::READ_MESSAGE_TYPE_BYTE);
            BOOST_REQUIRE_EQUAL(m_fragmentedBundleRxConcat, f1);
            BOOST_REQUIRE_EQUAL(m_numDataSegmentCallbackCountWithFragments, 1);
            
            BOOST_REQUIRE(TcpclV4::GenerateNonStartDataSegment(bundleSegment, false, m_transferId, (const uint8_t*)f2.data(), f2.size()));
            if (doCharByChar) {
                //skip tcpcl state machine shortcut optimizations
                for (std::size_t i = 0; i < bundleSegment.size(); ++i) {
                    m_tcpcl.HandleReceivedChar(bundleSegment[i]);
                }
            }
            else {
                m_tcpcl.HandleReceivedChars(bundleSegment.data(), bundleSegment.size());
            }
            BOOST_REQUIRE(m_tcpcl.m_mainRxState == TCPCLV4_MAIN_RX_STATE::READ_MESSAGE_TYPE_BYTE);
            BOOST_REQUIRE_EQUAL(m_fragmentedBundleRxConcat, f1 + f2);
            BOOST_REQUIRE_EQUAL(m_numDataSegmentCallbackCountWithFragments, 2);
            
            BOOST_REQUIRE(TcpclV4::GenerateNonStartDataSegment(bundleSegment, true, m_transferId, (const uint8_t*)f3.data(), f3.size()));
            if (doCharByChar) {
                //skip tcpcl state machine shortcut optimizations
                for (std::size_t i = 0; i < bundleSegment.size(); ++i) {
                    m_tcpcl.HandleReceivedChar(bundleSegment[i]);
                }
            }
            else {
                m_tcpcl.HandleReceivedChars(bundleSegment.data(), bundleSegment.size());
            }
            BOOST_REQUIRE(m_tcpcl.m_mainRxState == TCPCLV4_MAIN_RX_STATE::READ_MESSAGE_TYPE_BYTE);
            BOOST_REQUIRE_EQUAL(m_fragmentedBundleRxConcat, f1+f2+f3);
            BOOST_REQUIRE_EQUAL(m_numDataSegmentCallbackCountWithFragments, 3);
        }
        
        void ContactHeaderCallback(bool remoteHasEnabledTlsSecurity) {
            ++m_numContactHeaderCallbackCount;
            BOOST_REQUIRE(m_useTls == remoteHasEnabledTlsSecurity);
        }
        void SessionInitReadCallback(uint16_t keepAliveIntervalSeconds, uint64_t segmentMru, uint64_t transferMru,
            const std::string & remoteNodeEidUri, const TcpclV4::tcpclv4_extensions_t & sessionExtensions)
        {
            ++m_numSessionInitCallbackCount;
            m_numSessionExtensionsProcessed += sessionExtensions.extensionsVec.size();
            BOOST_REQUIRE_EQUAL(m_keepAliveInterval, keepAliveIntervalSeconds);
            BOOST_REQUIRE_EQUAL(m_segmentMru, segmentMru);
            BOOST_REQUIRE_EQUAL(m_transferMru, transferMru);
            BOOST_REQUIRE_EQUAL(m_remoteNodeEidUri, remoteNodeEidUri);
            BOOST_REQUIRE(sessionExtensions == m_sessionExtensions);
            BOOST_REQUIRE(!(sessionExtensions != m_sessionExtensions));
        }
        
        void DataSegmentCallbackNoFragment(padded_vector_uint8_t & dataSegmentDataVec, bool isStartFlag, bool isEndFlag,
            uint64_t transferId, const TcpclV4::tcpclv4_extensions_t & transferExtensions)
        {
            ++m_numDataSegmentCallbackCountNoFragment;
            m_numTransferExtensionsProcessed += transferExtensions.extensionsVec.size();
            BOOST_REQUIRE(isStartFlag);
            BOOST_REQUIRE(isEndFlag);
            BOOST_REQUIRE_EQUAL(transferId, m_transferId);
            const std::string rxBundleData(dataSegmentDataVec.data(), dataSegmentDataVec.data() + dataSegmentDataVec.size());
            BOOST_REQUIRE_EQUAL(m_bundleDataToSendNoFragment, rxBundleData);
        }

        void DataSegmentCallbackWithFragments(padded_vector_uint8_t & dataSegmentDataVec, bool isStartFlag, bool isEndFlag,
            uint64_t transferId, const TcpclV4::tcpclv4_extensions_t & transferExtensions)
        {
            BOOST_REQUIRE_EQUAL(transferId, m_transferId);
            if (m_numDataSegmentCallbackCountWithFragments == 0) {
                BOOST_REQUIRE(isStartFlag);
                BOOST_REQUIRE(!isEndFlag);
                BOOST_REQUIRE_LE(transferExtensions.extensionsVec.size(), 1);
                if (transferExtensions.extensionsVec.size()) {
                    BOOST_REQUIRE(transferExtensions.extensionsVec[0].IsCritical());
                    BOOST_REQUIRE_EQUAL(transferExtensions.extensionsVec[0].type, 0x0001); //length extension type
                    BOOST_REQUIRE_EQUAL(transferExtensions.extensionsVec[0].valueVec.size(), sizeof(m_lastBundleLength)); //length is 64 bit
                    memcpy(&m_lastBundleLength, transferExtensions.extensionsVec[0].valueVec.data(), sizeof(m_lastBundleLength));
                    boost::endian::big_to_native_inplace(m_lastBundleLength);
                    BOOST_REQUIRE_EQUAL(m_lastBundleLength, m_expectedBundleLength);
                }
            }
            else if (m_numDataSegmentCallbackCountWithFragments == 1) {
                BOOST_REQUIRE(!isStartFlag);
                BOOST_REQUIRE(!isEndFlag);
                BOOST_REQUIRE_EQUAL(transferExtensions.extensionsVec.size(), 0);
            }
            else if (m_numDataSegmentCallbackCountWithFragments == 2) {
                BOOST_REQUIRE(!isStartFlag);
                BOOST_REQUIRE(isEndFlag);
                BOOST_REQUIRE_EQUAL(transferExtensions.extensionsVec.size(), 0);
            }
            else {
                BOOST_REQUIRE(false);
            }
            ++m_numDataSegmentCallbackCountWithFragments;
            m_numTransferExtensionsProcessed += transferExtensions.extensionsVec.size();

            if (isStartFlag) {
                m_fragmentedBundleRxConcat.resize(0);
            }
            const std::string rxBundleData(dataSegmentDataVec.data(), dataSegmentDataVec.data() + dataSegmentDataVec.size());
            m_fragmentedBundleRxConcat.insert(m_fragmentedBundleRxConcat.end(), rxBundleData.begin(), rxBundleData.end()); //concatenate
        }

        void AckCallback(const TcpclV4::tcpclv4_ack_t & ack) {
            ++m_numAckCallbackCount;
            BOOST_REQUIRE_EQUAL(m_ackIsStart, ack.isStartSegment);
            BOOST_REQUIRE_EQUAL(m_ackIsEnd, ack.isEndSegment);
            BOOST_REQUIRE_EQUAL(m_ackTransferId, ack.transferId);
            BOOST_REQUIRE_EQUAL(m_ackBytesAcknowledged, ack.totalBytesAcknowledged);
        }
        
        void BundleRefusalCallback(TCPCLV4_TRANSFER_REFUSE_REASON_CODES refusalCode, uint64_t transferId) {
            ++m_numBundleRefusalCallbackCount;
            BOOST_REQUIRE(refusalCode == TCPCLV4_TRANSFER_REFUSE_REASON_CODES::REFUSAL_REASON_EXTENSION_FAILURE);
            BOOST_REQUIRE_EQUAL(m_bundleRefusalTransferId, transferId);
        }

        void MessageRejectCallback(TCPCLV4_MESSAGE_REJECT_REASON_CODES refusalCode, uint8_t rejectedMessageHeader) {
            ++m_numMessageRejectCallbackCount;
            BOOST_REQUIRE(refusalCode == TCPCLV4_MESSAGE_REJECT_REASON_CODES::MESSAGE_TYPE_UNKNOWN);
            BOOST_REQUIRE_EQUAL(rejectedMessageHeader, 253);
        }

        
        void KeepAliveCallback() {
            ++m_numKeepAliveCallbackCount;
        }

        void SessionTerminationMessageCallback(TCPCLV4_SESSION_TERMINATION_REASON_CODES terminationReasonCode, bool isAckOfAnEarlierSessionTerminationMessage) {
            ++m_numSessionTerminationMessageCallbackCount;
            BOOST_REQUIRE(m_sessionTerminationReasonCode == terminationReasonCode);
            BOOST_REQUIRE_EQUAL(m_isAckOfAnEarlierSessionTerminationMessage, isAckOfAnEarlierSessionTerminationMessage);
        }

    };
    
    Test t;

    //contact header with tls
    BOOST_REQUIRE(t.m_tcpcl.m_mainRxState == TCPCLV4_MAIN_RX_STATE::READ_CONTACT_HEADER);
    BOOST_REQUIRE_EQUAL(t.m_numContactHeaderCallbackCount, 0);
    t.m_useTls = true;
    t.DoRxContactHeader(); //use tls
    BOOST_REQUIRE_EQUAL(t.m_numContactHeaderCallbackCount, 1);
    BOOST_REQUIRE(t.m_tcpcl.m_mainRxState == TCPCLV4_MAIN_RX_STATE::READ_MESSAGE_TYPE_BYTE);
    t.m_tcpcl.InitRx();
    //contact header no tls
    BOOST_REQUIRE(t.m_tcpcl.m_mainRxState == TCPCLV4_MAIN_RX_STATE::READ_CONTACT_HEADER);
    t.m_useTls = false;
    t.DoRxContactHeader(); //don't use tls
    BOOST_REQUIRE_EQUAL(t.m_numContactHeaderCallbackCount, 2);
    BOOST_REQUIRE(t.m_tcpcl.m_mainRxState == TCPCLV4_MAIN_RX_STATE::READ_MESSAGE_TYPE_BYTE);

    //session init
    BOOST_REQUIRE_EQUAL(t.m_numSessionInitCallbackCount, 0);
    BOOST_REQUIRE_EQUAL(t.m_numSessionExtensionsProcessed, 0);
    t.DoSessionInit();
    BOOST_REQUIRE_EQUAL(t.m_numSessionInitCallbackCount, 1);
    BOOST_REQUIRE_EQUAL(t.m_numSessionExtensionsProcessed, 0);
    BOOST_REQUIRE(t.m_tcpcl.m_mainRxState == TCPCLV4_MAIN_RX_STATE::READ_MESSAGE_TYPE_BYTE);
    //add 1 extension
    t.m_sessionExtensions.extensionsVec.emplace_back(true, 10, std::vector<uint8_t>({ 0x5, 0x6 }));
    BOOST_REQUIRE(t.m_sessionExtensions.extensionsVec.back().IsCritical());
    t.DoSessionInit();
    BOOST_REQUIRE_EQUAL(t.m_numSessionInitCallbackCount, 2);
    BOOST_REQUIRE_EQUAL(t.m_numSessionExtensionsProcessed, 1);
    BOOST_REQUIRE(t.m_tcpcl.m_mainRxState == TCPCLV4_MAIN_RX_STATE::READ_MESSAGE_TYPE_BYTE);
    //add another extension that is zero-length
    t.m_sessionExtensions.extensionsVec.emplace_back(true, 15, std::vector<uint8_t>());
    BOOST_REQUIRE(t.m_sessionExtensions.extensionsVec.back().IsCritical());
    t.DoSessionInit();
    BOOST_REQUIRE_EQUAL(t.m_numSessionInitCallbackCount, 3);
    BOOST_REQUIRE_EQUAL(t.m_numSessionExtensionsProcessed, 3);
    BOOST_REQUIRE(t.m_tcpcl.m_mainRxState == TCPCLV4_MAIN_RX_STATE::READ_MESSAGE_TYPE_BYTE);
    //add a third extension
    t.m_sessionExtensions.extensionsVec.emplace_back(false, 20, std::vector<uint8_t>());
    BOOST_REQUIRE(!t.m_sessionExtensions.extensionsVec.back().IsCritical());
    t.DoSessionInit();
    BOOST_REQUIRE_EQUAL(t.m_numSessionInitCallbackCount, 4);
    BOOST_REQUIRE_EQUAL(t.m_numSessionExtensionsProcessed, 6);
    BOOST_REQUIRE(t.m_tcpcl.m_mainRxState == TCPCLV4_MAIN_RX_STATE::READ_MESSAGE_TYPE_BYTE);
    
    uint64_t bundlesize;
    //non fragmented data segment (no transfer extensions)
    t.m_numDataSegmentCallbackCountNoFragment = 0;
    t.m_numTransferExtensionsProcessed = 0;
    BOOST_REQUIRE_EQUAL(t.m_numDataSegmentCallbackCountNoFragment, 0);
    BOOST_REQUIRE_EQUAL(t.m_numTransferExtensionsProcessed, 0);
    t.m_transferId = 100000000000;
    t.DoDataSegmentNoFragment(false, false); //not char by char
    BOOST_REQUIRE_EQUAL(t.m_numDataSegmentCallbackCountNoFragment, 1);
    BOOST_REQUIRE_EQUAL(t.m_numTransferExtensionsProcessed, 0);
    BOOST_REQUIRE(t.m_tcpcl.m_mainRxState == TCPCLV4_MAIN_RX_STATE::READ_MESSAGE_TYPE_BYTE);
    t.m_numDataSegmentCallbackCountNoFragment = 0;
    t.m_numTransferExtensionsProcessed = 0;
    t.m_transferId = 100000000001;
    t.DoDataSegmentNoFragment(true, false); //char by char
    BOOST_REQUIRE_EQUAL(t.m_numDataSegmentCallbackCountNoFragment, 1);
    BOOST_REQUIRE_EQUAL(t.m_numTransferExtensionsProcessed, 0);
    BOOST_REQUIRE(t.m_tcpcl.m_mainRxState == TCPCLV4_MAIN_RX_STATE::READ_MESSAGE_TYPE_BYTE);
    t.m_numDataSegmentCallbackCountNoFragment = 0;
    t.m_numTransferExtensionsProcessed = 0;
    bundlesize = t.DoDataSegmentNoFragment(true, false, true); //sweep
    BOOST_REQUIRE_EQUAL(t.m_numDataSegmentCallbackCountNoFragment, bundlesize);
    BOOST_REQUIRE_EQUAL(t.m_numTransferExtensionsProcessed, 0);
    BOOST_REQUIRE(t.m_tcpcl.m_mainRxState == TCPCLV4_MAIN_RX_STATE::READ_MESSAGE_TYPE_BYTE);

    //non fragmented data segment (with transfer extensions)
    //start with no transfer extensions
    t.m_numDataSegmentCallbackCountNoFragment = 0;
    t.m_numTransferExtensionsProcessed = 0;
    t.m_transferId = 100000000002;
    t.DoDataSegmentNoFragment(false, true); //not char by char
    BOOST_REQUIRE_EQUAL(t.m_numDataSegmentCallbackCountNoFragment, 1);
    BOOST_REQUIRE_EQUAL(t.m_numTransferExtensionsProcessed, 0);
    BOOST_REQUIRE(t.m_tcpcl.m_mainRxState == TCPCLV4_MAIN_RX_STATE::READ_MESSAGE_TYPE_BYTE);
    t.m_numDataSegmentCallbackCountNoFragment = 0;
    t.m_numTransferExtensionsProcessed = 0;
    t.m_transferId = 100000000003;
    t.DoDataSegmentNoFragment(true, true); //char by char
    BOOST_REQUIRE_EQUAL(t.m_numDataSegmentCallbackCountNoFragment, 1);
    BOOST_REQUIRE_EQUAL(t.m_numTransferExtensionsProcessed, 0);
    BOOST_REQUIRE(t.m_tcpcl.m_mainRxState == TCPCLV4_MAIN_RX_STATE::READ_MESSAGE_TYPE_BYTE);
    t.m_numDataSegmentCallbackCountNoFragment = 0;
    t.m_numTransferExtensionsProcessed = 0;
    bundlesize = t.DoDataSegmentNoFragment(true, true, true); //sweep
    BOOST_REQUIRE_EQUAL(t.m_numDataSegmentCallbackCountNoFragment, bundlesize);
    BOOST_REQUIRE_EQUAL(t.m_numTransferExtensionsProcessed, 0);
    BOOST_REQUIRE(t.m_tcpcl.m_mainRxState == TCPCLV4_MAIN_RX_STATE::READ_MESSAGE_TYPE_BYTE);
    //add 1 extension
    t.m_transferExtensions.extensionsVec.emplace_back(true, 10, std::vector<uint8_t>({ 0x5, 0x6, 0x07 }));
    BOOST_REQUIRE(t.m_transferExtensions.extensionsVec.back().IsCritical());
    t.m_numDataSegmentCallbackCountNoFragment = 0;
    t.m_numTransferExtensionsProcessed = 0;
    t.m_transferId = 100000000004;
    t.DoDataSegmentNoFragment(false, true); //not char by char
    BOOST_REQUIRE_EQUAL(t.m_numDataSegmentCallbackCountNoFragment, 1);
    BOOST_REQUIRE_EQUAL(t.m_numTransferExtensionsProcessed, 1);
    BOOST_REQUIRE(t.m_tcpcl.m_mainRxState == TCPCLV4_MAIN_RX_STATE::READ_MESSAGE_TYPE_BYTE);
    t.m_numDataSegmentCallbackCountNoFragment = 0;
    t.m_numTransferExtensionsProcessed = 0;
    t.m_transferId = 100000000005;
    t.DoDataSegmentNoFragment(true, true); //char by char
    BOOST_REQUIRE_EQUAL(t.m_numDataSegmentCallbackCountNoFragment, 1);
    BOOST_REQUIRE_EQUAL(t.m_numTransferExtensionsProcessed, 1);
    BOOST_REQUIRE(t.m_tcpcl.m_mainRxState == TCPCLV4_MAIN_RX_STATE::READ_MESSAGE_TYPE_BYTE);
    t.m_numDataSegmentCallbackCountNoFragment = 0;
    t.m_numTransferExtensionsProcessed = 0;
    bundlesize = t.DoDataSegmentNoFragment(true, true, true); //sweep
    BOOST_REQUIRE_EQUAL(t.m_numDataSegmentCallbackCountNoFragment, bundlesize);
    BOOST_REQUIRE_EQUAL(t.m_numTransferExtensionsProcessed, bundlesize);
    BOOST_REQUIRE(t.m_tcpcl.m_mainRxState == TCPCLV4_MAIN_RX_STATE::READ_MESSAGE_TYPE_BYTE);
    //add another extension that is zero-length
    t.m_transferExtensions.extensionsVec.emplace_back(true, 15, std::vector<uint8_t>());
    BOOST_REQUIRE(t.m_transferExtensions.extensionsVec.back().IsCritical());
    t.m_numDataSegmentCallbackCountNoFragment = 0;
    t.m_numTransferExtensionsProcessed = 0;
    t.m_transferId = 100000000006;
    t.DoDataSegmentNoFragment(false, true); //not char by char
    BOOST_REQUIRE_EQUAL(t.m_numDataSegmentCallbackCountNoFragment, 1);
    BOOST_REQUIRE_EQUAL(t.m_numTransferExtensionsProcessed, 2);
    BOOST_REQUIRE(t.m_tcpcl.m_mainRxState == TCPCLV4_MAIN_RX_STATE::READ_MESSAGE_TYPE_BYTE);
    t.m_numDataSegmentCallbackCountNoFragment = 0;
    t.m_numTransferExtensionsProcessed = 0;
    t.m_transferId = 100000000007;
    t.DoDataSegmentNoFragment(true, true); //char by char
    BOOST_REQUIRE_EQUAL(t.m_numDataSegmentCallbackCountNoFragment, 1);
    BOOST_REQUIRE_EQUAL(t.m_numTransferExtensionsProcessed, 2);
    BOOST_REQUIRE(t.m_tcpcl.m_mainRxState == TCPCLV4_MAIN_RX_STATE::READ_MESSAGE_TYPE_BYTE);
    t.m_numDataSegmentCallbackCountNoFragment = 0;
    t.m_numTransferExtensionsProcessed = 0;
    bundlesize = t.DoDataSegmentNoFragment(true, true, true); //sweep
    BOOST_REQUIRE_EQUAL(t.m_numDataSegmentCallbackCountNoFragment, bundlesize);
    BOOST_REQUIRE_EQUAL(t.m_numTransferExtensionsProcessed, 2 * bundlesize);
    BOOST_REQUIRE(t.m_tcpcl.m_mainRxState == TCPCLV4_MAIN_RX_STATE::READ_MESSAGE_TYPE_BYTE);
    //add a third extension
    t.m_transferExtensions.extensionsVec.emplace_back(false, 20, std::vector<uint8_t>());
    BOOST_REQUIRE(!t.m_transferExtensions.extensionsVec.back().IsCritical());
    t.m_numDataSegmentCallbackCountNoFragment = 0;
    t.m_numTransferExtensionsProcessed = 0;
    t.m_transferId = 100000000008;
    t.DoDataSegmentNoFragment(false, true); //not char by char
    BOOST_REQUIRE_EQUAL(t.m_numDataSegmentCallbackCountNoFragment, 1);
    BOOST_REQUIRE_EQUAL(t.m_numTransferExtensionsProcessed, 3);
    BOOST_REQUIRE(t.m_tcpcl.m_mainRxState == TCPCLV4_MAIN_RX_STATE::READ_MESSAGE_TYPE_BYTE);
    t.m_numDataSegmentCallbackCountNoFragment = 0;
    t.m_numTransferExtensionsProcessed = 0;
    t.m_transferId = 100000000009;
    t.DoDataSegmentNoFragment(true, true); //char by char
    BOOST_REQUIRE_EQUAL(t.m_numDataSegmentCallbackCountNoFragment, 1);
    BOOST_REQUIRE_EQUAL(t.m_numTransferExtensionsProcessed, 3);
    BOOST_REQUIRE(t.m_tcpcl.m_mainRxState == TCPCLV4_MAIN_RX_STATE::READ_MESSAGE_TYPE_BYTE);
    t.m_numDataSegmentCallbackCountNoFragment = 0;
    t.m_numTransferExtensionsProcessed = 0;
    bundlesize = t.DoDataSegmentNoFragment(true, true, true); //sweep
    BOOST_REQUIRE_EQUAL(t.m_numDataSegmentCallbackCountNoFragment, bundlesize);
    BOOST_REQUIRE_EQUAL(t.m_numTransferExtensionsProcessed, 3 * bundlesize);
    BOOST_REQUIRE(t.m_tcpcl.m_mainRxState == TCPCLV4_MAIN_RX_STATE::READ_MESSAGE_TYPE_BYTE);


    //fragmented data segment
    //not char by char
    for (unsigned int whichTest = 0; whichTest <= 1; ++whichTest) {
        const bool doLengthExtension = (whichTest == 1);
        t.m_numDataSegmentCallbackCountWithFragments = 0;
        t.m_numTransferExtensionsProcessed = 0;
        t.m_transferId = 200000000001;
        BOOST_REQUIRE_EQUAL(t.m_numDataSegmentCallbackCountWithFragments, 0);
        t.DoDataSegmentThreeFragments(false, doLengthExtension); //not char by char
        BOOST_REQUIRE_EQUAL(t.m_numDataSegmentCallbackCountWithFragments, 3);
        BOOST_REQUIRE_EQUAL(t.m_numTransferExtensionsProcessed, whichTest);
        BOOST_REQUIRE(t.m_tcpcl.m_mainRxState == TCPCLV4_MAIN_RX_STATE::READ_MESSAGE_TYPE_BYTE);
        //char by char
        t.m_numDataSegmentCallbackCountWithFragments = 0;
        t.m_numTransferExtensionsProcessed = 0;
        t.m_transferId = 200000000002;
        BOOST_REQUIRE_EQUAL(t.m_numDataSegmentCallbackCountWithFragments, 0);
        t.DoDataSegmentThreeFragments(true, doLengthExtension); //char by char
        BOOST_REQUIRE_EQUAL(t.m_numDataSegmentCallbackCountWithFragments, 3);
        BOOST_REQUIRE_EQUAL(t.m_numTransferExtensionsProcessed, whichTest);
        BOOST_REQUIRE(t.m_tcpcl.m_mainRxState == TCPCLV4_MAIN_RX_STATE::READ_MESSAGE_TYPE_BYTE);
    }
    
    

    uint64_t ackPayloadSize;
    BOOST_REQUIRE_EQUAL(t.m_numAckCallbackCount, 0);
    t.m_ackIsStart = true;
    t.m_ackIsEnd = false;
    t.DoAck(false); //not char by char
    BOOST_REQUIRE_EQUAL(t.m_numAckCallbackCount, 1);
    BOOST_REQUIRE(t.m_tcpcl.m_mainRxState == TCPCLV4_MAIN_RX_STATE::READ_MESSAGE_TYPE_BYTE);
    //repeat ack with start and end swapped
    t.m_ackIsStart = false;
    t.m_ackIsEnd = true;
    t.m_ackTransferId = 5555555555555;
    t.m_ackBytesAcknowledged = 888888888888;
    t.DoAck(true); //char by char
    BOOST_REQUIRE_EQUAL(t.m_numAckCallbackCount, 2);
    BOOST_REQUIRE(t.m_tcpcl.m_mainRxState == TCPCLV4_MAIN_RX_STATE::READ_MESSAGE_TYPE_BYTE);
    //sweep test
    t.m_numAckCallbackCount = 0;
    ackPayloadSize = t.DoAck(true, true); //sweep
    BOOST_REQUIRE_EQUAL(t.m_numAckCallbackCount, ackPayloadSize);
    BOOST_REQUIRE(t.m_tcpcl.m_mainRxState == TCPCLV4_MAIN_RX_STATE::READ_MESSAGE_TYPE_BYTE);

    
    BOOST_REQUIRE_EQUAL(t.m_numBundleRefusalCallbackCount, 0);
    t.DoBundleRefusal();
    BOOST_REQUIRE_EQUAL(t.m_numBundleRefusalCallbackCount, 1);
    BOOST_REQUIRE(t.m_tcpcl.m_mainRxState == TCPCLV4_MAIN_RX_STATE::READ_MESSAGE_TYPE_BYTE);

    BOOST_REQUIRE_EQUAL(t.m_numMessageRejectCallbackCount, 0);
    t.DoMessageReject();
    BOOST_REQUIRE_EQUAL(t.m_numMessageRejectCallbackCount, 1);
    BOOST_REQUIRE(t.m_tcpcl.m_mainRxState == TCPCLV4_MAIN_RX_STATE::READ_MESSAGE_TYPE_BYTE);

    

    BOOST_REQUIRE_EQUAL(t.m_numKeepAliveCallbackCount, 0);
    t.DoKeepAlive();
    BOOST_REQUIRE_EQUAL(t.m_numKeepAliveCallbackCount, 1);
    BOOST_REQUIRE(t.m_tcpcl.m_mainRxState == TCPCLV4_MAIN_RX_STATE::READ_MESSAGE_TYPE_BYTE);

    BOOST_REQUIRE_EQUAL(t.m_numSessionTerminationMessageCallbackCount, 0);
    t.m_sessionTerminationReasonCode = TCPCLV4_SESSION_TERMINATION_REASON_CODES::IDLE_TIMEOUT;
    t.m_isAckOfAnEarlierSessionTerminationMessage = true;
    t.DoSessionTermination();
    BOOST_REQUIRE_EQUAL(t.m_numSessionTerminationMessageCallbackCount, 1);
    BOOST_REQUIRE(t.m_tcpcl.m_mainRxState == TCPCLV4_MAIN_RX_STATE::READ_MESSAGE_TYPE_BYTE); //not contact header in v4 (needs to be able to receive non-bundles)
    //repeat with different values
    t.m_sessionTerminationReasonCode = TCPCLV4_SESSION_TERMINATION_REASON_CODES::BUSY;
    t.m_isAckOfAnEarlierSessionTerminationMessage = false;
    t.DoSessionTermination();
    BOOST_REQUIRE_EQUAL(t.m_numSessionTerminationMessageCallbackCount, 2);
    BOOST_REQUIRE(t.m_tcpcl.m_mainRxState == TCPCLV4_MAIN_RX_STATE::READ_MESSAGE_TYPE_BYTE);

}



BOOST_AUTO_TEST_CASE(TcpclV4MagicHeaderStatesTestCase)
{

    TcpclV4 m_tcpcl;
    BOOST_REQUIRE(m_tcpcl.m_mainRxState == TCPCLV4_MAIN_RX_STATE::READ_CONTACT_HEADER);
    BOOST_REQUIRE(m_tcpcl.m_contactHeaderRxState == TCPCLV4_CONTACT_HEADER_RX_STATE::READ_SYNC_1);
    m_tcpcl.HandleReceivedChar('c'); //not d.. remain in 1
    BOOST_REQUIRE(m_tcpcl.m_mainRxState == TCPCLV4_MAIN_RX_STATE::READ_CONTACT_HEADER);
    BOOST_REQUIRE(m_tcpcl.m_contactHeaderRxState == TCPCLV4_CONTACT_HEADER_RX_STATE::READ_SYNC_1);
    m_tcpcl.HandleReceivedChar('d'); //first d.. advance
    BOOST_REQUIRE(m_tcpcl.m_mainRxState == TCPCLV4_MAIN_RX_STATE::READ_CONTACT_HEADER);
    BOOST_REQUIRE(m_tcpcl.m_contactHeaderRxState == TCPCLV4_CONTACT_HEADER_RX_STATE::READ_SYNC_2);
    m_tcpcl.HandleReceivedChar('d'); //second d.. ddtn!.. remain
    BOOST_REQUIRE(m_tcpcl.m_mainRxState == TCPCLV4_MAIN_RX_STATE::READ_CONTACT_HEADER);
    BOOST_REQUIRE(m_tcpcl.m_contactHeaderRxState == TCPCLV4_CONTACT_HEADER_RX_STATE::READ_SYNC_2);
    m_tcpcl.HandleReceivedChar('t'); //advance
    BOOST_REQUIRE(m_tcpcl.m_mainRxState == TCPCLV4_MAIN_RX_STATE::READ_CONTACT_HEADER);
    BOOST_REQUIRE(m_tcpcl.m_contactHeaderRxState == TCPCLV4_CONTACT_HEADER_RX_STATE::READ_SYNC_3);
    m_tcpcl.HandleReceivedChar('d'); //wrong but go to state 2 
    BOOST_REQUIRE(m_tcpcl.m_mainRxState == TCPCLV4_MAIN_RX_STATE::READ_CONTACT_HEADER);
    BOOST_REQUIRE(m_tcpcl.m_contactHeaderRxState == TCPCLV4_CONTACT_HEADER_RX_STATE::READ_SYNC_2);
    m_tcpcl.HandleReceivedChar('t'); //advance
    BOOST_REQUIRE(m_tcpcl.m_mainRxState == TCPCLV4_MAIN_RX_STATE::READ_CONTACT_HEADER);
    BOOST_REQUIRE(m_tcpcl.m_contactHeaderRxState == TCPCLV4_CONTACT_HEADER_RX_STATE::READ_SYNC_3);
    m_tcpcl.HandleReceivedChar('v'); //wrong , back to 1
    BOOST_REQUIRE(m_tcpcl.m_mainRxState == TCPCLV4_MAIN_RX_STATE::READ_CONTACT_HEADER);
    BOOST_REQUIRE(m_tcpcl.m_contactHeaderRxState == TCPCLV4_CONTACT_HEADER_RX_STATE::READ_SYNC_1);

    m_tcpcl.HandleReceivedChar('d'); //advance to 2
    BOOST_REQUIRE(m_tcpcl.m_mainRxState == TCPCLV4_MAIN_RX_STATE::READ_CONTACT_HEADER);
    BOOST_REQUIRE(m_tcpcl.m_contactHeaderRxState == TCPCLV4_CONTACT_HEADER_RX_STATE::READ_SYNC_2);
    m_tcpcl.HandleReceivedChar('t'); //advance
    BOOST_REQUIRE(m_tcpcl.m_mainRxState == TCPCLV4_MAIN_RX_STATE::READ_CONTACT_HEADER);
    BOOST_REQUIRE(m_tcpcl.m_contactHeaderRxState == TCPCLV4_CONTACT_HEADER_RX_STATE::READ_SYNC_3);
    m_tcpcl.HandleReceivedChar('n'); //advance
    BOOST_REQUIRE(m_tcpcl.m_mainRxState == TCPCLV4_MAIN_RX_STATE::READ_CONTACT_HEADER);
    BOOST_REQUIRE(m_tcpcl.m_contactHeaderRxState == TCPCLV4_CONTACT_HEADER_RX_STATE::READ_SYNC_4);
    m_tcpcl.HandleReceivedChar('d'); //wrong not !but go to state 2 
    BOOST_REQUIRE(m_tcpcl.m_mainRxState == TCPCLV4_MAIN_RX_STATE::READ_CONTACT_HEADER);
    BOOST_REQUIRE(m_tcpcl.m_contactHeaderRxState == TCPCLV4_CONTACT_HEADER_RX_STATE::READ_SYNC_2);
    m_tcpcl.HandleReceivedChar('t'); //advance
    BOOST_REQUIRE(m_tcpcl.m_mainRxState == TCPCLV4_MAIN_RX_STATE::READ_CONTACT_HEADER);
    BOOST_REQUIRE(m_tcpcl.m_contactHeaderRxState == TCPCLV4_CONTACT_HEADER_RX_STATE::READ_SYNC_3);
    m_tcpcl.HandleReceivedChar('n'); //advance
    BOOST_REQUIRE(m_tcpcl.m_mainRxState == TCPCLV4_MAIN_RX_STATE::READ_CONTACT_HEADER);
    BOOST_REQUIRE(m_tcpcl.m_contactHeaderRxState == TCPCLV4_CONTACT_HEADER_RX_STATE::READ_SYNC_4);
    m_tcpcl.HandleReceivedChar('v'); //wrong not !, back to 1
    BOOST_REQUIRE(m_tcpcl.m_mainRxState == TCPCLV4_MAIN_RX_STATE::READ_CONTACT_HEADER);
    BOOST_REQUIRE(m_tcpcl.m_contactHeaderRxState == TCPCLV4_CONTACT_HEADER_RX_STATE::READ_SYNC_1);

    m_tcpcl.HandleReceivedChar('d'); //advance to 2
    BOOST_REQUIRE(m_tcpcl.m_mainRxState == TCPCLV4_MAIN_RX_STATE::READ_CONTACT_HEADER);
    BOOST_REQUIRE(m_tcpcl.m_contactHeaderRxState == TCPCLV4_CONTACT_HEADER_RX_STATE::READ_SYNC_2);
    m_tcpcl.HandleReceivedChar('t'); //advance
    BOOST_REQUIRE(m_tcpcl.m_mainRxState == TCPCLV4_MAIN_RX_STATE::READ_CONTACT_HEADER);
    BOOST_REQUIRE(m_tcpcl.m_contactHeaderRxState == TCPCLV4_CONTACT_HEADER_RX_STATE::READ_SYNC_3);
    m_tcpcl.HandleReceivedChar('n'); //advance
    BOOST_REQUIRE(m_tcpcl.m_mainRxState == TCPCLV4_MAIN_RX_STATE::READ_CONTACT_HEADER);
    BOOST_REQUIRE(m_tcpcl.m_contactHeaderRxState == TCPCLV4_CONTACT_HEADER_RX_STATE::READ_SYNC_4);
    m_tcpcl.HandleReceivedChar('!'); //advance
    BOOST_REQUIRE(m_tcpcl.m_mainRxState == TCPCLV4_MAIN_RX_STATE::READ_CONTACT_HEADER);
    BOOST_REQUIRE(m_tcpcl.m_contactHeaderRxState == TCPCLV4_CONTACT_HEADER_RX_STATE::READ_VERSION);
    m_tcpcl.HandleReceivedChar('d'); //wrong version.. back to 2
    BOOST_REQUIRE(m_tcpcl.m_mainRxState == TCPCLV4_MAIN_RX_STATE::READ_CONTACT_HEADER);
    BOOST_REQUIRE(m_tcpcl.m_contactHeaderRxState == TCPCLV4_CONTACT_HEADER_RX_STATE::READ_SYNC_2);
    m_tcpcl.HandleReceivedChar('v'); //wrong , back to 1
    BOOST_REQUIRE(m_tcpcl.m_mainRxState == TCPCLV4_MAIN_RX_STATE::READ_CONTACT_HEADER);
    BOOST_REQUIRE(m_tcpcl.m_contactHeaderRxState == TCPCLV4_CONTACT_HEADER_RX_STATE::READ_SYNC_1);

    m_tcpcl.HandleReceivedChar('d'); //advance to 2
    BOOST_REQUIRE(m_tcpcl.m_mainRxState == TCPCLV4_MAIN_RX_STATE::READ_CONTACT_HEADER);
    BOOST_REQUIRE(m_tcpcl.m_contactHeaderRxState == TCPCLV4_CONTACT_HEADER_RX_STATE::READ_SYNC_2);
    m_tcpcl.HandleReceivedChar('t'); //advance
    BOOST_REQUIRE(m_tcpcl.m_mainRxState == TCPCLV4_MAIN_RX_STATE::READ_CONTACT_HEADER);
    BOOST_REQUIRE(m_tcpcl.m_contactHeaderRxState == TCPCLV4_CONTACT_HEADER_RX_STATE::READ_SYNC_3);
    m_tcpcl.HandleReceivedChar('n'); //advance
    BOOST_REQUIRE(m_tcpcl.m_mainRxState == TCPCLV4_MAIN_RX_STATE::READ_CONTACT_HEADER);
    BOOST_REQUIRE(m_tcpcl.m_contactHeaderRxState == TCPCLV4_CONTACT_HEADER_RX_STATE::READ_SYNC_4);
    m_tcpcl.HandleReceivedChar('!'); //advance
    BOOST_REQUIRE(m_tcpcl.m_mainRxState == TCPCLV4_MAIN_RX_STATE::READ_CONTACT_HEADER);
    BOOST_REQUIRE(m_tcpcl.m_contactHeaderRxState == TCPCLV4_CONTACT_HEADER_RX_STATE::READ_VERSION);
    m_tcpcl.HandleReceivedChar(2); //wrong version.. back to 1
    BOOST_REQUIRE(m_tcpcl.m_mainRxState == TCPCLV4_MAIN_RX_STATE::READ_CONTACT_HEADER);
    BOOST_REQUIRE(m_tcpcl.m_contactHeaderRxState == TCPCLV4_CONTACT_HEADER_RX_STATE::READ_SYNC_1);

    m_tcpcl.HandleReceivedChar('d'); //advance to 2
    BOOST_REQUIRE(m_tcpcl.m_mainRxState == TCPCLV4_MAIN_RX_STATE::READ_CONTACT_HEADER);
    BOOST_REQUIRE(m_tcpcl.m_contactHeaderRxState == TCPCLV4_CONTACT_HEADER_RX_STATE::READ_SYNC_2);
    m_tcpcl.HandleReceivedChar('t'); //advance
    BOOST_REQUIRE(m_tcpcl.m_mainRxState == TCPCLV4_MAIN_RX_STATE::READ_CONTACT_HEADER);
    BOOST_REQUIRE(m_tcpcl.m_contactHeaderRxState == TCPCLV4_CONTACT_HEADER_RX_STATE::READ_SYNC_3);
    m_tcpcl.HandleReceivedChar('n'); //advance
    BOOST_REQUIRE(m_tcpcl.m_mainRxState == TCPCLV4_MAIN_RX_STATE::READ_CONTACT_HEADER);
    BOOST_REQUIRE(m_tcpcl.m_contactHeaderRxState == TCPCLV4_CONTACT_HEADER_RX_STATE::READ_SYNC_4);
    m_tcpcl.HandleReceivedChar('!'); //advance
    BOOST_REQUIRE(m_tcpcl.m_mainRxState == TCPCLV4_MAIN_RX_STATE::READ_CONTACT_HEADER);
    BOOST_REQUIRE(m_tcpcl.m_contactHeaderRxState == TCPCLV4_CONTACT_HEADER_RX_STATE::READ_VERSION);
    m_tcpcl.HandleReceivedChar(4); //right version.. advance
    BOOST_REQUIRE(m_tcpcl.m_mainRxState == TCPCLV4_MAIN_RX_STATE::READ_CONTACT_HEADER);
    BOOST_REQUIRE(m_tcpcl.m_contactHeaderRxState == TCPCLV4_CONTACT_HEADER_RX_STATE::READ_FLAGS);

    m_tcpcl.InitRx();
    BOOST_REQUIRE(m_tcpcl.m_mainRxState == TCPCLV4_MAIN_RX_STATE::READ_CONTACT_HEADER);
    BOOST_REQUIRE(m_tcpcl.m_contactHeaderRxState == TCPCLV4_CONTACT_HEADER_RX_STATE::READ_SYNC_1);

    {
        const std::string bytesIn = "rrrrrrrrrrrrrdtyyyyyydtn!";
        m_tcpcl.HandleReceivedChars((const uint8_t *)bytesIn.c_str(), bytesIn.size());
        BOOST_REQUIRE(m_tcpcl.m_mainRxState == TCPCLV4_MAIN_RX_STATE::READ_CONTACT_HEADER);
        BOOST_REQUIRE(m_tcpcl.m_contactHeaderRxState == TCPCLV4_CONTACT_HEADER_RX_STATE::READ_VERSION);
    }
}

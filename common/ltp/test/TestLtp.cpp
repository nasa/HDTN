#include <boost/test/unit_test.hpp>
#include "Ltp.h"
#include <boost/bind.hpp>

BOOST_AUTO_TEST_CASE(LtpDataSegmentMetadataTestCase)
{
    Ltp::data_segment_metadata_t dsm1(1, 2, 3, NULL, NULL);
    Ltp::data_segment_metadata_t dsm2(1, 2, 3, NULL, NULL);
    BOOST_REQUIRE(dsm1 == dsm2);
    BOOST_REQUIRE(!(dsm1 != dsm2));
    {
        std::vector<uint8_t> serialization;
        uint64_t maxBytesRequired = dsm1.GetMaximumDataRequiredForSerialization();
        BOOST_REQUIRE_EQUAL(maxBytesRequired, 3 * 10);
        serialization.resize(maxBytesRequired);
        uint64_t bytesSerialized = dsm1.Serialize(serialization.data());
        BOOST_REQUIRE_EQUAL(bytesSerialized, 3);
    }

    uint64_t checkpointSerialNumber = 55;
    uint64_t reportSerialNumber = 66;
    dsm1.checkpointSerialNumber = &checkpointSerialNumber;
    dsm1.reportSerialNumber = &reportSerialNumber;
    {
        std::vector<uint8_t> serialization;
        uint64_t maxBytesRequired = dsm1.GetMaximumDataRequiredForSerialization();
        BOOST_REQUIRE_EQUAL(maxBytesRequired, 5 * 10);
        serialization.resize(maxBytesRequired);
        uint64_t bytesSerialized = dsm1.Serialize(serialization.data());
        BOOST_REQUIRE_EQUAL(bytesSerialized, 5);
    }
    BOOST_REQUIRE(dsm1 != dsm2);
    BOOST_REQUIRE(dsm2 != dsm1);
    dsm2.checkpointSerialNumber = &checkpointSerialNumber;
    dsm2.reportSerialNumber = &reportSerialNumber;
    BOOST_REQUIRE(dsm1 == dsm2);
    BOOST_REQUIRE(dsm2 == dsm1);
    dsm1.clientServiceId = 99;
    BOOST_REQUIRE(dsm1 != dsm2);
    BOOST_REQUIRE(dsm2 != dsm1);
}

BOOST_AUTO_TEST_CASE(LtpExtensionsTestCase)
{
    Ltp::ltp_extensions_t extensions;

    //ADD FIRST EXTENSION
    {
        Ltp::ltp_extension_t eCopy;
        Ltp::ltp_extension_t e;
        e.tag = 0x44;
        e.valueVec.assign(500, 'b');
        eCopy = e;
        BOOST_REQUIRE(eCopy == e);
        BOOST_REQUIRE(!(eCopy != e));
        extensions.extensionsVec.push_back(std::move(e));
    }
    std::vector<uint8_t> serialization;
    uint64_t maxBytesRequired = extensions.GetMaximumDataRequiredForSerialization();
    BOOST_REQUIRE_EQUAL(maxBytesRequired, 1 + 10 + 500);
    serialization.resize(maxBytesRequired);
    uint64_t bytesSerialized = extensions.Serialize(serialization.data());
    BOOST_REQUIRE_EQUAL(bytesSerialized, 1 + 2 + 500); //500 requires 2 byte sdnv
    BOOST_REQUIRE_EQUAL(serialization[0], 0x44);
    BOOST_REQUIRE_EQUAL(serialization[3], 'b');

    //ADD SECOND EXTENSION
    {
        Ltp::ltp_extension_t e2;
        e2.tag = 0x45;
        e2.valueVec.assign(100, 'c');
        extensions.extensionsVec.push_back(std::move(e2));
    }
    maxBytesRequired = extensions.GetMaximumDataRequiredForSerialization();
    BOOST_REQUIRE_EQUAL(maxBytesRequired, (1 + 10 + 500) + (1 + 10 + 100));
    serialization.resize(maxBytesRequired);
    bytesSerialized = extensions.Serialize(serialization.data());
    BOOST_REQUIRE_EQUAL(bytesSerialized, (1 + 2 + 500) + (1 + 1 + 100)); //500 requires 2 byte sdnv and 100 requires 1 byte sdnv
    BOOST_REQUIRE_EQUAL(serialization[0], 0x44);
    BOOST_REQUIRE_EQUAL(serialization[3], 'b');
    BOOST_REQUIRE_EQUAL(serialization[(1 + 2 + 500)], 0x45);
    BOOST_REQUIRE_EQUAL(serialization[(1 + 2 + 500) + 2], 'c');

    //COPY AND MOVE EXTENSIONS
    Ltp::ltp_extensions_t extensionsCopy = extensions;
    BOOST_REQUIRE(extensionsCopy == extensions);
    BOOST_REQUIRE(!(extensionsCopy != extensions));
    Ltp::ltp_extensions_t extensionsCopyMoved = std::move(extensionsCopy);
    BOOST_REQUIRE(extensionsCopy != extensions); //extensionsCopy has been moved
    BOOST_REQUIRE(extensionsCopyMoved == extensions);
    Ltp::ltp_extensions_t extensionsCopyMovedByCtor(std::move(extensionsCopyMoved));
    BOOST_REQUIRE(extensionsCopyMoved != extensions); //extensionsCopyMoved has been moved to extensionsCopyMovedByCtor
    BOOST_REQUIRE(extensionsCopyMovedByCtor == extensions);
    Ltp::ltp_extensions_t extensionsCopyByCtor(extensions);
    BOOST_REQUIRE(extensionsCopyByCtor == extensions);
}

BOOST_AUTO_TEST_CASE(LtpReportSegmentTestCase)
{
    Ltp::report_segment_t reportSegment;
    reportSegment.reportSerialNumber = 50;
    reportSegment.checkpointSerialNumber = 55;
    reportSegment.upperBound = 60;
    reportSegment.lowerBound = 65;
    reportSegment.receptionClaimCount = 130; //2 byte sdnv

    {
        std::vector<uint8_t> serialization;
        uint64_t maxBytesRequired = reportSegment.GetMaximumDataRequiredForSerialization();
        BOOST_REQUIRE_EQUAL(maxBytesRequired, 5 * 10);
        serialization.resize(maxBytesRequired);
        uint64_t bytesSerialized = reportSegment.Serialize(serialization.data());
        BOOST_REQUIRE_EQUAL(bytesSerialized, 6); //130 requires 2 bytes, reception claims are empty
    }

    //ADD FIRST RECEPTION CLAIM
    {
        Ltp::reception_claim_t rCopy;
        Ltp::reception_claim_t r;
        r.offset = 40;
        r.length = 505;
        rCopy = r;
        BOOST_REQUIRE(rCopy == r);
        BOOST_REQUIRE(!(rCopy != r));
        reportSegment.receptionClaims.push_back(r);
    }
    {
        std::vector<uint8_t> serialization;
        uint64_t maxBytesRequired = reportSegment.GetMaximumDataRequiredForSerialization();
        BOOST_REQUIRE_EQUAL(maxBytesRequired, 5 * 10 + 1 * 2 * 10);
        serialization.resize(maxBytesRequired);
        uint64_t bytesSerialized = reportSegment.Serialize(serialization.data());
        BOOST_REQUIRE_EQUAL(bytesSerialized, 6 + 3); //130 and 505 require 2 bytes
    }

    //ADD SECOND RECEPTION CLAIM
    {
        Ltp::reception_claim_t r;
        r.offset = 600;
        r.length = 700;
        reportSegment.receptionClaims.push_back(r);
    }
    {
        std::vector<uint8_t> serialization;
        uint64_t maxBytesRequired = reportSegment.GetMaximumDataRequiredForSerialization();
        BOOST_REQUIRE_EQUAL(maxBytesRequired, 5 * 10 + 2 * 2 * 10);
        serialization.resize(maxBytesRequired);
        uint64_t bytesSerialized = reportSegment.Serialize(serialization.data());
        BOOST_REQUIRE_EQUAL(bytesSerialized, 6 + 3 + 4); //130 and 505 and 600 and 700 require 2 bytes
    }

    //COPY AND MOVE EXTENSIONS
    Ltp::report_segment_t reportSegmentCopy = reportSegment;
    BOOST_REQUIRE(reportSegmentCopy == reportSegment);
    BOOST_REQUIRE(!(reportSegmentCopy != reportSegment));
    Ltp::report_segment_t reportSegmentCopyMoved = std::move(reportSegmentCopy);
    BOOST_REQUIRE(reportSegmentCopy != reportSegment); //reportSegmentCopy has been moved
    BOOST_REQUIRE(reportSegmentCopyMoved == reportSegment);
    Ltp::report_segment_t reportSegmentCopyMovedByCtor(std::move(reportSegmentCopyMoved));
    BOOST_REQUIRE(reportSegmentCopyMoved != reportSegment); //reportSegmentCopyMoved has been moved to reportSegmentCopyMovedByCtor
    BOOST_REQUIRE(reportSegmentCopyMovedByCtor == reportSegment);
    Ltp::report_segment_t reportSegmentCopyByCtor(reportSegment);
    BOOST_REQUIRE(reportSegmentCopyByCtor == reportSegment);
}

BOOST_AUTO_TEST_CASE(LtpFullTestCase)
{
    struct TestLtp {
        Ltp m_ltp;
        LTP_DATA_SEGMENT_TYPE_FLAGS m_desired_dataSegmentTypeFlags;
        uint64_t m_desired_sessionOriginatorEngineId;
        uint64_t m_desired_sessionNumber;
        std::vector<uint8_t> m_desired_clientServiceDataVec;
        Ltp::data_segment_metadata_t m_desired_dataSegmentMetadata;
        Ltp::ltp_extensions_t m_desired_headerExtensions;
        Ltp::ltp_extensions_t m_desired_trailerExtensions;

        Ltp::report_segment_t m_desired_reportSegment;

        uint64_t m_numDataSegmentCallbackCount;
        uint64_t m_numReportSegmentCallbackCount;
        TestLtp()
        {

        }

        void ReceiveReportSegment() {
            m_numReportSegmentCallbackCount = 0;
            std::vector<uint8_t> ltpReportSegmentPacket;
            Ltp::GenerateReportSegmentLtpPacket(ltpReportSegmentPacket,
                m_desired_sessionOriginatorEngineId, m_desired_sessionNumber, m_desired_reportSegment,
                (m_desired_headerExtensions.extensionsVec.empty()) ? NULL : &m_desired_headerExtensions,
                (m_desired_trailerExtensions.extensionsVec.empty()) ? NULL : &m_desired_trailerExtensions);
            
            for (unsigned int i = 1; i <= 5; ++i) {
                std::string errorMessage;
                BOOST_REQUIRE(m_ltp.HandleReceivedChars(ltpReportSegmentPacket.data(), ltpReportSegmentPacket.size(), errorMessage));
                BOOST_REQUIRE_EQUAL(m_numReportSegmentCallbackCount, i);
                BOOST_REQUIRE_EQUAL(errorMessage.size(), 0);
                BOOST_REQUIRE(m_ltp.IsAtBeginningState());
            }
        }

        void DoReportSegment() {


            m_ltp.SetReportSegmentContentsReadCallback(boost::bind(&TestLtp::ReportSegmentCallback, this,
                boost::placeholders::_1, boost::placeholders::_2, boost::placeholders::_3,
                boost::placeholders::_4, boost::placeholders::_5));



            m_desired_reportSegment.reportSerialNumber = 12345;
            m_desired_reportSegment.checkpointSerialNumber = 12346;
            m_desired_reportSegment.upperBound = 12347;
            m_desired_reportSegment.lowerBound = 12348;
            
            {
                Ltp::reception_claim_t rc;
                rc.offset = 12349;
                rc.length = 12350;
                m_desired_reportSegment.receptionClaims.push_back(rc);
                m_desired_reportSegment.receptionClaimCount = m_desired_reportSegment.receptionClaims.size(); //minimum size is 1
            }
            

            //NO TRAILER EXTENSIONS, NO HEADER EXTENSIONS, 1 CLAIM
            {
                m_desired_headerExtensions.extensionsVec.clear();
                m_desired_trailerExtensions.extensionsVec.clear();

                ReceiveReportSegment();
            }

            //NO TRAILER EXTENSIONS, NO HEADER EXTENSIONS, 2 CLAIMS
            {
                {
                    BOOST_REQUIRE_EQUAL(m_desired_reportSegment.receptionClaims.size(), 1);
                    Ltp::reception_claim_t rc;
                    rc.offset = 123490;
                    rc.length = 123500;
                    m_desired_reportSegment.receptionClaims.push_back(rc);
                    m_desired_reportSegment.receptionClaimCount = m_desired_reportSegment.receptionClaims.size(); //minimum size is 1
                }

                ReceiveReportSegment();
            }

            //back to 1 claim
            BOOST_REQUIRE_EQUAL(m_desired_reportSegment.receptionClaims.size(), 2);
            m_desired_reportSegment.receptionClaims.pop_back();
            m_desired_reportSegment.receptionClaimCount = m_desired_reportSegment.receptionClaims.size(); //minimum size is 1  TODO THIS IS MESSY
            BOOST_REQUIRE_EQUAL(m_desired_reportSegment.receptionClaims.size(), 1);

           
            //1 TRAILER EXTENSION WITH DATA, NO HEADER EXTENSIONS, 1 CLAIM
            {
                m_desired_headerExtensions.extensionsVec.clear();
                m_desired_trailerExtensions.extensionsVec.clear();
                Ltp::ltp_extension_t e;
                e.tag = 0x55;
                e.valueVec.assign(500, 'd');
                m_desired_trailerExtensions.extensionsVec.push_back(std::move(e));

                ReceiveReportSegment();
            }

            
            //1 TRAILER EXTENSION WITH NO DATA, NO HEADER EXTENSIONS, 1 CLAIM
            {
                m_desired_headerExtensions.extensionsVec.clear();
                m_desired_trailerExtensions.extensionsVec.clear();
                Ltp::ltp_extension_t e;
                e.tag = 0x56;
                m_desired_trailerExtensions.extensionsVec.push_back(std::move(e));

                ReceiveReportSegment();
            }

            //2 TRAILER EXTENSIONS WITH DATA, NO HEADER EXTENSIONS, 1 CLAIM
            {
                m_desired_headerExtensions.extensionsVec.clear();
                m_desired_trailerExtensions.extensionsVec.clear();
                {
                    Ltp::ltp_extension_t e;
                    e.tag = 0x60;
                    e.valueVec.assign(500, 'd');
                    m_desired_trailerExtensions.extensionsVec.push_back(std::move(e));
                }
                {
                    Ltp::ltp_extension_t e;
                    e.tag = 0x61;
                    e.valueVec.assign(50, 'f');
                    m_desired_trailerExtensions.extensionsVec.push_back(std::move(e));
                }

                ReceiveReportSegment();
            }

            //1 HEADER EXTENSION WITH DATA, NO TRAILER EXTENSIONS, 1 CLAIM
            {
                m_desired_headerExtensions.extensionsVec.clear();
                m_desired_trailerExtensions.extensionsVec.clear();
                Ltp::ltp_extension_t e;
                e.tag = 0x55;
                e.valueVec.assign(501, 'g');
                m_desired_headerExtensions.extensionsVec.push_back(std::move(e));

                ReceiveReportSegment();
            }

            //1 HEADER EXTENSION WITH NO DATA, NO TRAILER EXTENSIONS, 1 CLAIM
            {
                m_desired_headerExtensions.extensionsVec.clear();
                m_desired_trailerExtensions.extensionsVec.clear();
                Ltp::ltp_extension_t e;
                e.tag = 0x56;
                m_desired_headerExtensions.extensionsVec.push_back(std::move(e));

                ReceiveReportSegment();
            }

            //2 HEADER EXTENSIONS WITH DATA, NO TRAILER EXTENSIONS, 1 CLAIM
            {
                m_desired_headerExtensions.extensionsVec.clear();
                m_desired_trailerExtensions.extensionsVec.clear();
                {
                    Ltp::ltp_extension_t e;
                    e.tag = 0x60;
                    e.valueVec.assign(502, 'h');
                    m_desired_headerExtensions.extensionsVec.push_back(std::move(e));
                }
                {
                    Ltp::ltp_extension_t e;
                    e.tag = 0x61;
                    e.valueVec.assign(51, 'i');
                    m_desired_headerExtensions.extensionsVec.push_back(std::move(e));
                }

                ReceiveReportSegment();
            }

            //2 HEADER EXTENSIONS WITH DATA, 2 TRAILER EXTENSIONS WITH DATA, 1 CLAIM
            {
                m_desired_headerExtensions.extensionsVec.clear();
                m_desired_trailerExtensions.extensionsVec.clear();
                {
                    Ltp::ltp_extension_t e;
                    e.tag = 0x70;
                    e.valueVec.assign(502, 'A');
                    m_desired_headerExtensions.extensionsVec.push_back(std::move(e));
                }
                {
                    Ltp::ltp_extension_t e;
                    e.tag = 0x71;
                    e.valueVec.assign(51, 'B');
                    m_desired_headerExtensions.extensionsVec.push_back(std::move(e));
                }
                {
                    Ltp::ltp_extension_t e;
                    e.tag = 0x72;
                    e.valueVec.assign(502, 'C');
                    m_desired_trailerExtensions.extensionsVec.push_back(std::move(e));
                }
                {
                    Ltp::ltp_extension_t e;
                    e.tag = 0x73;
                    e.valueVec.assign(51, 'D');
                    m_desired_trailerExtensions.extensionsVec.push_back(std::move(e));
                }

                ReceiveReportSegment();
            }

            //2 HEADER EXTENSIONS WITH NO DATA, 2 TRAILER EXTENSIONS WITH NO DATA, 1 CLAIM
            {
                m_desired_headerExtensions.extensionsVec.clear();
                m_desired_trailerExtensions.extensionsVec.clear();
                {
                    Ltp::ltp_extension_t e;
                    e.tag = 0x80;
                    m_desired_headerExtensions.extensionsVec.push_back(std::move(e));
                }
                {
                    Ltp::ltp_extension_t e;
                    e.tag = 0x81;
                    m_desired_headerExtensions.extensionsVec.push_back(std::move(e));
                }
                {
                    Ltp::ltp_extension_t e;
                    e.tag = 0x82;
                    m_desired_trailerExtensions.extensionsVec.push_back(std::move(e));
                }
                {
                    Ltp::ltp_extension_t e;
                    e.tag = 0x83;
                    m_desired_trailerExtensions.extensionsVec.push_back(std::move(e));
                }

                ReceiveReportSegment();
            }
            
            //2 HEADER EXTENSIONS WITH NO DATA, 2 TRAILER EXTENSIONS WITH NO DATA, ADD ANOTHER CLAIM
            {
                {
                    Ltp::reception_claim_t rc;
                    rc.offset = 123490;
                    rc.length = 123500;
                    m_desired_reportSegment.receptionClaims.push_back(rc);
                    m_desired_reportSegment.receptionClaimCount = m_desired_reportSegment.receptionClaims.size(); //minimum size is 1
                }

                ReceiveReportSegment();
            }
        }

        void ReportSegmentCallback(uint64_t sessionOriginatorEngineId, uint64_t sessionNumber, const Ltp::report_segment_t & reportSegment,
            Ltp::ltp_extensions_t & headerExtensions, Ltp::ltp_extensions_t & trailerExtensions)
        {
            ++m_numReportSegmentCallbackCount;
            BOOST_REQUIRE_EQUAL(sessionOriginatorEngineId, m_desired_sessionOriginatorEngineId);
            BOOST_REQUIRE_EQUAL(sessionNumber, m_desired_sessionNumber);
            BOOST_REQUIRE(reportSegment == m_desired_reportSegment);
            BOOST_REQUIRE(headerExtensions == m_desired_headerExtensions);
            BOOST_REQUIRE(trailerExtensions == m_desired_trailerExtensions);
        }

        void ReceiveDataSegment() {
            m_numDataSegmentCallbackCount = 0;
            std::vector<uint8_t> ltpHeaderPlusDataSegmentMetadata;
            Ltp::GenerateLtpHeaderPlusDataSegmentMetadata(ltpHeaderPlusDataSegmentMetadata, m_desired_dataSegmentTypeFlags,
                m_desired_sessionOriginatorEngineId, m_desired_sessionNumber, m_desired_dataSegmentMetadata,
                (m_desired_headerExtensions.extensionsVec.empty()) ? NULL : &m_desired_headerExtensions,
                static_cast<uint8_t>(m_desired_trailerExtensions.extensionsVec.size()));
            for (unsigned int i = 1; i <= 5; ++i) {
                std::string errorMessage;
                BOOST_REQUIRE(m_ltp.HandleReceivedChars(ltpHeaderPlusDataSegmentMetadata.data(), ltpHeaderPlusDataSegmentMetadata.size(), errorMessage));
                BOOST_REQUIRE(m_ltp.HandleReceivedChars(m_desired_clientServiceDataVec.data(), m_desired_clientServiceDataVec.size(), errorMessage));
                if (!m_desired_trailerExtensions.extensionsVec.empty()) {
                    std::vector<uint8_t> trailerSerialization(m_desired_trailerExtensions.GetMaximumDataRequiredForSerialization());
                    const uint64_t bytesSerialized = m_desired_trailerExtensions.Serialize(trailerSerialization.data());
                    BOOST_REQUIRE(m_ltp.HandleReceivedChars(trailerSerialization.data(), bytesSerialized, errorMessage));
                }
                BOOST_REQUIRE_EQUAL(m_numDataSegmentCallbackCount, i);
                BOOST_REQUIRE_EQUAL(errorMessage.size(), 0);
                BOOST_REQUIRE(m_ltp.IsAtBeginningState());
            }
        }

        void DoDataSegment() {


            m_ltp.SetDataSegmentContentsReadCallback(boost::bind(&TestLtp::DataSegmentCallback, this,
                boost::placeholders::_1, boost::placeholders::_2, boost::placeholders::_3,
                boost::placeholders::_4, boost::placeholders::_5, boost::placeholders::_6, boost::placeholders::_7));



            m_desired_dataSegmentTypeFlags = LTP_DATA_SEGMENT_TYPE_FLAGS::GREENDATA;
            m_desired_sessionOriginatorEngineId = 5555;
            m_desired_sessionNumber = 6666;
            m_desired_clientServiceDataVec = { 'a', 'b', 'c', 'd' };
            m_desired_dataSegmentMetadata.clientServiceId = 7777;
            m_desired_dataSegmentMetadata.offset = 8888;
            m_desired_dataSegmentMetadata.length = m_desired_clientServiceDataVec.size();
            m_desired_dataSegmentMetadata.checkpointSerialNumber = NULL;
            m_desired_dataSegmentMetadata.reportSerialNumber = NULL;


            //NO TRAILER EXTENSIONS, NO HEADER EXTENSIONS, NO CHECKPOINT
            {
                m_desired_headerExtensions.extensionsVec.clear();
                m_desired_trailerExtensions.extensionsVec.clear();

                ReceiveDataSegment();
            }

            //1 TRAILER EXTENSION WITH DATA, NO HEADER EXTENSIONS, NO CHECKPOINT
            {
                m_desired_headerExtensions.extensionsVec.clear();
                m_desired_trailerExtensions.extensionsVec.clear();
                Ltp::ltp_extension_t e;
                e.tag = 0x55;
                e.valueVec.assign(500, 'd');
                m_desired_trailerExtensions.extensionsVec.push_back(std::move(e));

                ReceiveDataSegment();
            }

            //1 TRAILER EXTENSION WITH NO DATA, NO HEADER EXTENSIONS, NO CHECKPOINT
            {
                m_desired_headerExtensions.extensionsVec.clear();
                m_desired_trailerExtensions.extensionsVec.clear();
                Ltp::ltp_extension_t e;
                e.tag = 0x56;
                m_desired_trailerExtensions.extensionsVec.push_back(std::move(e));

                ReceiveDataSegment();
            }

            //2 TRAILER EXTENSIONS WITH DATA, NO HEADER EXTENSIONS, NO CHECKPOINT
            {
                m_desired_headerExtensions.extensionsVec.clear();
                m_desired_trailerExtensions.extensionsVec.clear();
                {
                    Ltp::ltp_extension_t e;
                    e.tag = 0x60;
                    e.valueVec.assign(500, 'd');
                    m_desired_trailerExtensions.extensionsVec.push_back(std::move(e));
                }
                {
                    Ltp::ltp_extension_t e;
                    e.tag = 0x61;
                    e.valueVec.assign(50, 'f');
                    m_desired_trailerExtensions.extensionsVec.push_back(std::move(e));
                }

                ReceiveDataSegment();
            }

            //1 HEADER EXTENSION WITH DATA, NO TRAILER EXTENSIONS, NO CHECKPOINT
            {
                m_desired_headerExtensions.extensionsVec.clear();
                m_desired_trailerExtensions.extensionsVec.clear();
                Ltp::ltp_extension_t e;
                e.tag = 0x55;
                e.valueVec.assign(501, 'g');
                m_desired_headerExtensions.extensionsVec.push_back(std::move(e));

                ReceiveDataSegment();
            }

            //1 HEADER EXTENSION WITH NO DATA, NO TRAILER EXTENSIONS, NO CHECKPOINT
            {
                m_desired_headerExtensions.extensionsVec.clear();
                m_desired_trailerExtensions.extensionsVec.clear();
                Ltp::ltp_extension_t e;
                e.tag = 0x56;
                m_desired_headerExtensions.extensionsVec.push_back(std::move(e));

                ReceiveDataSegment();
            }

            //2 HEADER EXTENSIONS WITH DATA, NO TRAILER EXTENSIONS, NO CHECKPOINT
            {
                m_desired_headerExtensions.extensionsVec.clear();
                m_desired_trailerExtensions.extensionsVec.clear();
                {
                    Ltp::ltp_extension_t e;
                    e.tag = 0x60;
                    e.valueVec.assign(502, 'h');
                    m_desired_headerExtensions.extensionsVec.push_back(std::move(e));
                }
                {
                    Ltp::ltp_extension_t e;
                    e.tag = 0x61;
                    e.valueVec.assign(51, 'i');
                    m_desired_headerExtensions.extensionsVec.push_back(std::move(e));
                }

                ReceiveDataSegment();
            }

            //2 HEADER EXTENSIONS WITH DATA, 2 TRAILER EXTENSIONS WITH DATA, NO CHECKPOINT
            {
                m_desired_headerExtensions.extensionsVec.clear();
                m_desired_trailerExtensions.extensionsVec.clear();
                {
                    Ltp::ltp_extension_t e;
                    e.tag = 0x70;
                    e.valueVec.assign(502, 'A');
                    m_desired_headerExtensions.extensionsVec.push_back(std::move(e));
                }
                {
                    Ltp::ltp_extension_t e;
                    e.tag = 0x71;
                    e.valueVec.assign(51, 'B');
                    m_desired_headerExtensions.extensionsVec.push_back(std::move(e));
                }
                {
                    Ltp::ltp_extension_t e;
                    e.tag = 0x72;
                    e.valueVec.assign(502, 'C');
                    m_desired_trailerExtensions.extensionsVec.push_back(std::move(e));
                }
                {
                    Ltp::ltp_extension_t e;
                    e.tag = 0x73;
                    e.valueVec.assign(51, 'D');
                    m_desired_trailerExtensions.extensionsVec.push_back(std::move(e));
                }

                ReceiveDataSegment();
            }

            //2 HEADER EXTENSIONS WITH NO DATA, 2 TRAILER EXTENSIONS WITH NO DATA, NO CHECKPOINT
            {
                m_desired_headerExtensions.extensionsVec.clear();
                m_desired_trailerExtensions.extensionsVec.clear();
                {
                    Ltp::ltp_extension_t e;
                    e.tag = 0x80;
                    m_desired_headerExtensions.extensionsVec.push_back(std::move(e));
                }
                {
                    Ltp::ltp_extension_t e;
                    e.tag = 0x81;
                    m_desired_headerExtensions.extensionsVec.push_back(std::move(e));
                }
                {
                    Ltp::ltp_extension_t e;
                    e.tag = 0x82;
                    m_desired_trailerExtensions.extensionsVec.push_back(std::move(e));
                }
                {
                    Ltp::ltp_extension_t e;
                    e.tag = 0x83;
                    m_desired_trailerExtensions.extensionsVec.push_back(std::move(e));
                }

                ReceiveDataSegment();
            }

            //2 HEADER EXTENSIONS WITH NO DATA, 2 TRAILER EXTENSIONS WITH NO DATA, ADD CHECKPOINT
            {
                m_desired_dataSegmentTypeFlags = LTP_DATA_SEGMENT_TYPE_FLAGS::REDDATA_CHECKPOINT;
                uint64_t cp = 1000;
                uint64_t rp = 2000;
                m_desired_dataSegmentMetadata.checkpointSerialNumber = &cp;
                m_desired_dataSegmentMetadata.reportSerialNumber = &rp;

                ReceiveDataSegment();
            }
        }




        //typedef boost::function<void(uint8_t segmentTypeFlags, uint64_t sessionOriginatorEngineId, uint64_t sessionNumber,
        //std::vector<uint8_t> & clientServiceDataVec, uint64_t clientServiceId, uint64_t offset, uint64_t length,
        //uint64_t * checkpointSerialNumber, uint64_t * reportSerialNumber) > DataSegmentContentsReadCallback_t;


        void DataSegmentCallback(uint8_t segmentTypeFlags, uint64_t sessionOriginatorEngineId, uint64_t sessionNumber,
            std::vector<uint8_t> & clientServiceDataVec, const Ltp::data_segment_metadata_t & dataSegmentMetadata,
            Ltp::ltp_extensions_t & headerExtensions, Ltp::ltp_extensions_t & trailerExtensions)
        {
            ++m_numDataSegmentCallbackCount;
            BOOST_REQUIRE_EQUAL(segmentTypeFlags, static_cast<uint8_t>(m_desired_dataSegmentTypeFlags));
            BOOST_REQUIRE_EQUAL(sessionOriginatorEngineId, m_desired_sessionOriginatorEngineId);
            BOOST_REQUIRE_EQUAL(sessionNumber, m_desired_sessionNumber);
            BOOST_REQUIRE(dataSegmentMetadata == m_desired_dataSegmentMetadata);
            BOOST_REQUIRE(headerExtensions == m_desired_headerExtensions);
            BOOST_REQUIRE(trailerExtensions == m_desired_trailerExtensions);
            BOOST_REQUIRE_EQUAL(m_desired_dataSegmentMetadata.length, clientServiceDataVec.size());

            BOOST_REQUIRE(clientServiceDataVec == m_desired_clientServiceDataVec);
        }


    };

    TestLtp t;

    BOOST_REQUIRE(t.m_ltp.IsAtBeginningState());
    t.DoDataSegment();
    BOOST_REQUIRE(t.m_ltp.IsAtBeginningState());

    BOOST_REQUIRE(t.m_ltp.IsAtBeginningState());
    t.DoReportSegment();
    BOOST_REQUIRE(t.m_ltp.IsAtBeginningState());
}
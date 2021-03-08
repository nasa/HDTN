#include <arpa/inet.h>
#include <egress.h>
#include <iostream>
#include <boost/test/unit_test.hpp>

// Create a test fixture.  This text fixture also inherits from the class being
// tested so it can access the protected memebers.
class HegrUdpEntryFixture : public hdtn::HegrUdpEntry {
public:
    HegrUdpEntryFixture();
    ~HegrUdpEntryFixture();
};

HegrUdpEntryFixture::HegrUdpEntryFixture() {
    //    std::cout << "Called HegrUdpEntryFixture::HegrUdpEntryFixture()" <<
    //    std::endl;
}

HegrUdpEntryFixture::~HegrUdpEntryFixture() {
    //    std::cout << "Called HegrUdpEntryFixture::~HegrUdpEntryFixture()" <<
    //    std::endl;
}


// Create a test fixture.  This text fixture also inherits from the class being
// tested so it can access the protected memebers.
class HegrStcpEntryFixture : public hdtn::HegrStcpEntry {
public:
    HegrStcpEntryFixture();
    ~HegrStcpEntryFixture();
};

HegrStcpEntryFixture::HegrStcpEntryFixture() {
    //    std::cout << "Called HegrUdpEntryFixture::HegrUdpEntryFixture()" <<
    //    std::endl;
}

HegrStcpEntryFixture::~HegrStcpEntryFixture() {
    //    std::cout << "Called HegrStcpEntryFixture::~HegrUdpEntryFixture()" <<
    //    std::endl;
}

BOOST_FIXTURE_TEST_CASE( InitTestNominal1, HegrStcpEntryFixture ) {
//    std::cout << "Running test case InitTestNominal1" << std::endl << std::flush;
    uint64_t flags = HEGR_FLAG_UDP;
    const char* dst = "127.0.0.1";
    int port = 4557;
    struct sockaddr_in saddr;
    saddr.sin_port = htons((uint16_t)port);
    saddr.sin_family = AF_INET;
    inet_pton(AF_INET, dst, &(saddr.sin_addr));
    this->Init(&saddr, flags);
    BOOST_CHECK_EQUAL(saddr.sin_family, this->m_ipv4.sin_family);
    BOOST_CHECK_EQUAL(saddr.sin_port, this->m_ipv4.sin_port);
    BOOST_CHECK_EQUAL(saddr.sin_addr.s_addr, this->m_ipv4.sin_addr.s_addr);
}

BOOST_FIXTURE_TEST_CASE(LabelTestNominal1, HegrUdpEntryFixture) {
//    std::cout << "Running test case LabelTestNominal1" << std::endl << std::flush;
    this->Label(1);
    BOOST_CHECK_EQUAL(1, this->m_label);
}

BOOST_FIXTURE_TEST_CASE(EnableTestNominal1, HegrUdpEntryFixture) {
//    std::cout << "Running test case EnableTestNominal1" << std::endl << std::flush;
    uint64_t flags = HEGR_FLAG_UDP;
    const char* dst = "127.0.0.1";
    int port = 4557;
    struct sockaddr_in saddr;
    saddr.sin_port = htons((uint16_t)port);
    saddr.sin_family = AF_INET;
    inet_pton(AF_INET, dst, &(saddr.sin_addr));
    this->Init(&saddr, flags);
    int returnCode = this->Enable();
    BOOST_CHECK_EQUAL(0, returnCode);
    BOOST_CHECK_EQUAL(HEGR_FLAG_UP, this->m_flags & HEGR_FLAG_UP);
}

// It does not seem right that this test is valid.
BOOST_FIXTURE_TEST_CASE(EnableTestOffNominal1,HegrUdpEntryFixture) {
//    std::cout << "Running test case EnableTestOffNominal1" << std::endl << std::flush;
    // Calling enable without first calling init
    int returnCode = this->Enable();
    BOOST_CHECK_EQUAL(0, returnCode);
    BOOST_CHECK_EQUAL(HEGR_FLAG_UP, this->m_flags & HEGR_FLAG_UP);
}

BOOST_FIXTURE_TEST_CASE(DisableTestNominal1, HegrUdpEntryFixture) {
//    std::cout << "Running test case DisableTestNominal1" << std::endl << std::flush;
    this->Enable();
    int returnCode = this->Disable();
    BOOST_CHECK_EQUAL(0, returnCode);
    BOOST_CHECK_NE(HEGR_FLAG_UP, this->m_flags & HEGR_FLAG_UP);
}


// NOTE -- the method Name is not properly implemented in the code
// NOTE -- The method, name, takes a char *.  This should probably be a const
// char * as it is not meant to be mutable.  The code below gives us a warning.
//BOOST_FIXTURE_TEST_CASE(NameTestNominal1, HegrUdpEntryFixture) {
//    std::cout << "Running test case NameTestNominal1" << std::endl << std::flush;
//    this->Name("Test Name");
//    // BOOST_TEST("Test Name" == this->_name);
//}


// NOTE -- _rate is missing or commented out.
//BOOST_FIXTURE_TEST_CASE(RateTestNominal1, HegrUdpEntryFixture) {
//    this->Rate(1000);
//    BOOST_CHECK_EQUAL(1000,this->_rate);
//}


// Google Test remnants below -----


//// Test Driven Development.  Code currenly does not throw exception for negative
//// value though it should. We can comment this test out until we are ready to
//// add this feature.
//// TEST_F(HegrUdpEntryFixture, LabelTestOffNominalNegativeValue)
////{
////    ASSERT_THROW(this->label(-1), std::runtime_error);
////}


//// Test Driven Devleopment.  Code currenly does not throw exception for negative
//// value though it should. We can comment this test out until we are ready to
//// add this feature.
//// TEST_F(HegrUdpEntryFixture, RateTestOffNominalNegativeValue)
////{
////    ASSERT_THROW(this->rate(-1), std::runtime_error);
////}

//// TEST_F(HegrUdpEntryFixture,ForwardTestNominal1) {
////    this->enable();
////    int returnCode = this->disable();
////    EXPECT_EQ(0,returnCode) << "Problem HegrUdpEntryFixture,
////    DisableTestNominal1, comparison with returnCode";
////    EXPECT_NE(HEGR_FLAG_UP,this->flags_ & HEGR_FLAG_UP) << "Problem
////    HegrUdpEntryFixture, DisableTestNominal1, comparison with flags_";
////}

//TEST_F(HegrUdpEntryFixture, DISABLED_ForwardTestNominal1) {
//    FAIL() << "Test needed for class HegrUdpEntry, method forward.";
//}

//TEST_F(HegrUdpEntryFixture, DISABLED_UpdateTestNominal1) {
//    FAIL() << "Test needed for class HegrUdpEntry, method update.";
//}

//TEST_F(HegrStcpEntryFixture, DISABLED_InitTestNominal1) {
//    FAIL() << "Test needed for class HegrStcpEntry, method init.";
//}

//TEST_F(HegrStcpEntryFixture, DISABLED_RateTestNominal1) {
//    FAIL() << "Test needed for class HegrStcpEntry, method rate.";
//}

//TEST_F(HegrStcpEntryFixture, DISABLED_ForwardTestNominal1) {
//    FAIL() << "Test needed for class HegrStcpEntry, method forward.";
//}

//TEST_F(HegrStcpEntryFixture, DISABLED_UpdateTestNominal1) {
//    FAIL() << "Test needed for class HegrStcpEntry, method update.";
//}

//TEST_F(HegrStcpEntryFixture, DISABLED_EnableTestNominal1) {
//    FAIL() << "Test needed for class HegrStcpEntry, method enable.";
//}

//TEST_F(HegrStcpEntryFixture, DISABLED_DisableTestNominal1) {
//    FAIL() << "Test needed for class HegrStcpEntry, method disable.";
//}



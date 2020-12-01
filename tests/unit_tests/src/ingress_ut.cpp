#include <gtest/gtest.h>
#include <ingress.h>
#include <iostream>
#include <arpa/inet.h>

// Create a test fixture.  
class BpIngressSyscallFixture : public testing::Test {
public:
    BpIngressSyscallFixture();
    ~BpIngressSyscallFixture();
    void SetUp() override;          // This is called after constructor.
    void TearDown() override;       // This is called before destructor.
    
    hdtn3::bp_ingress_syscall * ptrBpIngressSyscall;
};

BpIngressSyscallFixture::BpIngressSyscallFixture() {
//    std::cout << "Called BpIngressSyscallFixture::BpIngressSyscallFixture()" << std::endl;
	ptrBpIngressSyscall = new hdtn3::bp_ingress_syscall();
}

BpIngressSyscallFixture::~BpIngressSyscallFixture() {
//    std::cout << "Called BpIngressSyscallFixture::~BpIngressSyscallFixture()" << std::endl;
	delete ptrBpIngressSyscall;
}

void BpIngressSyscallFixture::SetUp() {
//    std::cout << "BpIngressSyscallFixture::SetUp called\n";
}

void BpIngressSyscallFixture::TearDown() {
//    std::cout << "BpIngressSyscallFixture::TearDown called\n";
}

TEST_F(BpIngressSyscallFixture,DestroyTestNominal1) {
    FAIL() << "Test needed for class bp_ingress_syscall, method destroy.";
}

TEST_F(BpIngressSyscallFixture,InitTestNominal1) {
    FAIL() << "Test needed for class bp_ingress_syscall, method init.";
}

TEST_F(BpIngressSyscallFixture,NetstartTestNominal1) {
    FAIL() << "Test needed for class bp_ingress_syscall, method netstart.";
}

TEST_F(BpIngressSyscallFixture,ProcessTestNominal1) {
    FAIL() << "Test needed for class bp_ingress_syscall, method process.";
}

TEST_F(BpIngressSyscallFixture,SendTelemetryTestNominal1) {
    FAIL() << "Test needed for class bp_ingress_syscall, method send_telemetry.";
}

TEST_F(BpIngressSyscallFixture,UpdateTestNominal1) {
    FAIL() << "Test needed for class bp_ingress_syscall, method update.";
}

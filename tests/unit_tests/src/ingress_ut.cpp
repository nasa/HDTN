#include <arpa/inet.h>
#include <ingress.h>
#include <iostream>
#include <boost/test/unit_test.hpp>

// Create a test fixture.
class BpIngressSyscallFixture  {
public:
    BpIngressSyscallFixture();
    ~BpIngressSyscallFixture();
private:
    hdtn::BpIngressSyscall* ptrBpIngressSyscall;
};

BpIngressSyscallFixture::BpIngressSyscallFixture() {
    //    std::cout << "Called BpIngressSyscallFixture::BpIngressSyscallFixture()"
    //    << std::endl;
    ptrBpIngressSyscall = new hdtn::BpIngressSyscall();
}

BpIngressSyscallFixture::~BpIngressSyscallFixture() {
    //    std::cout << "Called
    //    BpIngressSyscallFixture::~BpIngressSyscallFixture()" << std::endl;
    delete ptrBpIngressSyscall;
}

//TEST_F(BpIngressSyscallFixture, DISABLED_DestroyTestNominal1) {
//    FAIL() << "Test needed for class bp_ingress_syscall, method destroy.";
//}

//TEST_F(BpIngressSyscallFixture, DISABLED_InitTestNominal1) {
//    FAIL() << "Test needed for class bp_ingress_syscall, method init.";
//}

//TEST_F(BpIngressSyscallFixture, DISABLED_NetstartTestNominal1) {
//    FAIL() << "Test needed for class bp_ingress_syscall, method netstart.";
//}

//TEST_F(BpIngressSyscallFixture, DISABLED_ProcessTestNominal1) {
//    FAIL() << "Test needed for class bp_ingress_syscall, method process.";
//}

//TEST_F(BpIngressSyscallFixture, DISABLED_SendTelemetryTestNominal1) {
//    FAIL() << "Test needed for class bp_ingress_syscall, method send_telemetry.";
//}

//TEST_F(BpIngressSyscallFixture, DISABLED_UpdateTestNominal1) {
//    FAIL() << "Test needed for class bp_ingress_syscall, method update.";
//}



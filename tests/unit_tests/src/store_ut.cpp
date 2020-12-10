#include <gtest/gtest.h>
#include <store.hpp>
#include <iostream>
#include <arpa/inet.h>

// Create a test fixture
class StorageFixture : public testing::Test {
public:
    StorageFixture();
    ~StorageFixture();
    void SetUp() override;          // This is called after constructor.
    void TearDown() override;       // This is called before destructor.
    
    hdtn::storage * ptrStorage;
    hdtn::storage_worker * ptrStorageWorker;
    hdtn::scheduler * ptrScheduler;
};

StorageFixture::StorageFixture() {
//    std::cout << "Called StorageFixture::StorageFixture()" << std::endl;
    ptrStorage = new hdtn::storage();
    ptrStorageWorker = new hdtn::storage_worker();
    ptrScheduler = new hdtn::scheduler();
}

StorageFixture::~StorageFixture() {
//    std::cout << "Called StorageFixture::~StorageFixture()" << std::endl;
    delete ptrStorage;
    delete ptrStorageWorker;
    delete ptrScheduler;
}

void StorageFixture::SetUp() {
//    std::cout << "StorageFixture::SetUp called\n";
}

void StorageFixture::TearDown() {
//    std::cout << "StorageFixture::TearDown called\n";
}


TEST_F(StorageFixture,InitTestNominal1) {
    FAIL() << "Test needed for class storage, method init.";
}

TEST_F(StorageFixture,UpdateTestNominal1) {
    FAIL() << "Test needed for class storage, method update.";
}

TEST_F(StorageFixture,DispatchTestNominal1) {
    FAIL() << "Test needed for class storage, method dispatch.";
}

TEST_F(StorageFixture,C2TelemTestNominal1) {
    FAIL() << "Test needed for class storage, method c2telem.";
}

TEST_F(StorageFixture,ReleaseTestNominal1) {
    FAIL() << "Test needed for class storage, method release.";
}

TEST_F(StorageFixture,IngressTestNominal1) {
    FAIL() << "Test needed for class storage, method ingress.";
}

TEST_F(StorageFixture,StatsTestNominal1) {
    FAIL() << "Test needed for class storage, method stats.";
}

TEST_F(StorageFixture,InitWorkerTestNominal1) {
    FAIL() << "Test needed for class storage_worker, method init.";
}

TEST_F(StorageFixture,LaunchTestNominal1) {
    FAIL() << "Test needed for class storage_worker, method launch.";
}

TEST_F(StorageFixture,ExecuteTestNominal1) {
    FAIL() << "Test needed for class storage_worker, method execute.";
}

TEST_F(StorageFixture,ThreadTestNominal1) {
    FAIL() << "Test needed for class storage_worker, method thread.";
}

TEST_F(StorageFixture,WriteTestNominal1) {
    FAIL() << "Test needed for class storage_worker, method write.";
}

TEST_F(StorageFixture,InitSchedulerTestNominal1) {
    FAIL() << "Test needed for class scheduler, method init.";
}

TEST_F(StorageFixture,AddTestNominal1) {
    FAIL() << "Test needed for class scheduler, method add.";
}

TEST_F(StorageFixture,NextTestNominal1) {
    FAIL() << "Test needed for class scheduler, method next.";
}



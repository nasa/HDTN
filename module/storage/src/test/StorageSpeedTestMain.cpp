/**
 * @file StorageSpeedTestMain.cpp
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

#include <string>
#include "BundleStorageManagerMT.h"
#include "BundleStorageManagerAsio.h"
#include <boost/make_unique.hpp>
#include <boost/random/mersenne_twister.hpp>
#include <boost/random/uniform_int_distribution.hpp>
#include <boost/timer/timer.hpp>
#include "SignalHandler.h"
#include "Logger.h"
#include <atomic>

static const uint64_t PRIMARY_SRC_NODE = 100;
static const uint64_t PRIMARY_SRC_SVC = 1;
//static const uint64_t PRIMARY_DEST_NODE = 300;
//static const uint64_t PRIMARY_DEST_SVC = 3;
//static const uint64_t PRIMARY_TIME = 1000;
//static const uint64_t PRIMARY_LIFETIME = 2000;
static const uint64_t PRIMARY_SEQ = 1;
static constexpr hdtn::Logger::SubProcess subprocess = hdtn::Logger::SubProcess::storage;

static std::atomic<bool> g_running(true);

static void MonitorExitKeypressThreadFunction() {
    LOG_INFO(subprocess) << "Keyboard Interrupt.. exiting";
    g_running = false; //do this first
}

static SignalHandler g_sigHandler(boost::bind(&MonitorExitKeypressThreadFunction));



struct TestFile {
    TestFile() {}
    TestFile(boost::uint64_t size) : m_data(size) {
        boost::random::mt19937 gen(static_cast<unsigned int>(std::time(0)));
        const boost::random::uniform_int_distribution<> distRandomData(0, 255);
        for (std::size_t i = 0; i < size; ++i) {
            m_data[i] = distRandomData(gen);
        }
    }
    padded_vector_uint8_t m_data;
};

//two days
#define NUMBER_OF_EXPIRATIONS (86400*2)

bool TestSpeed(BundleStorageManagerBase & bsm) {
    boost::random::mt19937 gen(static_cast<unsigned int>(std::time(0)));
    const boost::random::uniform_int_distribution<> distLinkId(0, 9);
    const boost::random::uniform_int_distribution<> distFileId(0, 9);
    const boost::random::uniform_int_distribution<> distPriorityIndex(0, 2);
    const boost::random::uniform_int_distribution<> distAbsExpiration(0, NUMBER_OF_EXPIRATIONS - 1);
    const boost::random::uniform_int_distribution<> distTotalBundleSize(1, 65536);

    g_sigHandler.Start();

    static const cbhe_eid_t DEST_LINKS[10] = {
        cbhe_eid_t(1,1),
        cbhe_eid_t(2,1),
        cbhe_eid_t(3,1),
        cbhe_eid_t(4,1),
        cbhe_eid_t(5,1),
        cbhe_eid_t(6,1),
        cbhe_eid_t(7,1),
        cbhe_eid_t(8,1),
        cbhe_eid_t(9,1),
        cbhe_eid_t(10,1)
    };
    const std::vector<cbhe_eid_t> availableDestLinks = {
        cbhe_eid_t(1,1),
        cbhe_eid_t(2,1),
        cbhe_eid_t(3,1),
        cbhe_eid_t(4,1),
        cbhe_eid_t(5,1),
        cbhe_eid_t(6,1),
        cbhe_eid_t(7,1),
        cbhe_eid_t(8,1),
        cbhe_eid_t(9,1),
        cbhe_eid_t(10,1)
    };



    bsm.Start();

    

    static const boost::uint64_t sizes[10] = {

        BUNDLE_STORAGE_PER_SEGMENT_SIZE - 2 ,
        BUNDLE_STORAGE_PER_SEGMENT_SIZE + 2,

        2 * BUNDLE_STORAGE_PER_SEGMENT_SIZE - 2 ,
        2 * BUNDLE_STORAGE_PER_SEGMENT_SIZE + 2,

        500 * BUNDLE_STORAGE_PER_SEGMENT_SIZE - 2 ,
        500 * BUNDLE_STORAGE_PER_SEGMENT_SIZE + 2,

        1000 * BUNDLE_STORAGE_PER_SEGMENT_SIZE - 2 ,
        1000 * BUNDLE_STORAGE_PER_SEGMENT_SIZE + 2,

        10000 * BUNDLE_STORAGE_PER_SEGMENT_SIZE - 2 ,
        10000 * BUNDLE_STORAGE_PER_SEGMENT_SIZE + 2,
    };
    LOG_INFO(subprocess) << "generating test files";
    std::vector<TestFile> testFiles(10);
    std::map<boost::uint64_t, TestFile*> fileMap;
    for (std::size_t i = 0; i < 10; ++i) {
        testFiles[i] = TestFile(sizes[i]);
        fileMap[sizes[i]] = &testFiles[i];
    }
    LOG_INFO(subprocess) << "done generating test files";

    boost::uint64_t totalSegmentsStoredOnDisk = 0;
    double gigaBitsPerSecReadDoubleAvg = 0.0, gigaBitsPerSecWriteDoubleAvg = 0.0;
    const unsigned int NUM_TESTS = 5;
    uint64_t custodyId = 0;
    for (unsigned int testI = 0; testI < NUM_TESTS; ++testI) {

        {
            LOG_INFO(subprocess) << "filling up the storage";
            boost::uint64_t totalBytesWrittenThisTest = 0;
            boost::timer::cpu_timer timer;
            while (g_running.load(std::memory_order_acquire)) {
                const unsigned int fileIdx = distFileId(gen);
                padded_vector_uint8_t& data = testFiles[fileIdx].m_data;
                const boost::uint64_t size = data.size();

                const unsigned int linkId = distLinkId(gen);
                const unsigned int priorityIndex = distPriorityIndex(gen) & 3;
                static const BPV6_BUNDLEFLAG priorityBundleFlags[4] = {
                    BPV6_BUNDLEFLAG::PRIORITY_BULK, BPV6_BUNDLEFLAG::PRIORITY_NORMAL, BPV6_BUNDLEFLAG::PRIORITY_EXPEDITED, BPV6_BUNDLEFLAG::PRIORITY_BIT_MASK
                };
                const uint64_t absExpiration = distAbsExpiration(gen);

                BundleStorageManagerSession_WriteToDisk sessionWrite;
                Bpv6CbhePrimaryBlock primary;
                primary.SetZero();
                primary.m_bundleProcessingControlFlags = priorityBundleFlags[priorityIndex] | (BPV6_BUNDLEFLAG::SINGLETON | BPV6_BUNDLEFLAG::NOFRAGMENT);
                primary.m_sourceNodeId.Set(PRIMARY_SRC_NODE, PRIMARY_SRC_SVC);
                primary.m_destinationEid = DEST_LINKS[linkId];
                primary.m_custodianEid.SetZero();
                primary.m_creationTimestamp.secondsSinceStartOfYear2000 = 0;
                primary.m_lifetimeSeconds = absExpiration;
                primary.m_creationTimestamp.sequenceNumber = PRIMARY_SEQ;

                boost::uint64_t totalSegmentsRequired = bsm.Push(sessionWrite, primary, size);
                if (totalSegmentsRequired == 0) break;
                totalSegmentsStoredOnDisk += totalSegmentsRequired;
                totalBytesWrittenThisTest += size;

                ++custodyId;
                const uint64_t totalBytesPushed = bsm.PushAllSegments(sessionWrite, primary, custodyId, data.data(), data.size());
                
            }
            const boost::uint64_t nanoSecWall = timer.elapsed().wall;
            const double bytesPerNanoSecDouble = static_cast<double>(totalBytesWrittenThisTest) / static_cast<double>(nanoSecWall);
            const double gigaBytesPerSecDouble = bytesPerNanoSecDouble;// / 1e9 * 1e9;
            const double gigaBitsPerSecDouble = gigaBytesPerSecDouble * 8.0;
            gigaBitsPerSecWriteDoubleAvg += gigaBitsPerSecDouble;
            LOG_DEBUG(subprocess) << "WRITE GBits/sec=" << gigaBitsPerSecDouble;
        }
        {
            LOG_INFO(subprocess) << "reading half of the stored";
            boost::uint64_t totalBytesReadThisTest = 0;
            boost::timer::cpu_timer timer;
            BundleStorageManagerSession_ReadFromDisk sessionRead; //reuse (heap allocated)
            while (g_running.load(std::memory_order_acquire)) {

                
                boost::uint64_t bytesToReadFromDisk = bsm.PopTop(sessionRead, availableDestLinks);
                padded_vector_uint8_t dataReadBack(bytesToReadFromDisk);
                TestFile & originalFile = *fileMap[bytesToReadFromDisk];

                const std::size_t numSegmentsToRead = sessionRead.catalogEntryPtr->segmentIdChainVec.size();
                bsm.ReadAllSegments(sessionRead, dataReadBack);
                const std::size_t totalBytesRead = dataReadBack.size();
                
                if (totalBytesRead != bytesToReadFromDisk) return false;
                totalBytesReadThisTest += totalBytesRead;
                if (dataReadBack != originalFile.m_data) {
                    LOG_WARNING(subprocess) << "dataReadBack does not equal data";
                    return false;
                }
                if (!bsm.RemoveReadBundleFromDisk(sessionRead)) {
                    LOG_ERROR(subprocess) << "error freeing bundle from disk";
                    return false;
                }

                totalSegmentsStoredOnDisk -= numSegmentsToRead;
                if (totalSegmentsStoredOnDisk < (bsm.M_MAX_SEGMENTS / 2)) {
                    break;
                }
            }
            const boost::uint64_t nanoSecWall = timer.elapsed().wall;
            const double bytesPerNanoSecDouble = static_cast<double>(totalBytesReadThisTest) / static_cast<double>(nanoSecWall);
            const double gigaBytesPerSecDouble = bytesPerNanoSecDouble;// / 1e9 * 1e9;
            const double gigaBitsPerSecDouble = gigaBytesPerSecDouble * 8.0;
            gigaBitsPerSecReadDoubleAvg += gigaBitsPerSecDouble;
            LOG_DEBUG(subprocess) << "READ GBits/sec=" << gigaBitsPerSecDouble;
        }
    }

    if (g_running.load(std::memory_order_acquire)) {
        LOG_DEBUG(subprocess) << "Read avg GBits/sec=" << gigaBitsPerSecReadDoubleAvg / NUM_TESTS;
        LOG_DEBUG(subprocess) << "Write avg GBits/sec=" << gigaBitsPerSecWriteDoubleAvg / NUM_TESTS;
    }
    return true;

}


int main() {
    hdtn::Logger::initializeWithProcess(hdtn::Logger::Process::storagespeedtest);
    std::unique_ptr<BundleStorageManagerBase> bsmPtr;
    if (true) {
        boost::make_unique<BundleStorageManagerMT>();
    }
    else {
        boost::make_unique<BundleStorageManagerAsio>();
    }
    LOG_INFO(subprocess) << TestSpeed(*bsmPtr);
    return 0;
}

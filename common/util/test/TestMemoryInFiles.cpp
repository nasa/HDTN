/**
 * @file TestMemoryInFiles.cpp
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
#include "MemoryInFiles.h"
#include <boost/bind/bind.hpp>
#include <boost/make_unique.hpp>
#include "Logger.h"

static constexpr hdtn::Logger::SubProcess subprocess = hdtn::Logger::SubProcess::unittest;

static constexpr uint64_t blockSize = 4096;

BOOST_AUTO_TEST_CASE(MemoryInFilesCeilToNearestBlockMultipleTestCase)
{
    //pair = value, expected
    typedef std::pair<uint64_t, uint64_t> pairVE;
    
    static const std::vector<pairVE> testValuesPlusExpected = {
        pairVE(0, 0),
        pairVE(1, blockSize),
        pairVE(2, blockSize),
        pairVE(blockSize - 2, blockSize),
        pairVE(blockSize - 1, blockSize),
        pairVE(blockSize - 0, blockSize),
        pairVE(blockSize + 1, 2 * blockSize),
        pairVE(blockSize + 2, 2 * blockSize),
        pairVE((2 * blockSize) - 2, 2 * blockSize),
        pairVE((2 * blockSize) - 1, 2 * blockSize),
        pairVE((2 * blockSize) - 0, 2 * blockSize),
        pairVE((2 * blockSize) + 1, 3 * blockSize),
        pairVE((2 * blockSize) + 2, 3 * blockSize),
    };

    for (std::size_t i = 0; i < testValuesPlusExpected.size(); ++i) {
        const pairVE& p = testValuesPlusExpected[i];
        const uint64_t value = p.first;
        const uint64_t expected = p.second;
        BOOST_REQUIRE_EQUAL(MemoryInFiles::CeilToNearestBlockMultiple(value), expected);
    }
}

BOOST_AUTO_TEST_CASE(MemoryInFilesTestCase)
{
    namespace fs = boost::filesystem;

    struct Test {
        boost::asio::io_service ioService;
        std::unique_ptr<boost::asio::io_service::work> work = 
            boost::make_unique<boost::asio::io_service::work>(ioService); //prevent having to call ioService.reset()
        const fs::path rootPath;
        const uint64_t newFileAggregationTimeMs;
        MemoryInFiles mf;
        std::shared_ptr<std::string> lastDataReadBackCallbackSharedPtr;
        std::shared_ptr<std::vector<std::string> > lastMultiDataReadBackCallbackSharedPtr;
        unsigned int readMultiCount;
        unsigned int writeMultiCount;
        unsigned int readSingleCount;
        unsigned int writeSingleCount;
        Test(const fs::path & paramRootPath) :
            rootPath(paramRootPath),
            newFileAggregationTimeMs(2000),
            mf(ioService, rootPath, newFileAggregationTimeMs, 10) {}

        void DoTest() {
            
            { //create memoryBlockId=1, size 10, write "56789" to indices 5..9
                const uint64_t totalMemoryBlockSize = 10;
                BOOST_REQUIRE_EQUAL(mf.GetCountTotalFilesCreated(), 0);
                BOOST_REQUIRE_EQUAL(mf.GetCountTotalFilesActive(), 0);
                const uint64_t memoryBlockId = mf.AllocateNewWriteMemoryBlock(totalMemoryBlockSize);
                BOOST_REQUIRE_EQUAL(memoryBlockId, 1);
                BOOST_REQUIRE_EQUAL(mf.GetSizeOfMemoryBlock(memoryBlockId), blockSize); //rounds 10 up to 4096
                std::shared_ptr<std::string> dataSharedPtr = std::make_shared<std::string>("56789");
                BOOST_REQUIRE_EQUAL(dataSharedPtr->size(), 5);
                BOOST_REQUIRE_EQUAL(mf.GetCountTotalFilesCreated(), 1);
                BOOST_REQUIRE_EQUAL(mf.GetCountTotalFilesActive(), 1);
                MemoryInFiles::deferred_write_t deferredWrite;
                deferredWrite.memoryBlockId = memoryBlockId;
                deferredWrite.offset = 5;
                deferredWrite.writeFromThisLocationPtr = dataSharedPtr->data();
                deferredWrite.length = dataSharedPtr->size();
                BOOST_REQUIRE(mf.WriteMemoryAsync(deferredWrite, boost::bind(&Test::WriteHandler, this, std::move(dataSharedPtr))));
                BOOST_REQUIRE_EQUAL(mf.GetCountTotalFilesCreated(), 1);
                BOOST_REQUIRE_EQUAL(mf.GetCountTotalFilesActive(), 1);
                BOOST_REQUIRE(!dataSharedPtr);
                BOOST_REQUIRE_EQUAL(ioService.run_one(), 1); //finish the write
                BOOST_REQUIRE_EQUAL(mf.GetCountTotalFilesCreated(), 1);
                BOOST_REQUIRE_EQUAL(mf.GetCountTotalFilesActive(), 1);
            }
            { //create memoryBlockId=2, size 9000 (this will create a gap between memory block 1's first and second fragments when resized next)
                const uint64_t totalMemoryBlockSize = 9000;
                const uint64_t memoryBlockId = mf.AllocateNewWriteMemoryBlock(totalMemoryBlockSize);
                BOOST_REQUIRE_EQUAL(memoryBlockId, 2);
                BOOST_REQUIRE_EQUAL(mf.GetSizeOfMemoryBlock(memoryBlockId), blockSize * 3); //rounds 9000 up to 4096*3=12288
            }
            { //resize memoryBlockId=1, size 7000 (rounds to 8192), write "vwxyz" to indices 6500..6504
                const uint64_t totalMemoryBlockSize = 7000;
                const uint64_t memoryBlockId = 1;
                const uint64_t newSize = mf.Resize(memoryBlockId, totalMemoryBlockSize);
                BOOST_REQUIRE_EQUAL(newSize, blockSize * 2);
                BOOST_REQUIRE_EQUAL(mf.GetSizeOfMemoryBlock(memoryBlockId), blockSize * 2); //rounds 10 up to 4096
                std::shared_ptr<std::string> dataSharedPtr = std::make_shared<std::string>("vwxyz");
                BOOST_REQUIRE_EQUAL(dataSharedPtr->size(), 5);
                BOOST_REQUIRE_EQUAL(mf.GetCountTotalFilesCreated(), 1);
                BOOST_REQUIRE_EQUAL(mf.GetCountTotalFilesActive(), 1);
                MemoryInFiles::deferred_write_t deferredWrite;
                deferredWrite.memoryBlockId = memoryBlockId;
                deferredWrite.offset = 6500;
                deferredWrite.writeFromThisLocationPtr = dataSharedPtr->data();
                deferredWrite.length = dataSharedPtr->size();
                BOOST_REQUIRE(mf.WriteMemoryAsync(deferredWrite, boost::bind(&Test::WriteHandler, this, std::move(dataSharedPtr))));
                BOOST_REQUIRE_EQUAL(mf.GetCountTotalFilesCreated(), 1);
                BOOST_REQUIRE_EQUAL(mf.GetCountTotalFilesActive(), 1);
                BOOST_REQUIRE(!dataSharedPtr);
                BOOST_REQUIRE_EQUAL(ioService.run_one(), 1); //finish the write
                BOOST_REQUIRE_EQUAL(mf.GetCountTotalFilesCreated(), 1);
                BOOST_REQUIRE_EQUAL(mf.GetCountTotalFilesActive(), 1);
            }
            { //write memoryBlockId=1 "qwerty" to indices 4094..4099 (spans two fragments)
                const uint64_t memoryBlockId = 1;
                std::shared_ptr<std::string> dataSharedPtr = std::make_shared<std::string>("qwerty");
                BOOST_REQUIRE_EQUAL(dataSharedPtr->size(), 6);
                BOOST_REQUIRE_EQUAL(mf.GetCountTotalFilesCreated(), 1);
                BOOST_REQUIRE_EQUAL(mf.GetCountTotalFilesActive(), 1);
                MemoryInFiles::deferred_write_t deferredWrite;
                deferredWrite.memoryBlockId = memoryBlockId;
                deferredWrite.offset = 4094;
                deferredWrite.writeFromThisLocationPtr = dataSharedPtr->data();
                deferredWrite.length = dataSharedPtr->size();
                BOOST_REQUIRE(mf.WriteMemoryAsync(deferredWrite, boost::bind(&Test::WriteHandler, this, std::move(dataSharedPtr))));
                BOOST_REQUIRE_EQUAL(mf.GetCountTotalFilesCreated(), 1);
                BOOST_REQUIRE_EQUAL(mf.GetCountTotalFilesActive(), 1);
                BOOST_REQUIRE(!dataSharedPtr);
                writeSingleCount = 0;
                BOOST_REQUIRE_EQUAL(ioService.run_one(), 1); //spans two fragments, so call twice
                BOOST_REQUIRE_EQUAL(writeSingleCount, 0);
                BOOST_REQUIRE_EQUAL(ioService.run_one(), 1); //finish the write
                BOOST_REQUIRE_EQUAL(writeSingleCount, 1);
                BOOST_REQUIRE_EQUAL(mf.GetCountTotalFilesCreated(), 1);
                BOOST_REQUIRE_EQUAL(mf.GetCountTotalFilesActive(), 1);
            }

            { //write "01" and "34" to memoryBlockId=1, indices 0..1 and 3..4 respectively
                const uint64_t memoryBlockId = 1;
                writeMultiCount = 0;
                std::shared_ptr<std::vector<std::string> > multiDataToWriteSharedPtr = std::make_shared<std::vector<std::string> >(2);
                std::vector<MemoryInFiles::deferred_write_t> deferredWritesVec(2);
                { //write "01"
                    std::string& s = (*multiDataToWriteSharedPtr)[0];
                    s.assign("01");
                    MemoryInFiles::deferred_write_t& deferredWrite = deferredWritesVec[0];
                    deferredWrite.memoryBlockId = memoryBlockId;
                    deferredWrite.offset = 0;
                    deferredWrite.writeFromThisLocationPtr = s.data();
                    deferredWrite.length = s.size();
                }
                { //write "34"
                    std::string& s = (*multiDataToWriteSharedPtr)[1];
                    s.assign("34");
                    MemoryInFiles::deferred_write_t& deferredWrite = deferredWritesVec[1];
                    deferredWrite.memoryBlockId = memoryBlockId;
                    deferredWrite.offset = 3;
                    deferredWrite.writeFromThisLocationPtr = s.data();
                    deferredWrite.length = s.size();
                }

                BOOST_REQUIRE(mf.WriteMemoryAsync(deferredWritesVec, boost::bind(&Test::WriteHandlerMulti, this, std::move(multiDataToWriteSharedPtr))));
                BOOST_REQUIRE_EQUAL(mf.GetCountTotalFilesCreated(), 1);
                BOOST_REQUIRE_EQUAL(mf.GetCountTotalFilesActive(), 1);
                BOOST_REQUIRE(!multiDataToWriteSharedPtr);
                BOOST_REQUIRE_EQUAL(ioService.run_one(), 1);
                BOOST_REQUIRE_EQUAL(writeMultiCount, 0); //called after next run_one
                BOOST_REQUIRE_EQUAL(ioService.run_one(), 1);
                BOOST_REQUIRE_EQUAL(writeMultiCount, 1); //finished
                BOOST_REQUIRE_EQUAL(mf.GetCountTotalFilesCreated(), 1);
                BOOST_REQUIRE_EQUAL(mf.GetCountTotalFilesActive(), 1);
            }
            { //read only "345678" from memoryBlockId=1, indices 3..8
                const uint64_t memoryBlockId = 1;
                lastDataReadBackCallbackSharedPtr.reset();
                std::shared_ptr<std::string> dataReadBackSharedPtr = std::make_shared<std::string>("aaaaaa"); //overwrite this initial value
                MemoryInFiles::deferred_read_t deferredRead;
                deferredRead.memoryBlockId = memoryBlockId;
                deferredRead.offset = 3;
                deferredRead.readToThisLocationPtr = dataReadBackSharedPtr->data();
                deferredRead.length = dataReadBackSharedPtr->size();
                BOOST_REQUIRE(mf.ReadMemoryAsync(deferredRead, boost::bind(&Test::ReadHandler,
                    this, boost::placeholders::_1, std::move(dataReadBackSharedPtr))));
                BOOST_REQUIRE(!dataReadBackSharedPtr);
                BOOST_REQUIRE_EQUAL(ioService.run_one(), 1);
                BOOST_REQUIRE(lastDataReadBackCallbackSharedPtr);
                BOOST_REQUIRE_EQUAL(*lastDataReadBackCallbackSharedPtr, "345678");
                BOOST_REQUIRE_EQUAL(mf.GetCountTotalFilesCreated(), 1);
                BOOST_REQUIRE_EQUAL(mf.GetCountTotalFilesActive(), 1);
            }

            { //read memoryBlockId=1 "qwerty" from indices 4094..4099 (spans two fragments)
                const uint64_t memoryBlockId = 1;
                lastDataReadBackCallbackSharedPtr.reset();
                std::shared_ptr<std::string> dataReadBackSharedPtr = std::make_shared<std::string>("aaaaaa"); //overwrite this initial value
                MemoryInFiles::deferred_read_t deferredRead;
                deferredRead.memoryBlockId = memoryBlockId;
                deferredRead.offset = 4094;
                deferredRead.readToThisLocationPtr = dataReadBackSharedPtr->data();
                deferredRead.length = dataReadBackSharedPtr->size();
                BOOST_REQUIRE(mf.ReadMemoryAsync(deferredRead, boost::bind(&Test::ReadHandler,
                    this, boost::placeholders::_1, std::move(dataReadBackSharedPtr))));
                BOOST_REQUIRE(!dataReadBackSharedPtr);
                readSingleCount = 0;
                BOOST_REQUIRE_EQUAL(ioService.run_one(), 1); //two calls needed for the span of two fragments
                BOOST_REQUIRE(!lastDataReadBackCallbackSharedPtr);
                BOOST_REQUIRE_EQUAL(readSingleCount, 0);
                BOOST_REQUIRE_EQUAL(ioService.run_one(), 1);
                BOOST_REQUIRE(lastDataReadBackCallbackSharedPtr);
                BOOST_REQUIRE_EQUAL(readSingleCount, 1);
                BOOST_REQUIRE_EQUAL(*lastDataReadBackCallbackSharedPtr, "qwerty");
                BOOST_REQUIRE_EQUAL(mf.GetCountTotalFilesCreated(), 1);
                BOOST_REQUIRE_EQUAL(mf.GetCountTotalFilesActive(), 1);
            }

            { //read "56" and "89" from memoryBlockId=1, indices 5..6 and 8..9 respectively
                const uint64_t memoryBlockId = 1;
                lastMultiDataReadBackCallbackSharedPtr.reset();
                readMultiCount = 0;
                std::shared_ptr<std::vector<std::string> > multiDataReadBackSharedPtr = std::make_shared<std::vector<std::string> >(2);
                std::vector<MemoryInFiles::deferred_read_t> deferredReadsVec(2);
                { //read "56"
                    std::string& s = (*multiDataReadBackSharedPtr)[0];
                    s.assign("aa");
                    MemoryInFiles::deferred_read_t & deferredRead = deferredReadsVec[0];
                    deferredRead.memoryBlockId = memoryBlockId;
                    deferredRead.offset = 5;
                    deferredRead.readToThisLocationPtr = s.data();
                    deferredRead.length = s.size();
                }
                { //read "89"
                    std::string& s = (*multiDataReadBackSharedPtr)[1];
                    s.assign("bb");
                    MemoryInFiles::deferred_read_t& deferredRead = deferredReadsVec[1];
                    deferredRead.memoryBlockId = memoryBlockId;
                    deferredRead.offset = 8;
                    deferredRead.readToThisLocationPtr = s.data();
                    deferredRead.length = s.size();
                }

                BOOST_REQUIRE(mf.ReadMemoryAsync(deferredReadsVec, boost::bind(&Test::ReadHandlerMulti,
                    this, boost::placeholders::_1, std::move(multiDataReadBackSharedPtr))));
                BOOST_REQUIRE(!multiDataReadBackSharedPtr);
                BOOST_REQUIRE_EQUAL(ioService.run_one(), 1);
                BOOST_REQUIRE_EQUAL(readMultiCount, 0); //called after next run_one
                BOOST_REQUIRE_EQUAL(ioService.run_one(), 1);
                BOOST_REQUIRE_EQUAL(readMultiCount, 1); //finished
                BOOST_REQUIRE(lastMultiDataReadBackCallbackSharedPtr);
                BOOST_REQUIRE_EQUAL((*lastMultiDataReadBackCallbackSharedPtr)[0], "56");
                BOOST_REQUIRE_EQUAL((*lastMultiDataReadBackCallbackSharedPtr)[1], "89");
                BOOST_REQUIRE_EQUAL(mf.GetCountTotalFilesCreated(), 1);
                BOOST_REQUIRE_EQUAL(mf.GetCountTotalFilesActive(), 1);
            }

            { //read fail (out of bounds)
                const uint64_t memoryBlockId = 1;
                lastDataReadBackCallbackSharedPtr.reset();
                std::shared_ptr<std::string> dataReadBackSharedPtr = std::make_shared<std::string>("aaa");
                char* data = dataReadBackSharedPtr->data();
                const uint64_t sizeData = dataReadBackSharedPtr->size();
                MemoryInFiles::deferred_read_t deferredRead;
                deferredRead.memoryBlockId = memoryBlockId;
                deferredRead.offset = 8190;
                deferredRead.readToThisLocationPtr = data;
                deferredRead.length = sizeData;
                BOOST_REQUIRE(!mf.ReadMemoryAsync(deferredRead, boost::bind(&Test::ReadHandler,
                    this, boost::placeholders::_1, std::move(dataReadBackSharedPtr))));
                BOOST_REQUIRE_EQUAL(mf.GetCountTotalFilesCreated(), 1);
                BOOST_REQUIRE_EQUAL(mf.GetCountTotalFilesActive(), 1);
            }

           

            { //expire the 3 second timer, create new memoryBlockId=3 in second file, write "abcdefg" to indices 0..6
                LOG_INFO(subprocess) << "waiting for 2 second timer to expire..";
                BOOST_REQUIRE_EQUAL(ioService.run_one(), 1);
                LOG_INFO(subprocess) << "expired";
                BOOST_REQUIRE_EQUAL(mf.GetCountTotalFilesCreated(), 1); //no allocation yet
                BOOST_REQUIRE_EQUAL(mf.GetCountTotalFilesActive(), 1);

                std::shared_ptr<std::string> dataSharedPtr = std::make_shared<std::string>("abcdefg");
                const uint64_t memoryBlockId = mf.AllocateNewWriteMemoryBlock(dataSharedPtr->size());
                BOOST_REQUIRE_EQUAL(memoryBlockId, 3);
                
                BOOST_REQUIRE_EQUAL(dataSharedPtr->size(), 7);
                BOOST_REQUIRE_EQUAL(mf.GetCountTotalFilesCreated(), 2);
                BOOST_REQUIRE_EQUAL(mf.GetCountTotalFilesActive(), 2);
                MemoryInFiles::deferred_write_t deferredWrite;
                deferredWrite.memoryBlockId = memoryBlockId;
                deferredWrite.offset = 0;
                deferredWrite.writeFromThisLocationPtr = dataSharedPtr->data();
                deferredWrite.length = dataSharedPtr->size();
                BOOST_REQUIRE(mf.WriteMemoryAsync(deferredWrite, boost::bind(&Test::WriteHandler, this, std::move(dataSharedPtr))));
                BOOST_REQUIRE_EQUAL(mf.GetCountTotalFilesCreated(), 2);
                BOOST_REQUIRE_EQUAL(mf.GetCountTotalFilesActive(), 2);
                BOOST_REQUIRE(!dataSharedPtr);
                BOOST_REQUIRE_EQUAL(ioService.run_one(), 1); //finish the write
                BOOST_REQUIRE_EQUAL(mf.GetCountTotalFilesCreated(), 2);
                BOOST_REQUIRE_EQUAL(mf.GetCountTotalFilesActive(), 2);
            }

            { //read back all of memoryBlockId 3 (from second file)
                const uint64_t memoryBlockId = 3;
                lastDataReadBackCallbackSharedPtr.reset();
                std::shared_ptr<std::string> dataReadBackSharedPtr = std::make_shared<std::string>("0000000");
                MemoryInFiles::deferred_read_t deferredRead;
                deferredRead.memoryBlockId = memoryBlockId;
                deferredRead.offset = 0;
                deferredRead.readToThisLocationPtr = dataReadBackSharedPtr->data();
                deferredRead.length = dataReadBackSharedPtr->size();
                BOOST_REQUIRE(mf.ReadMemoryAsync(deferredRead, boost::bind(&Test::ReadHandler,
                    this, boost::placeholders::_1, std::move(dataReadBackSharedPtr))));
                BOOST_REQUIRE(!dataReadBackSharedPtr);
                BOOST_REQUIRE_EQUAL(ioService.run_one(), 1);
                BOOST_REQUIRE(lastDataReadBackCallbackSharedPtr);
                BOOST_REQUIRE_EQUAL(*lastDataReadBackCallbackSharedPtr, "abcdefg");
                BOOST_REQUIRE_EQUAL(mf.GetCountTotalFilesCreated(), 2);
                BOOST_REQUIRE_EQUAL(mf.GetCountTotalFilesActive(), 2);
            }
            { //read "cd" from memoryBlockId=3 indices 2..3 AND read "78" from memoryBlockId=1 indices 7..8 AND read "01" from memoryBlockId=1 indices 0..1 
                lastMultiDataReadBackCallbackSharedPtr.reset();
                readMultiCount = 0;
                std::shared_ptr<std::vector<std::string> > multiDataReadBackSharedPtr = std::make_shared<std::vector<std::string> >(3);
                std::vector<MemoryInFiles::deferred_read_t> deferredReadsVec(3);
                { //read "cd" from memoryBlockId=3
                    std::string& s = (*multiDataReadBackSharedPtr)[0];
                    s.assign("aa");
                    MemoryInFiles::deferred_read_t& deferredRead = deferredReadsVec[0];
                    deferredRead.memoryBlockId = 3;
                    deferredRead.offset = 2;
                    deferredRead.readToThisLocationPtr = s.data();
                    deferredRead.length = s.size();
                }
                { //read "78" from memoryBlockId=1
                    std::string& s = (*multiDataReadBackSharedPtr)[1];
                    s.assign("bb");
                    MemoryInFiles::deferred_read_t& deferredRead = deferredReadsVec[1];
                    deferredRead.memoryBlockId = 1;
                    deferredRead.offset = 7;
                    deferredRead.readToThisLocationPtr = s.data();
                    deferredRead.length = s.size();
                }
                { //read "01" from memoryBlockId=1
                    std::string& s = (*multiDataReadBackSharedPtr)[2];
                    s.assign("hh");
                    MemoryInFiles::deferred_read_t& deferredRead = deferredReadsVec[2];
                    deferredRead.memoryBlockId = 1;
                    deferredRead.offset = 0;
                    deferredRead.readToThisLocationPtr = s.data();
                    deferredRead.length = s.size();
                }

                BOOST_REQUIRE(mf.ReadMemoryAsync(deferredReadsVec, boost::bind(&Test::ReadHandlerMulti,
                    this, boost::placeholders::_1, std::move(multiDataReadBackSharedPtr))));
                BOOST_REQUIRE(!multiDataReadBackSharedPtr);
                BOOST_REQUIRE_EQUAL(ioService.run_one(), 1);
                BOOST_REQUIRE_EQUAL(readMultiCount, 0);
                BOOST_REQUIRE_EQUAL(ioService.run_one(), 1);
                BOOST_REQUIRE_EQUAL(readMultiCount, 0); //called after next run_one
                BOOST_REQUIRE_EQUAL(ioService.run_one(), 1);
                BOOST_REQUIRE_EQUAL(readMultiCount, 1); //finished
                BOOST_REQUIRE(lastMultiDataReadBackCallbackSharedPtr);
                BOOST_REQUIRE_EQUAL((*lastMultiDataReadBackCallbackSharedPtr)[0], "cd");
                BOOST_REQUIRE_EQUAL((*lastMultiDataReadBackCallbackSharedPtr)[1], "78");
                BOOST_REQUIRE_EQUAL((*lastMultiDataReadBackCallbackSharedPtr)[2], "01");
                BOOST_REQUIRE_EQUAL(mf.GetCountTotalFilesCreated(), 2);
                BOOST_REQUIRE_EQUAL(mf.GetCountTotalFilesActive(), 2);
            }

            { 
                //resize memoryBlockId=1 in new file, (from size 7000 (rounds to 8192)) to size 10000 (rounds 10000 up to 4096*3=12288)
                //write memoryBlockId=1 "QWERTYUIOP" to indices 8188..8197 (spans two fragments in separate files)
                const uint64_t totalMemoryBlockSize = 10000;
                const uint64_t memoryBlockId = 1;
                const uint64_t newSize = mf.Resize(memoryBlockId, totalMemoryBlockSize);
                BOOST_REQUIRE_EQUAL(newSize, blockSize * 3);
                BOOST_REQUIRE_EQUAL(mf.GetSizeOfMemoryBlock(memoryBlockId), blockSize * 3); //rounds 10000 up to 12288
                std::shared_ptr<std::string> dataSharedPtr = std::make_shared<std::string>("QWERTYUIOP");
                BOOST_REQUIRE_EQUAL(dataSharedPtr->size(), 10);
                BOOST_REQUIRE_EQUAL(mf.GetCountTotalFilesCreated(), 2);
                BOOST_REQUIRE_EQUAL(mf.GetCountTotalFilesActive(), 2);
                MemoryInFiles::deferred_write_t deferredWrite;
                deferredWrite.memoryBlockId = memoryBlockId;
                deferredWrite.offset = 8188;
                deferredWrite.writeFromThisLocationPtr = dataSharedPtr->data();
                deferredWrite.length = dataSharedPtr->size();
                BOOST_REQUIRE(mf.WriteMemoryAsync(deferredWrite, boost::bind(&Test::WriteHandler, this, std::move(dataSharedPtr))));
                BOOST_REQUIRE_EQUAL(mf.GetCountTotalFilesCreated(), 2);
                BOOST_REQUIRE_EQUAL(mf.GetCountTotalFilesActive(), 2);
                BOOST_REQUIRE(!dataSharedPtr);
                writeSingleCount = 0;
                BOOST_REQUIRE_EQUAL(ioService.run_one(), 1); //spans two fragments, so call twice
                BOOST_REQUIRE_EQUAL(writeSingleCount, 0);
                BOOST_REQUIRE_EQUAL(ioService.run_one(), 1); //finish the write
                BOOST_REQUIRE_EQUAL(writeSingleCount, 1);
                BOOST_REQUIRE_EQUAL(mf.GetCountTotalFilesCreated(), 2);
                BOOST_REQUIRE_EQUAL(mf.GetCountTotalFilesActive(), 2);
            }
            { //read memoryBlockId=1 "QWERTYUIOP" from indices 8188..8197 (spans two fragments in separate files)
                const uint64_t memoryBlockId = 1;
                lastDataReadBackCallbackSharedPtr.reset();
                std::shared_ptr<std::string> dataReadBackSharedPtr = std::make_shared<std::string>("0123456789"); //overwrite this initial value
                MemoryInFiles::deferred_read_t deferredRead;
                deferredRead.memoryBlockId = memoryBlockId;
                deferredRead.offset = 8188;
                deferredRead.readToThisLocationPtr = dataReadBackSharedPtr->data();
                deferredRead.length = dataReadBackSharedPtr->size();
                BOOST_REQUIRE(mf.ReadMemoryAsync(deferredRead, boost::bind(&Test::ReadHandler,
                    this, boost::placeholders::_1, std::move(dataReadBackSharedPtr))));
                BOOST_REQUIRE(!dataReadBackSharedPtr);
                readSingleCount = 0;
                BOOST_REQUIRE_EQUAL(ioService.run_one(), 1); //two calls needed for the span of two fragments
                BOOST_REQUIRE(!lastDataReadBackCallbackSharedPtr);
                BOOST_REQUIRE_EQUAL(readSingleCount, 0);
                BOOST_REQUIRE_EQUAL(ioService.run_one(), 1);
                BOOST_REQUIRE(lastDataReadBackCallbackSharedPtr);
                BOOST_REQUIRE_EQUAL(readSingleCount, 1);
                BOOST_REQUIRE_EQUAL(*lastDataReadBackCallbackSharedPtr, "QWERTYUIOP");
                BOOST_REQUIRE_EQUAL(mf.GetCountTotalFilesCreated(), 2);
                BOOST_REQUIRE_EQUAL(mf.GetCountTotalFilesActive(), 2);
            }

            { 
                //read "cd" from memoryBlockId=3 indices 2..3
                // AND read "QWERTYUIOP" from memoryBlockId=1 indices 8188..8197 (spans two fragments in separate files)
                // AND read "qwerty" from memoryBlockId=1 indices 4094..4099 (spans two fragments)
                lastMultiDataReadBackCallbackSharedPtr.reset();
                readMultiCount = 0;
                std::shared_ptr<std::vector<std::string> > multiDataReadBackSharedPtr = std::make_shared<std::vector<std::string> >(3);
                std::vector<MemoryInFiles::deferred_read_t> deferredReadsVec(3);
                { //read "cd" from memoryBlockId=3
                    std::string& s = (*multiDataReadBackSharedPtr)[0];
                    s.assign("aa");
                    MemoryInFiles::deferred_read_t& deferredRead = deferredReadsVec[0];
                    deferredRead.memoryBlockId = 3;
                    deferredRead.offset = 2;
                    deferredRead.readToThisLocationPtr = s.data();
                    deferredRead.length = s.size();
                }
                { //read "QWERTYUIOP" from memoryBlockId=1
                    std::string& s = (*multiDataReadBackSharedPtr)[1];
                    s.assign("0123456789");
                    MemoryInFiles::deferred_read_t& deferredRead = deferredReadsVec[1];
                    deferredRead.memoryBlockId = 1;
                    deferredRead.offset = 8188;
                    deferredRead.readToThisLocationPtr = s.data();
                    deferredRead.length = s.size();
                }
                { //read "qwerty" from memoryBlockId=1
                    std::string& s = (*multiDataReadBackSharedPtr)[2];
                    s.assign("000000");
                    MemoryInFiles::deferred_read_t& deferredRead = deferredReadsVec[2];
                    deferredRead.memoryBlockId = 1;
                    deferredRead.offset = 4094;
                    deferredRead.readToThisLocationPtr = s.data();
                    deferredRead.length = s.size();
                }

                BOOST_REQUIRE(mf.ReadMemoryAsync(deferredReadsVec, boost::bind(&Test::ReadHandlerMulti,
                    this, boost::placeholders::_1, std::move(multiDataReadBackSharedPtr))));
                BOOST_REQUIRE(!multiDataReadBackSharedPtr);
                //5 runs to complete
                BOOST_REQUIRE_EQUAL(ioService.run_one(), 1);
                BOOST_REQUIRE_EQUAL(readMultiCount, 0);
                BOOST_REQUIRE_EQUAL(ioService.run_one(), 1);
                BOOST_REQUIRE_EQUAL(readMultiCount, 0);
                BOOST_REQUIRE_EQUAL(ioService.run_one(), 1);
                BOOST_REQUIRE_EQUAL(readMultiCount, 0);
                BOOST_REQUIRE_EQUAL(ioService.run_one(), 1);
                BOOST_REQUIRE_EQUAL(readMultiCount, 0); //called after next run_one
                BOOST_REQUIRE_EQUAL(ioService.run_one(), 1);
                BOOST_REQUIRE_EQUAL(readMultiCount, 1); //finished
                BOOST_REQUIRE(lastMultiDataReadBackCallbackSharedPtr);
                BOOST_REQUIRE_EQUAL((*lastMultiDataReadBackCallbackSharedPtr)[0], "cd");
                BOOST_REQUIRE_EQUAL((*lastMultiDataReadBackCallbackSharedPtr)[1], "QWERTYUIOP");
                BOOST_REQUIRE_EQUAL((*lastMultiDataReadBackCallbackSharedPtr)[2], "qwerty");
                BOOST_REQUIRE_EQUAL(mf.GetCountTotalFilesCreated(), 2);
                BOOST_REQUIRE_EQUAL(mf.GetCountTotalFilesActive(), 2);
            }

            { //expire the last 3 second timer, create memoryBlockId=4 in third file
                LOG_INFO(subprocess) << "waiting for 2 second timer to expire..";
                BOOST_REQUIRE_EQUAL(ioService.run_one(), 1);
                LOG_INFO(subprocess) << "expired";
                BOOST_REQUIRE_EQUAL(mf.GetCountTotalFilesCreated(), 2); //no allocation yet
                BOOST_REQUIRE_EQUAL(mf.GetCountTotalFilesActive(), 2);

                const uint64_t totalMemoryBlockSize = 7;
                const uint64_t memoryBlockId = mf.AllocateNewWriteMemoryBlock(totalMemoryBlockSize);
                BOOST_REQUIRE_EQUAL(memoryBlockId, 4);
                BOOST_REQUIRE_EQUAL(mf.GetCountTotalFilesCreated(), 3);
                BOOST_REQUIRE_EQUAL(mf.GetCountTotalFilesActive(), 3);
            }
            { //deallocate blocks
                BOOST_REQUIRE(!mf.DeleteMemoryBlock(10)); //doesn't exist
                //1 and 2 were in first file
                BOOST_REQUIRE(mf.DeleteMemoryBlock(1));
                BOOST_REQUIRE_EQUAL(mf.GetCountTotalFilesCreated(), 3); //still 3 files
                BOOST_REQUIRE_EQUAL(mf.GetCountTotalFilesActive(), 3);
                BOOST_REQUIRE(mf.DeleteMemoryBlock(2));
                BOOST_REQUIRE_EQUAL(mf.GetCountTotalFilesCreated(), 3); //first file deleted
                BOOST_REQUIRE_EQUAL(mf.GetCountTotalFilesActive(), 2);
                //4 was in third file (however that file is still active)
                BOOST_REQUIRE(mf.DeleteMemoryBlock(4));
                BOOST_REQUIRE_EQUAL(mf.GetCountTotalFilesCreated(), 3); //third file not deleted (because still active)
                BOOST_REQUIRE_EQUAL(mf.GetCountTotalFilesActive(), 2);
                //3 was in second file
                BOOST_REQUIRE(mf.DeleteMemoryBlock(3));
                BOOST_REQUIRE_EQUAL(mf.GetCountTotalFilesCreated(), 3); //second file deleted
                BOOST_REQUIRE_EQUAL(mf.GetCountTotalFilesActive(), 1);
                //expire the timer to delete the third file
                LOG_INFO(subprocess) << "waiting for last 2 second timer to expire..";
                BOOST_REQUIRE_EQUAL(ioService.run_one(), 1);
                LOG_INFO(subprocess) << "expired";
                BOOST_REQUIRE_EQUAL(mf.GetCountTotalFilesCreated(), 3); //third file deleted on timer expiration
                BOOST_REQUIRE_EQUAL(mf.GetCountTotalFilesActive(), 0);
            }

        }

        void ReadHandler(bool success, std::shared_ptr<std::string>& dataSharedPtr) {
            BOOST_REQUIRE(dataSharedPtr);
            ++readSingleCount;
            lastDataReadBackCallbackSharedPtr = std::move(dataSharedPtr);
        }
        void ReadHandlerMulti(bool success, std::shared_ptr<std::vector<std::string> >& dataSharedPtr) {
            ++readMultiCount;
            BOOST_REQUIRE_EQUAL(readMultiCount, 1); //must only get called once
            BOOST_REQUIRE(dataSharedPtr);
            lastMultiDataReadBackCallbackSharedPtr = std::move(dataSharedPtr);
        }
        void WriteHandler(std::shared_ptr<std::string> & dataSharedPtr) {
            ++writeSingleCount;
            BOOST_REQUIRE(dataSharedPtr);
        }
        void WriteHandlerMulti(std::shared_ptr<std::vector<std::string> >& dataSharedPtr) {
            ++writeMultiCount;
            BOOST_REQUIRE_EQUAL(writeMultiCount, 1); //must only get called once
            BOOST_REQUIRE(dataSharedPtr);
        }
    };
    
    const fs::path rootPath = fs::temp_directory_path() / "MemoryInFilesTest";
    if (boost::filesystem::is_directory(rootPath)) {
        fs::remove_all(rootPath);
    }
    BOOST_REQUIRE(boost::filesystem::create_directory(rootPath));
    LOG_INFO(subprocess) << "running MemoryInFilesTestCase with rootpath=" << rootPath;
        
    Test t(rootPath);
    t.DoTest();
    LOG_INFO(subprocess) << "finished MemoryInFilesTestCase";
}








BOOST_AUTO_TEST_CASE(MemoryInFilesSpeedTestCase)
{
    namespace fs = boost::filesystem;
    static constexpr uint64_t NUM_MEMORY_BLOCKS_TO_ALLOCATE = 1000;

    struct Test {
        boost::asio::io_service ioService;
        std::unique_ptr<boost::asio::io_service::work> work =
            boost::make_unique<boost::asio::io_service::work>(ioService); //prevent having to call ioService.reset()
        const fs::path rootPath;
        const uint64_t newFileAggregationTimeMs;
        MemoryInFiles mf;
        std::queue<std::vector<uint64_t> > expectedBlockReadsQueue;
        uint64_t expectedUseCountWrite;
        uint64_t expectedUseCountRead;

        uint64_t writeUseCountCountsAt2;
        uint64_t writeUseCountCountsAt1;
        uint64_t readUseCountCountsAt2;
        uint64_t readUseCountCountsAt1;
        Test(const fs::path& paramRootPath) :
            rootPath(paramRootPath),
            newFileAggregationTimeMs(2000),
            mf(ioService, rootPath, newFileAggregationTimeMs, 10) {}

        void DoTest() {
            static constexpr uint64_t BLOCK_NUM_VALUES = 10;
            static constexpr uint64_t BLOCK_NUM_BYTES = BLOCK_NUM_VALUES * sizeof(uint64_t);
            static constexpr uint64_t BLOCK_HALF_NUM_BYTES = BLOCK_NUM_BYTES / 2;
            expectedUseCountWrite = 2;
            expectedUseCountRead = 2;

            writeUseCountCountsAt2 = 0;
            writeUseCountCountsAt1 = 0;
            readUseCountCountsAt2 = 0;
            readUseCountCountsAt1 = 0;

            uint64_t valueCounter = 0;
            uint64_t lastMemoryBlockNeedingDeleted = 1;
            uint64_t numDeleted = 0;
            for (uint64_t memoryBlockId = 1; memoryBlockId <= NUM_MEMORY_BLOCKS_TO_ALLOCATE; ++memoryBlockId) {
                std::vector<uint64_t> expectedVec(BLOCK_NUM_VALUES);
                for (std::size_t i = 0; i < expectedVec.size(); ++i) {
                    expectedVec[i] = ++valueCounter;
                }
                std::shared_ptr<std::vector<uint64_t> > dataToWriteSharedPtr = std::make_shared<std::vector<uint64_t> >(expectedVec);
                expectedBlockReadsQueue.emplace(std::move(expectedVec));
                std::shared_ptr<std::vector<uint64_t> > dataToReadBackSharedPtr = std::make_shared<std::vector<uint64_t> >(BLOCK_NUM_VALUES, 0);
                BOOST_REQUIRE_EQUAL(mf.AllocateNewWriteMemoryBlock(BLOCK_NUM_BYTES), memoryBlockId);
                { //write first half of bytes
                    MemoryInFiles::deferred_write_t deferredWrite;
                    deferredWrite.memoryBlockId = memoryBlockId;
                    deferredWrite.offset = 0;
                    deferredWrite.writeFromThisLocationPtr = dataToWriteSharedPtr->data();
                    deferredWrite.length = BLOCK_HALF_NUM_BYTES;
                    BOOST_REQUIRE(mf.WriteMemoryAsync(deferredWrite, boost::bind(&Test::WriteHandler, this, dataToWriteSharedPtr)));
                }
                { //write second half of bytes
                    MemoryInFiles::deferred_write_t deferredWrite;
                    deferredWrite.memoryBlockId = memoryBlockId;
                    deferredWrite.offset = BLOCK_HALF_NUM_BYTES;
                    deferredWrite.writeFromThisLocationPtr = ((const uint8_t*)(dataToWriteSharedPtr->data())) + BLOCK_HALF_NUM_BYTES;
                    deferredWrite.length = BLOCK_HALF_NUM_BYTES;
                    BOOST_REQUIRE(mf.WriteMemoryAsync(deferredWrite, boost::bind(&Test::WriteHandler, this, std::move(dataToWriteSharedPtr))));
                }
                { //read first half of bytes
                    MemoryInFiles::deferred_read_t deferredRead;
                    deferredRead.memoryBlockId = memoryBlockId;
                    deferredRead.offset = 0;
                    deferredRead.readToThisLocationPtr = dataToReadBackSharedPtr->data();
                    deferredRead.length = BLOCK_HALF_NUM_BYTES;
                    BOOST_REQUIRE(mf.ReadMemoryAsync(deferredRead, boost::bind(&Test::ReadHandler,
                        this, boost::placeholders::_1, dataToReadBackSharedPtr)));
                }
                { //read second half of bytes
                    MemoryInFiles::deferred_read_t deferredRead;
                    deferredRead.memoryBlockId = memoryBlockId;
                    deferredRead.offset = BLOCK_HALF_NUM_BYTES;
                    deferredRead.readToThisLocationPtr = ((uint8_t*)(dataToReadBackSharedPtr->data())) + BLOCK_HALF_NUM_BYTES;
                    deferredRead.length = BLOCK_HALF_NUM_BYTES;
                    BOOST_REQUIRE(mf.ReadMemoryAsync(deferredRead, boost::bind(&Test::ReadHandler,
                        this, boost::placeholders::_1, std::move(dataToReadBackSharedPtr))));
                }
                for (unsigned int r = 0; r < 4; ++r) {
                    ioService.run_one();
                }
                if (memoryBlockId == (NUM_MEMORY_BLOCKS_TO_ALLOCATE / 2)) {
                    LOG_INFO(subprocess) << "half way.. waiting for 2 second timer to expire..";
                    BOOST_REQUIRE_EQUAL(ioService.run_one(), 1);
                    LOG_INFO(subprocess) << "expired";
                }
                if (memoryBlockId % 5 == 0) {
                    for (; lastMemoryBlockNeedingDeleted <= memoryBlockId; ++lastMemoryBlockNeedingDeleted) {
                        BOOST_REQUIRE(mf.DeleteMemoryBlock(lastMemoryBlockNeedingDeleted));
                        ++numDeleted;
                    }
                    //LOG_DEBUG(subprocess) << "next needing deleted=" << lastMemoryBlockNeedingDeleted;
                }
            }
            for (; lastMemoryBlockNeedingDeleted <= NUM_MEMORY_BLOCKS_TO_ALLOCATE; ++lastMemoryBlockNeedingDeleted) {
                BOOST_REQUIRE(mf.DeleteMemoryBlock(lastMemoryBlockNeedingDeleted));
                ++numDeleted;
                //LOG_DEBUG(subprocess) << "finish delete=" << lastMemoryBlockNeedingDeleted;
            }
            BOOST_REQUIRE_EQUAL(numDeleted, NUM_MEMORY_BLOCKS_TO_ALLOCATE);
        }

        void ReadHandler(bool success, std::shared_ptr<std::vector<uint64_t> >& dataSharedPtr) {
            BOOST_REQUIRE_EQUAL(dataSharedPtr.use_count(), expectedUseCountRead);
            if (expectedUseCountRead == 2) {
                //LOG_DEBUG(subprocess) << "u2";
                expectedUseCountRead = 1; //next will decrement
                ++readUseCountCountsAt2;
            }
            else { //use count was 1 (read complete)
                std::vector<uint64_t>& vec = *dataSharedPtr;
                BOOST_REQUIRE(vec == expectedBlockReadsQueue.front());
                //LOG_DEBUG(subprocess) << "v " << expectedBlockReadsQueue.front()[0] << " " << expectedBlockReadsQueue.front()[10 - 1];
                expectedBlockReadsQueue.pop();
                expectedUseCountRead = 2;
                ++readUseCountCountsAt1;
            }
        }
        void WriteHandler(std::shared_ptr<std::vector<uint64_t> >& dataSharedPtr) {
            BOOST_REQUIRE_EQUAL(dataSharedPtr.use_count(), expectedUseCountWrite);
            if (expectedUseCountWrite == 2) {
                expectedUseCountWrite = 1; //next will decrement
                ++writeUseCountCountsAt2;
            }
            else { //use count was 1
                expectedUseCountWrite = 2;
                ++writeUseCountCountsAt1;
            }
        }
    };

    const fs::path rootPath = fs::temp_directory_path() / "MemoryInFilesSpeedTestCase";
    if (boost::filesystem::is_directory(rootPath)) {
        fs::remove_all(rootPath);
    }
    BOOST_REQUIRE(boost::filesystem::create_directory(rootPath));
    LOG_INFO(subprocess) << "running MemoryInFilesSpeedTestCase with rootpath=" << rootPath;

    Test t(rootPath);
    t.DoTest();
    BOOST_REQUIRE_EQUAL(t.writeUseCountCountsAt2, NUM_MEMORY_BLOCKS_TO_ALLOCATE);
    BOOST_REQUIRE_EQUAL(t.writeUseCountCountsAt1, NUM_MEMORY_BLOCKS_TO_ALLOCATE);
    BOOST_REQUIRE_EQUAL(t.readUseCountCountsAt2, NUM_MEMORY_BLOCKS_TO_ALLOCATE);
    BOOST_REQUIRE_EQUAL(t.readUseCountCountsAt1, NUM_MEMORY_BLOCKS_TO_ALLOCATE);
    LOG_INFO(subprocess) << "finished MemoryInFilesSpeedTestCase";
}

BOOST_AUTO_TEST_CASE(MemoryInFilesSpeedTestAllWritesFirstCase)
{
    namespace fs = boost::filesystem;
    static constexpr uint64_t NUM_MEMORY_BLOCKS_TO_ALLOCATE = 1000;

    struct Test {
        boost::asio::io_service ioService;
        std::unique_ptr<boost::asio::io_service::work> work =
            boost::make_unique<boost::asio::io_service::work>(ioService); //prevent having to call ioService.reset()
        const fs::path rootPath;
        const uint64_t newFileAggregationTimeMs;
        MemoryInFiles mf;
        std::queue<std::vector<uint64_t> > expectedBlockReadsQueue;
        uint64_t expectedUseCountWrite;
        uint64_t expectedUseCountRead;

        uint64_t writeUseCountCountsAt2;
        uint64_t writeUseCountCountsAt1;
        uint64_t readUseCountCountsAt2;
        uint64_t readUseCountCountsAt1;
        Test(const fs::path& paramRootPath) :
            rootPath(paramRootPath),
            newFileAggregationTimeMs(2000),
            mf(ioService, rootPath, newFileAggregationTimeMs, 10) {}

        void DoTest() {
            static constexpr uint64_t BLOCK_NUM_VALUES = 10;
            static constexpr uint64_t BLOCK_NUM_BYTES = BLOCK_NUM_VALUES * sizeof(uint64_t);
            static constexpr uint64_t BLOCK_HALF_NUM_BYTES = BLOCK_NUM_BYTES / 2;
            expectedUseCountWrite = 2;
            expectedUseCountRead = 2;

            writeUseCountCountsAt2 = 0;
            writeUseCountCountsAt1 = 0;
            readUseCountCountsAt2 = 0;
            readUseCountCountsAt1 = 0;

            uint64_t valueCounter = 0;
            uint64_t lastMemoryBlockNeedingDeleted = 1;
            uint64_t numDeleted = 0;
            for (uint64_t memoryBlockId = 1; memoryBlockId <= NUM_MEMORY_BLOCKS_TO_ALLOCATE; ++memoryBlockId) {
                std::vector<uint64_t> expectedVec(BLOCK_NUM_VALUES);
                for (std::size_t i = 0; i < expectedVec.size(); ++i) {
                    expectedVec[i] = ++valueCounter;
                }
                std::shared_ptr<std::vector<uint64_t> > dataToWriteSharedPtr = std::make_shared<std::vector<uint64_t> >(expectedVec);
                expectedBlockReadsQueue.emplace(std::move(expectedVec));
                
                BOOST_REQUIRE_EQUAL(mf.AllocateNewWriteMemoryBlock(BLOCK_NUM_BYTES), memoryBlockId);
                { //write first half of bytes
                    MemoryInFiles::deferred_write_t deferredWrite;
                    deferredWrite.memoryBlockId = memoryBlockId;
                    deferredWrite.offset = 0;
                    deferredWrite.writeFromThisLocationPtr = dataToWriteSharedPtr->data();
                    deferredWrite.length = BLOCK_HALF_NUM_BYTES;
                    BOOST_REQUIRE(mf.WriteMemoryAsync(deferredWrite, boost::bind(&Test::WriteHandler, this, dataToWriteSharedPtr)));
                }
                { //write second half of bytes
                    MemoryInFiles::deferred_write_t deferredWrite;
                    deferredWrite.memoryBlockId = memoryBlockId;
                    deferredWrite.offset = BLOCK_HALF_NUM_BYTES;
                    deferredWrite.writeFromThisLocationPtr = ((const uint8_t*)(dataToWriteSharedPtr->data())) + BLOCK_HALF_NUM_BYTES;
                    deferredWrite.length = BLOCK_HALF_NUM_BYTES;
                    BOOST_REQUIRE(mf.WriteMemoryAsync(deferredWrite, boost::bind(&Test::WriteHandler, this, std::move(dataToWriteSharedPtr))));
                }
                for (unsigned int r = 0; r < 2; ++r) {
                    ioService.run_one();
                }
                if (memoryBlockId == (NUM_MEMORY_BLOCKS_TO_ALLOCATE / 2)) {
                    LOG_INFO(subprocess) << "half way writing.. waiting for 2 second timer to expire..";
                    BOOST_REQUIRE_EQUAL(ioService.run_one(), 1);
                    LOG_INFO(subprocess) << "expired";
                }
            }
            for (uint64_t memoryBlockId = 1; memoryBlockId <= NUM_MEMORY_BLOCKS_TO_ALLOCATE; ++memoryBlockId) {
                std::shared_ptr<std::vector<uint64_t> > dataToReadBackSharedPtr = std::make_shared<std::vector<uint64_t> >(BLOCK_NUM_VALUES, 0);
                { //read first half of bytes
                    MemoryInFiles::deferred_read_t deferredRead;
                    deferredRead.memoryBlockId = memoryBlockId;
                    deferredRead.offset = 0;
                    deferredRead.readToThisLocationPtr = dataToReadBackSharedPtr->data();
                    deferredRead.length = BLOCK_HALF_NUM_BYTES;
                    BOOST_REQUIRE(mf.ReadMemoryAsync(deferredRead, boost::bind(&Test::ReadHandler,
                        this, boost::placeholders::_1, dataToReadBackSharedPtr)));
                }
                { //read second half of bytes
                    MemoryInFiles::deferred_read_t deferredRead;
                    deferredRead.memoryBlockId = memoryBlockId;
                    deferredRead.offset = BLOCK_HALF_NUM_BYTES;
                    deferredRead.readToThisLocationPtr = ((uint8_t*)(dataToReadBackSharedPtr->data())) + BLOCK_HALF_NUM_BYTES;
                    deferredRead.length = BLOCK_HALF_NUM_BYTES;
                    BOOST_REQUIRE(mf.ReadMemoryAsync(deferredRead, boost::bind(&Test::ReadHandler,
                        this, boost::placeholders::_1, std::move(dataToReadBackSharedPtr))));
                }
                for (unsigned int r = 0; r < 2; ++r) {
                    ioService.run_one();
                }
                if (memoryBlockId % 5 == 0) {
                    for (; lastMemoryBlockNeedingDeleted <= memoryBlockId; ++lastMemoryBlockNeedingDeleted) {
                        BOOST_REQUIRE(mf.DeleteMemoryBlock(lastMemoryBlockNeedingDeleted));
                        ++numDeleted;
                    }
                    //LOG_DEBUG(subprocess) << "next needing deleted=" << lastMemoryBlockNeedingDeleted;
                }
            }
            for (; lastMemoryBlockNeedingDeleted <= NUM_MEMORY_BLOCKS_TO_ALLOCATE; ++lastMemoryBlockNeedingDeleted) {
                BOOST_REQUIRE(mf.DeleteMemoryBlock(lastMemoryBlockNeedingDeleted));
                ++numDeleted;
                //LOG_DEBUG(subprocess) << "finish delete=" << lastMemoryBlockNeedingDeleted;
            }
            BOOST_REQUIRE_EQUAL(numDeleted, NUM_MEMORY_BLOCKS_TO_ALLOCATE);
        }

        void ReadHandler(bool success, std::shared_ptr<std::vector<uint64_t> >& dataSharedPtr) {
            BOOST_REQUIRE_EQUAL(dataSharedPtr.use_count(), expectedUseCountRead);
            if (expectedUseCountRead == 2) {
                //LOG_DEBUG(subprocess) << "u2";
                expectedUseCountRead = 1; //next will decrement
                ++readUseCountCountsAt2;
            }
            else { //use count was 1 (read complete)
                std::vector<uint64_t>& vec = *dataSharedPtr;
                BOOST_REQUIRE(vec == expectedBlockReadsQueue.front());
                //LOG_DEBUG(subprocess) << "v " << expectedBlockReadsQueue.front()[0] << " " << expectedBlockReadsQueue.front()[10 - 1];
                expectedBlockReadsQueue.pop();
                expectedUseCountRead = 2;
                ++readUseCountCountsAt1;
            }
        }
        void WriteHandler(std::shared_ptr<std::vector<uint64_t> >& dataSharedPtr) {
            BOOST_REQUIRE_EQUAL(dataSharedPtr.use_count(), expectedUseCountWrite);
            if (expectedUseCountWrite == 2) {
                expectedUseCountWrite = 1; //next will decrement
                ++writeUseCountCountsAt2;
            }
            else { //use count was 1
                expectedUseCountWrite = 2;
                ++writeUseCountCountsAt1;
            }
        }
    };

    const fs::path rootPath = fs::temp_directory_path() / "MemoryInFilesSpeedTestAllWritesFirstCase";
    if (boost::filesystem::is_directory(rootPath)) {
        fs::remove_all(rootPath);
    }
    BOOST_REQUIRE(boost::filesystem::create_directory(rootPath));
    LOG_INFO(subprocess) << "running MemoryInFilesSpeedTestAllWritesFirstCase with rootpath=" << rootPath;

    Test t(rootPath);
    t.DoTest();
    BOOST_REQUIRE_EQUAL(t.writeUseCountCountsAt2, NUM_MEMORY_BLOCKS_TO_ALLOCATE);
    BOOST_REQUIRE_EQUAL(t.writeUseCountCountsAt1, NUM_MEMORY_BLOCKS_TO_ALLOCATE);
    BOOST_REQUIRE_EQUAL(t.readUseCountCountsAt2, NUM_MEMORY_BLOCKS_TO_ALLOCATE);
    BOOST_REQUIRE_EQUAL(t.readUseCountCountsAt1, NUM_MEMORY_BLOCKS_TO_ALLOCATE);
    LOG_INFO(subprocess) << "finished MemoryInFilesSpeedTestAllWritesFirstCase";
}

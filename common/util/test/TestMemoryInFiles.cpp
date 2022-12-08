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
        Test(const fs::path & paramRootPath) :
            rootPath(paramRootPath),
            newFileAggregationTimeMs(2000),
            mf(ioService, rootPath, newFileAggregationTimeMs) {}

        void DoTest() {
            uint64_t memoryBlockId;
            {
                const std::size_t totalMemoryBlockSize = 10;
                BOOST_REQUIRE_EQUAL(mf.GetCountTotalFilesCreated(), 0);
                BOOST_REQUIRE_EQUAL(mf.GetCountTotalFilesActive(), 0);
                memoryBlockId = mf.AllocateNewWriteMemoryBlock(totalMemoryBlockSize);
                BOOST_REQUIRE_EQUAL(memoryBlockId, 1);
                std::shared_ptr<std::string> dataSharedPtr = std::make_shared<std::string>("56789");
                BOOST_REQUIRE_EQUAL(dataSharedPtr->size(), 5);
                const char* data = dataSharedPtr->data();
                const std::size_t sizeData = dataSharedPtr->size();
                BOOST_REQUIRE_EQUAL(mf.GetCountTotalFilesCreated(), 1);
                BOOST_REQUIRE_EQUAL(mf.GetCountTotalFilesActive(), 1);
                BOOST_REQUIRE(mf.WriteMemoryAsync(memoryBlockId, 5, data, sizeData, boost::bind(&Test::WriteHandler, this, std::move(dataSharedPtr))));
                BOOST_REQUIRE_EQUAL(mf.GetCountTotalFilesCreated(), 1);
                BOOST_REQUIRE_EQUAL(mf.GetCountTotalFilesActive(), 1);
                BOOST_REQUIRE(!dataSharedPtr);
                BOOST_REQUIRE_EQUAL(ioService.run_one(), 1); //finish the write
                BOOST_REQUIRE_EQUAL(mf.GetCountTotalFilesCreated(), 1);
                BOOST_REQUIRE_EQUAL(mf.GetCountTotalFilesActive(), 1);
            }
            { //read only "678"
                lastDataReadBackCallbackSharedPtr.reset();
                std::shared_ptr<std::string> dataReadBackSharedPtr = std::make_shared<std::string>("aaa");
                char* data = dataReadBackSharedPtr->data();
                const std::size_t sizeData = dataReadBackSharedPtr->size();
                BOOST_REQUIRE(mf.ReadMemoryAsync(memoryBlockId, 6, data, sizeData, boost::bind(&Test::ReadHandler,
                    this, boost::placeholders::_1, std::move(dataReadBackSharedPtr))));
                BOOST_REQUIRE(!dataReadBackSharedPtr);
                BOOST_REQUIRE_EQUAL(ioService.run_one(), 1);
                BOOST_REQUIRE(lastDataReadBackCallbackSharedPtr);
                BOOST_REQUIRE_EQUAL(*lastDataReadBackCallbackSharedPtr, "678");
                BOOST_REQUIRE_EQUAL(mf.GetCountTotalFilesCreated(), 1);
                BOOST_REQUIRE_EQUAL(mf.GetCountTotalFilesActive(), 1);
            }
            { //read fail (out of bounds)
                lastDataReadBackCallbackSharedPtr.reset();
                std::shared_ptr<std::string> dataReadBackSharedPtr = std::make_shared<std::string>("aaa");
                char* data = dataReadBackSharedPtr->data();
                const std::size_t sizeData = dataReadBackSharedPtr->size();
                BOOST_REQUIRE(!mf.ReadMemoryAsync(memoryBlockId, 8, data, sizeData, boost::bind(&Test::ReadHandler,
                    this, boost::placeholders::_1, std::move(dataReadBackSharedPtr))));
                BOOST_REQUIRE_EQUAL(mf.GetCountTotalFilesCreated(), 1);
                BOOST_REQUIRE_EQUAL(mf.GetCountTotalFilesActive(), 1);
            }
            { //write fail (out of bounds)
                const std::size_t totalMemoryBlockSize = 10;
                memoryBlockId = mf.AllocateNewWriteMemoryBlock(totalMemoryBlockSize);
                BOOST_REQUIRE_EQUAL(memoryBlockId, 2);
                std::shared_ptr<std::string> dataSharedPtr = std::make_shared<std::string>("56789");
                BOOST_REQUIRE_EQUAL(dataSharedPtr->size(), 5);
                const char* data = dataSharedPtr->data();
                const std::size_t sizeData = dataSharedPtr->size();
                BOOST_REQUIRE(!mf.WriteMemoryAsync(memoryBlockId, 6, data, sizeData, boost::bind(&Test::WriteHandler, this, std::move(dataSharedPtr))));
                BOOST_REQUIRE_EQUAL(mf.GetCountTotalFilesCreated(), 1); //still under the 3 second timeout
                BOOST_REQUIRE_EQUAL(mf.GetCountTotalFilesActive(), 1);
            }
            { //expire the 3 second timer, create new block in second file
                LOG_INFO(subprocess) << "waiting for 2 second timer to expire..";
                BOOST_REQUIRE_EQUAL(ioService.run_one(), 1);
                LOG_INFO(subprocess) << "expired";
                BOOST_REQUIRE_EQUAL(mf.GetCountTotalFilesCreated(), 1); //no allocation yet
                BOOST_REQUIRE_EQUAL(mf.GetCountTotalFilesActive(), 1);

                const std::size_t totalMemoryBlockSize = 7;
                memoryBlockId = mf.AllocateNewWriteMemoryBlock(totalMemoryBlockSize);
                BOOST_REQUIRE_EQUAL(memoryBlockId, 3);
                std::shared_ptr<std::string> dataSharedPtr = std::make_shared<std::string>("abcdefg");
                BOOST_REQUIRE_EQUAL(dataSharedPtr->size(), totalMemoryBlockSize);
                const char* data = dataSharedPtr->data();
                BOOST_REQUIRE_EQUAL(mf.GetCountTotalFilesCreated(), 2);
                BOOST_REQUIRE_EQUAL(mf.GetCountTotalFilesActive(), 2);
                BOOST_REQUIRE(mf.WriteMemoryAsync(memoryBlockId, 0, data, totalMemoryBlockSize, boost::bind(&Test::WriteHandler, this, std::move(dataSharedPtr))));
                BOOST_REQUIRE_EQUAL(mf.GetCountTotalFilesCreated(), 2);
                BOOST_REQUIRE_EQUAL(mf.GetCountTotalFilesActive(), 2);
                BOOST_REQUIRE(!dataSharedPtr);
                BOOST_REQUIRE_EQUAL(ioService.run_one(), 1); //finish the write
                BOOST_REQUIRE_EQUAL(mf.GetCountTotalFilesCreated(), 2);
                BOOST_REQUIRE_EQUAL(mf.GetCountTotalFilesActive(), 2);
            }
            { //read back all of memoryBlockId 3
                lastDataReadBackCallbackSharedPtr.reset();
                std::shared_ptr<std::string> dataReadBackSharedPtr = std::make_shared<std::string>("aaaaaaa");
                char* data = dataReadBackSharedPtr->data();
                const std::size_t sizeData = dataReadBackSharedPtr->size();
                BOOST_REQUIRE(mf.ReadMemoryAsync(memoryBlockId, 0, data, sizeData, boost::bind(&Test::ReadHandler,
                    this, boost::placeholders::_1, std::move(dataReadBackSharedPtr))));
                BOOST_REQUIRE(!dataReadBackSharedPtr);
                BOOST_REQUIRE_EQUAL(ioService.run_one(), 1);
                BOOST_REQUIRE(lastDataReadBackCallbackSharedPtr);
                BOOST_REQUIRE_EQUAL(*lastDataReadBackCallbackSharedPtr, "abcdefg");
                BOOST_REQUIRE_EQUAL(mf.GetCountTotalFilesCreated(), 2);
                BOOST_REQUIRE_EQUAL(mf.GetCountTotalFilesActive(), 2);
            }
            { //expire the last 3 second timer, create new block in third file
                LOG_INFO(subprocess) << "waiting for 2 second timer to expire..";
                BOOST_REQUIRE_EQUAL(ioService.run_one(), 1);
                LOG_INFO(subprocess) << "expired";
                BOOST_REQUIRE_EQUAL(mf.GetCountTotalFilesCreated(), 2); //no allocation yet
                BOOST_REQUIRE_EQUAL(mf.GetCountTotalFilesActive(), 2);

                const std::size_t totalMemoryBlockSize = 7;
                memoryBlockId = mf.AllocateNewWriteMemoryBlock(totalMemoryBlockSize);
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
            lastDataReadBackCallbackSharedPtr = std::move(dataSharedPtr);
        }
        void WriteHandler(std::shared_ptr<std::string> & dataSharedPtr) {
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

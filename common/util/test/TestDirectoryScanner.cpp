/**
 * @file TestDirectoryScanner.cpp
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
#include <boost/thread.hpp>
#include <iostream>
#include <boost/filesystem/fstream.hpp>
#include "DirectoryScanner.h"

BOOST_AUTO_TEST_CASE(DirectoryScannerTestCase)
{
    boost::asio::io_service ioService;
    static constexpr uint64_t recheckFileSizeDurationMilliseconds = 250;
    {
        namespace fs = boost::filesystem;
        const boost::filesystem::path rootPath = fs::temp_directory_path() / "DirectoryScannerTest";
        const boost::filesystem::path& rp = rootPath;
        if (boost::filesystem::is_directory(rootPath)) {
            fs::remove_all(rootPath);
        }
        std::cout << "running DirectoryScannerTestCase with rootpath=" << rootPath << "\n";
#if 0 //classic ascii
        const boost::filesystem::path d4Dir = "e";
        const boost::filesystem::path d4Dir2 = "f";
        const boost::filesystem::path d4File = "d4_file1.txt";
        const boost::filesystem::path d4FileNew = "d4e_filenew.txt";
#else //UTF-8 (Hebrew characters)
        std::string shalomTxtUtf8Str({ '\xd7', '\xa9', '\xd7', '\x9c', '\xd7', '\x95', '\xd7', '\x9d', '.', 't', 'x', 't' });
        std::string shalomDatUtf8Str({ '\xd7', '\xa9', '\xd7', '\x9c', '\xd7', '\x95', '\xd7', '\x9d', '.', 'd', 'a', 't' });
        std::string shinUtf8DirStr({ '\xd7', '\xa9' });
        std::string lamedUtf8DirStr({ '\xd7', '\x9c' });
        boost::filesystem::path shalomTxtPath(Utf8Paths::Utf8StringToPath(shalomTxtUtf8Str));
        boost::filesystem::path shalomDatPath(Utf8Paths::Utf8StringToPath(shalomDatUtf8Str));
        boost::filesystem::path shinDirPath(Utf8Paths::Utf8StringToPath(shinUtf8DirStr));
        boost::filesystem::path lamedDirPath(Utf8Paths::Utf8StringToPath(lamedUtf8DirStr));

        const boost::filesystem::path& d4Dir = shinDirPath;
        const boost::filesystem::path& d4Dir2 = lamedDirPath;
        const boost::filesystem::path& d4File = shalomTxtPath;
        const boost::filesystem::path& d4FileNew = shalomDatPath;
#endif
        fs::create_directories(rootPath / "a/b/c");
        fs::create_directories(rootPath / "a/b/d" / d4Dir);
        

        boost::filesystem::ofstream(rootPath / "d0_file1.txt");
        boost::filesystem::ofstream(rootPath / "a/d1_file1.txt");
        boost::filesystem::ofstream(rootPath / "a/b/d2_file1.txt");
        boost::filesystem::ofstream(rootPath / "a/b/c/d3_file1.txt");
        boost::filesystem::ofstream(rootPath / "a/b/d/d3_file1.txt");
        
        boost::filesystem::ofstream(rootPath / "a/b/d" / d4Dir / d4File);
        const DirectoryScanner::path_list_t depthIndexToExpectedAbsoluteList[5] = {
            { rp / "d0_file1.txt" }, //depth 0

            { rp / "a/d1_file1.txt", rp / "d0_file1.txt" }, //depth 1

            { rp / "a/b/d2_file1.txt", rp / "a/d1_file1.txt", rp / "d0_file1.txt" }, //depth 2

            { rp / "a/b/c/d3_file1.txt", rp / "a/b/d/d3_file1.txt", rp / "a/b/d2_file1.txt",
              rp / "a/d1_file1.txt", rp / "d0_file1.txt" },//depth 3
            
            { rp / "a/b/c/d3_file1.txt", rp / "a/b/d/d3_file1.txt", rp / "a/b/d" / d4Dir / d4File,
              rp / "a/b/d2_file1.txt", rp / "a/d1_file1.txt", rp / "d0_file1.txt" }, //depth 4
        };
        const DirectoryScanner::path_list_t depthIndexToExpectedRelativeList[5] = {
            { fs::path("d0_file1.txt") }, //depth 0

            { fs::path("a/d1_file1.txt"), fs::path("d0_file1.txt") }, //depth 1

            { fs::path("a/b/d2_file1.txt"), fs::path("a/d1_file1.txt"), fs::path("d0_file1.txt") }, //depth 2

            { fs::path("a/b/c/d3_file1.txt"), fs::path("a/b/d/d3_file1.txt"), fs::path("a/b/d2_file1.txt"),
              fs::path("a/d1_file1.txt"), fs::path("d0_file1.txt") },//depth 3

            { fs::path("a/b/c/d3_file1.txt"), fs::path("a/b/d/d3_file1.txt"), fs::path("a/b/d" / d4Dir / d4File),
              fs::path("a/b/d2_file1.txt"), fs::path("a/d1_file1.txt"), fs::path("d0_file1.txt") }, //depth 4
        };

        const DirectoryScanner::path_set_t depthIndexToExpectedAbsoluteDirectoriesMonitoredSet[5] = {
            { rp }, //depth 0
            { rp, rp / "a" }, //depth 1
            { rp, rp / "a", rp / "a/b" }, //depth 2
            { rp, rp / "a", rp / "a/b", rp / "a/b/c", rp / "a/b/d" }, //depth 3
            { rp, rp / "a", rp / "a/b", rp / "a/b/c", rp / "a/b/d", rp / "a/b/d" / d4Dir }, //depth 4
        };
        const fs::path aRp("a");
        const DirectoryScanner::path_set_t depthIndexToExpectedRelativeDirectoriesMonitoredSet[5] = {
            { fs::path(".") }, //depth 0
            { fs::path("."), aRp }, //depth 1
            { fs::path("."), aRp, aRp / fs::path("b") }, //depth 2
            { fs::path("."), aRp, aRp / fs::path("b"), aRp / fs::path("b/c"), aRp / fs::path("b/d") }, //depth 3
            { fs::path("."), aRp, aRp / fs::path("b"), aRp / fs::path("b/c"), aRp / fs::path("b/d"), aRp / fs::path("b/d" / d4Dir) }, //depth 4
        };
        //std::cout << "here " << (fs::path("a") / fs::path("b")) << "\n";
        
        bool includeExistingFiles = true;
        bool includeNewFiles = false;
        for (unsigned int recurseDirectoriesDepth = 0; recurseDirectoriesDepth <= 4; ++recurseDirectoriesDepth) {
            {
                DirectoryScanner ds(rootPath, includeExistingFiles, includeNewFiles, recurseDirectoriesDepth, ioService, recheckFileSizeDurationMilliseconds);
                BOOST_REQUIRE(depthIndexToExpectedAbsoluteList[recurseDirectoriesDepth] == ds.GetListOfFilesAbsolute());
                BOOST_REQUIRE(depthIndexToExpectedRelativeList[recurseDirectoriesDepth] == ds.GetListOfFilesRelativeCopy());
                BOOST_REQUIRE(ds.GetSetOfMonitoredDirectoriesAbsolute().empty());
                BOOST_REQUIRE(ds.GetSetOfMonitoredDirectoriesRelativeCopy().empty());
                boost::filesystem::path nextFilePathAbsolute;
                boost::filesystem::path nextFilePathRelative;
                DirectoryScanner::path_list_t gotPoppedAbsoluteList;
                DirectoryScanner::path_list_t gotPoppedRelativeList;
                while (ds.GetNextFilePath(nextFilePathAbsolute, nextFilePathRelative)) {
                    gotPoppedAbsoluteList.emplace_back(std::move(nextFilePathAbsolute));
                    gotPoppedRelativeList.emplace_back(std::move(nextFilePathRelative));
                }
                BOOST_REQUIRE(ds.GetListOfFilesAbsolute().empty());
                BOOST_REQUIRE(ds.GetListOfFilesRelativeCopy().empty());
                BOOST_REQUIRE(depthIndexToExpectedAbsoluteList[recurseDirectoriesDepth] == gotPoppedAbsoluteList);
                BOOST_REQUIRE(depthIndexToExpectedRelativeList[recurseDirectoriesDepth] == gotPoppedRelativeList);
                //std::cout << "depth: " << recurseDirectoriesDepth << "\n";
                //std::cout << "print absolute\n";
                //std::cout << ds.GetListOfFilesAbsolute() << "\n";
                //std::cout << "print relative\n";
                //std::cout << ds.GetListOfFilesRelativeCopy() << "\n";
                //std::cout << "\n\n";
            }
            ioService.run();
            ioService.reset();
        }

        includeExistingFiles = false;
        includeNewFiles = true;
        for (unsigned int recurseDirectoriesDepth = 0; recurseDirectoriesDepth <= 4; ++recurseDirectoriesDepth) {
            {
                DirectoryScanner ds(rootPath, includeExistingFiles, includeNewFiles, recurseDirectoriesDepth, ioService, recheckFileSizeDurationMilliseconds);
                BOOST_REQUIRE(ds.GetListOfFilesAbsolute().empty());
                BOOST_REQUIRE(ds.GetListOfFilesRelativeCopy().empty());
                BOOST_REQUIRE(depthIndexToExpectedAbsoluteDirectoriesMonitoredSet[recurseDirectoriesDepth] == ds.GetSetOfMonitoredDirectoriesAbsolute());
                BOOST_REQUIRE(depthIndexToExpectedRelativeDirectoriesMonitoredSet[recurseDirectoriesDepth] == ds.GetSetOfMonitoredDirectoriesRelativeCopy());
                //std::cout << "depth: " << recurseDirectoriesDepth << "\n";
                //std::cout << "print absolute\n";
                //std::cout << ds.GetSetOfMonitoredDirectoriesAbsolute() << "\n";
                //std::cout << "print relative\n";
                //std::cout << ds.GetSetOfMonitoredDirectoriesRelativeCopy() << "\n";
                //std::cout << "\n\n";
                //ioService.stop();
            }
            ioService.run();
            ioService.reset();
            
        }

        includeExistingFiles = false;
        includeNewFiles = true;
        {
            {
                unsigned int recurseDirectoriesDepth = 4;
                DirectoryScanner ds(rootPath, includeExistingFiles, includeNewFiles, recurseDirectoriesDepth, ioService, recheckFileSizeDurationMilliseconds);
                {
                    boost::filesystem::ofstream os(rootPath / "a/b/d" / d4Dir / d4FileNew);
                    os << "my new file";
                }
                fs::create_directories(rootPath / "a/b/d" / d4Dir2 / "g");//d4Dir2(f) should add, but g exceeds depth (and won't be detected anyway since listener on f not exist)
                                                               // (but will be detected by manual iteration after event added)
                fs::create_directories(rootPath / "a/b/w/x/y");//w,x should add, but y exceeds depth
                // (but will be detected by manual iteration after event added)
                {
                    std::ofstream os((rootPath / "a/b/w/x/d4x_filenew.txt").string());
                    os << "my new file";
                }
                {
                    std::ofstream os((rootPath / "a/b/w/x/y/d5y_filenew.txt").string()); //too deep
                    os << "my new file";
                }
                fs::create_directories(rootPath / "a/b/d" / d4Dir / "h");//h exceeds depth
                ioService.run_for(std::chrono::seconds(2));
                const DirectoryScanner::path_list_t expectedAbsoluteList({ rp / "a/b/d" / d4Dir / d4FileNew, rp / "a/b/w/x/d4x_filenew.txt" });
                const DirectoryScanner::path_list_t expectedRelativeList({ fs::path("a/b/d" / d4Dir / d4FileNew), fs::path("a/b/w/x/d4x_filenew.txt") });
                DirectoryScanner::path_list_t gotAbsoluteList = ds.GetListOfFilesAbsolute();
                gotAbsoluteList.sort();
                DirectoryScanner::path_list_t gotRelativeList = ds.GetListOfFilesRelativeCopy();
                gotRelativeList.sort();
                BOOST_REQUIRE(expectedAbsoluteList == gotAbsoluteList);
                BOOST_REQUIRE(expectedRelativeList == gotRelativeList);
                DirectoryScanner::path_set_t expectedAbsoluteDirectorySet = depthIndexToExpectedAbsoluteDirectoriesMonitoredSet[recurseDirectoriesDepth];
                DirectoryScanner::path_set_t expectedRelativeDirectorySet = depthIndexToExpectedRelativeDirectoriesMonitoredSet[recurseDirectoriesDepth];
                BOOST_REQUIRE(expectedAbsoluteDirectorySet.emplace(rp / "a/b/d" / d4Dir2).second);
                BOOST_REQUIRE(expectedAbsoluteDirectorySet.emplace(rp / "a/b/w").second);
                BOOST_REQUIRE(expectedAbsoluteDirectorySet.emplace(rp / "a/b/w/x").second);

                BOOST_REQUIRE(expectedRelativeDirectorySet.emplace(aRp / fs::path("b/d" / d4Dir2)).second);
                BOOST_REQUIRE(expectedRelativeDirectorySet.emplace(aRp / fs::path("b/w")).second);
                BOOST_REQUIRE(expectedRelativeDirectorySet.emplace(aRp / fs::path("b/w/x")).second);

                BOOST_REQUIRE(expectedAbsoluteDirectorySet == ds.GetSetOfMonitoredDirectoriesAbsolute());
                BOOST_REQUIRE(expectedRelativeDirectorySet == ds.GetSetOfMonitoredDirectoriesRelativeCopy());
            }
            ioService.run();
            ioService.reset();

        }
        //ioService.reset();
        const uintmax_t numRemoved = fs::remove_all(rootPath);
        //std::cout << "removed " << numRemoved << "\n";
        BOOST_REQUIRE_EQUAL(numRemoved, 12 + 9); //+9 d4e_filenew.txt, f, g, h, w, x, y, d4x_filenew.txt, d5y_filenew.txt
    }

    

}


//
// Copyright (c) 2008, 2009 Boris Schaeling <boris@highscore.de>
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
#include <boost/test/unit_test.hpp>
#include "dir_monitor/dir_monitor.hpp"
#include "check_paths.hpp"
#include "directory.hpp"
#include <boost/predef/os.h>

static boost::asio::io_service io_service; //must be declared static

BOOST_AUTO_TEST_CASE(dir_monitor_sync_create_file)
{
    directory dir(TEST_DIR1 "_sync_create_file");

    boost::asio::dir_monitor dm(io_service);
    dm.add_directory(TEST_DIR1 "_sync_create_file");

    const boost::filesystem::path test_file1 = dir.create_file(TEST_FILE1);

    boost::asio::dir_monitor_event ev = dm.monitor();

    BOOST_CHECK_THE_SAME_PATHS_RELATIVE(ev.path, test_file1);
    BOOST_CHECK_EQUAL(ev.type, boost::asio::dir_monitor_event::added);
}

BOOST_AUTO_TEST_CASE(dir_monitor_sync_rename_file)
{
    directory dir(TEST_DIR1 "_sync_rename_file");
    const boost::filesystem::path test_file1 = dir.create_file(TEST_FILE1);

    boost::asio::dir_monitor dm(io_service);
    dm.add_directory(TEST_DIR1 "_sync_rename_file");

    const boost::filesystem::path test_file2 = dir.rename_file(TEST_FILE1, TEST_FILE2);
    while (true) {
        boost::asio::dir_monitor_event ev = dm.monitor();
#if (BOOST_OS_MACOS) //only the file will cause an event, the dm has been fixed to prevent the directory creation event
        if (ev.type == boost::asio::dir_monitor_event::added) {
            continue;
        }
#endif
#if (BOOST_OS_BSD) //bsd does not have any rename events, only added and removed
        BOOST_CHECK_THE_SAME_PATHS_RELATIVE(ev.path, test_file2);
        BOOST_CHECK_EQUAL(ev.type, boost::asio::dir_monitor_event::added);
#else
        BOOST_CHECK_THE_SAME_PATHS_RELATIVE(ev.path, test_file1);
        BOOST_CHECK_EQUAL(ev.type, boost::asio::dir_monitor_event::renamed_old_name);
#endif
        break;
    }
    boost::asio::dir_monitor_event ev = dm.monitor();

#if (BOOST_OS_BSD)
    BOOST_CHECK_THE_SAME_PATHS_RELATIVE(ev.path, test_file1);
    BOOST_CHECK_EQUAL(ev.type, boost::asio::dir_monitor_event::removed);
#else
    BOOST_CHECK_THE_SAME_PATHS_RELATIVE(ev.path, test_file2);
    BOOST_CHECK_EQUAL(ev.type, boost::asio::dir_monitor_event::renamed_new_name);
#endif
}

BOOST_AUTO_TEST_CASE(dir_monitor_sync_remove_file)
{
    directory dir(TEST_DIR1 "_sync_remove_file");
    const boost::filesystem::path test_file1 = dir.create_file(TEST_FILE1);

    boost::asio::dir_monitor dm(io_service);
    dm.add_directory(TEST_DIR1 "_sync_remove_file");

    dir.remove_file(TEST_FILE1);
    while (true) {
        boost::asio::dir_monitor_event ev = dm.monitor();
#if (BOOST_OS_MACOS) //only the file will cause an event, the dm has been fixed to prevent the directory creation event
        if (ev.type == boost::asio::dir_monitor_event::added) {
            continue;
        }
#endif
        BOOST_CHECK_THE_SAME_PATHS_RELATIVE(ev.path, test_file1);
        BOOST_CHECK_EQUAL(ev.type, boost::asio::dir_monitor_event::removed);
        break;
    }
}

#if (!BOOST_OS_BSD) //bsd kqueue implementation only listens for directory change events, and altering the file won't trigger a directory change event 
BOOST_AUTO_TEST_CASE(dir_monitor_sync_modify_file)
{
    directory dir(TEST_DIR1 "_sync_modify_file");
    const boost::filesystem::path test_file1 = dir.create_file(TEST_FILE1);

    boost::asio::dir_monitor dm(io_service);
    dm.add_directory(TEST_DIR1 "_sync_modify_file");

    dir.write_file(TEST_FILE1, TEST_FILE2);
    while (true) {
        boost::asio::dir_monitor_event ev = dm.monitor();
#if (BOOST_OS_MACOS) //only the file will cause an event, the dm has been fixed to prevent the directory creation event
        if (ev.type == boost::asio::dir_monitor_event::added) {
            continue;
        }
#endif
        BOOST_CHECK_THE_SAME_PATHS_RELATIVE(ev.path, test_file1);
        BOOST_CHECK_EQUAL(ev.type, boost::asio::dir_monitor_event::modified);
        break;
    }
}
#endif

BOOST_AUTO_TEST_CASE(dir_monitor_sync_multiple_events)
{
    directory dir(TEST_DIR1 "_sync_multiple_events");

    boost::asio::dir_monitor dm(io_service);
    dm.add_directory(TEST_DIR1 "_sync_multiple_events");

    const boost::filesystem::path test_file1 = dir.create_file(TEST_FILE1);
#if (BOOST_OS_BSD)
    boost::this_thread::sleep(boost::posix_time::milliseconds(500));
#endif
    const boost::filesystem::path test_file2 = dir.rename_file(TEST_FILE1, TEST_FILE2);
    //dir.remove_file(TEST_FILE2); //does not work here with apple

    boost::asio::dir_monitor_event ev = dm.monitor();
    BOOST_CHECK_THE_SAME_PATHS_RELATIVE(ev.path, test_file1);
    BOOST_CHECK_EQUAL(ev.type, boost::asio::dir_monitor_event::added);

    ev = dm.monitor();
#if (BOOST_OS_BSD) //bsd does not have any rename events, only added and removed
    BOOST_CHECK_THE_SAME_PATHS_RELATIVE(ev.path, test_file2);
    BOOST_CHECK_EQUAL(ev.type, boost::asio::dir_monitor_event::added);
#else
    BOOST_CHECK_THE_SAME_PATHS_RELATIVE(ev.path, test_file1);
    BOOST_CHECK_EQUAL(ev.type, boost::asio::dir_monitor_event::renamed_old_name);
#endif

    ev = dm.monitor();
#if (BOOST_OS_BSD)
    BOOST_CHECK_THE_SAME_PATHS_RELATIVE(ev.path, test_file1);
    BOOST_CHECK_EQUAL(ev.type, boost::asio::dir_monitor_event::removed);
#else
    BOOST_CHECK_THE_SAME_PATHS_RELATIVE(ev.path, test_file2);
    BOOST_CHECK_EQUAL(ev.type, boost::asio::dir_monitor_event::renamed_new_name);
#endif

    dir.remove_file(TEST_FILE2); //works here with apple

    ev = dm.monitor();
    BOOST_CHECK_THE_SAME_PATHS_RELATIVE(ev.path, test_file2);
    BOOST_CHECK_EQUAL(ev.type, boost::asio::dir_monitor_event::removed);
}

BOOST_AUTO_TEST_CASE(dir_monitor_sync_destruction)
{
    directory dir(TEST_DIR1 "_sync_destruction");

    boost::asio::dir_monitor dm(io_service);
    dm.add_directory(TEST_DIR1 "_sync_destruction");

    dir.create_file(TEST_FILE1);
}
#if 1 //removed this test to remove dependency on boost::locale (but now have util/Utf8Paths helper instead)
BOOST_AUTO_TEST_CASE(dir_monitor_sync_non_ascii_paths)
{
    char utf8DirName[]  = "\xe6\x97\xa5\xe6\x9c\xac\xe5\x9b\xbd"; // 日本国
    char utf8FileName[] = "\xd8\xa7\xd9\x84\xd8\xb9\xd8\xb1\xd8\xa8\xd9\x8a\xd8\xa9"".txt"; // العربية.txt

    directory dir(utf8DirName);

    boost::asio::dir_monitor dm(io_service);
    dm.add_directory(utf8DirName);

    const boost::filesystem::path test_file = dir.create_file(utf8FileName);

    boost::asio::dir_monitor_event ev = dm.monitor();
    BOOST_CHECK_THE_SAME_PATHS_RELATIVE(ev.path, test_file);
    BOOST_CHECK_EQUAL(ev.type, boost::asio::dir_monitor_event::added);
}
#endif

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
static boost::system::error_code g_ec;
static boost::asio::dir_monitor_event g_ev;

static void Reset() {
    g_ec = boost::system::error_code();
    g_ev = boost::asio::dir_monitor_event();
}
static void dir_event_handler(const boost::system::error_code& ec, const boost::asio::dir_monitor_event& ev) {
    g_ec = ec;
    g_ev = ev;
}


BOOST_AUTO_TEST_CASE(dir_monitor_async_create_file)
{
    directory dir(TEST_DIR1 "_async_create_file");
    boost::asio::dir_monitor dm(io_service);
    dm.add_directory(TEST_DIR1 "_async_create_file");
    const boost::filesystem::path test_file1 = dir.create_file(TEST_FILE1);

    Reset();
    dm.async_monitor(boost::bind(&dir_event_handler, boost::placeholders::_1, boost::placeholders::_2));
    io_service.run();
    io_service.reset();
    BOOST_CHECK_EQUAL(g_ec, boost::system::error_code());
    BOOST_CHECK_THE_SAME_PATHS_RELATIVE(g_ev.path, test_file1);
    BOOST_CHECK_EQUAL(g_ev.type, boost::asio::dir_monitor_event::added);

}


BOOST_AUTO_TEST_CASE(dir_monitor_async_rename_file)
{
    directory dir(TEST_DIR1 "_async_rename_file");
    const boost::filesystem::path test_file1 = dir.create_file(TEST_FILE1);

    boost::asio::dir_monitor dm(io_service);
    dm.add_directory(TEST_DIR1 "_async_rename_file");

    const boost::filesystem::path test_file2 = dir.rename_file(TEST_FILE1, TEST_FILE2);

    while (true) {
        Reset();

        dm.async_monitor(boost::bind(&dir_event_handler, boost::placeholders::_1, boost::placeholders::_2));
        io_service.run();
        io_service.reset();
	//std::cout << g_ev.type << " " << g_ev.path << "\n";
	//continue;
#if (BOOST_OS_MACOS) //only the file will cause an event, the dm has been fixed to prevent the directory creation event
        if (g_ev.type == boost::asio::dir_monitor_event::added) {
            continue;
        }
#endif
        BOOST_CHECK_EQUAL(g_ec, boost::system::error_code());
#if (BOOST_OS_BSD) //bsd does not have any rename events, only added and removed
        BOOST_CHECK_THE_SAME_PATHS_RELATIVE(g_ev.path, test_file2);
        BOOST_CHECK_EQUAL(g_ev.type, boost::asio::dir_monitor_event::added);
#else
        BOOST_CHECK_THE_SAME_PATHS_RELATIVE(g_ev.path, test_file1);
        BOOST_CHECK_EQUAL(g_ev.type, boost::asio::dir_monitor_event::renamed_old_name);
#endif
        break;
    }

    

    Reset();
    dm.async_monitor(boost::bind(&dir_event_handler, boost::placeholders::_1, boost::placeholders::_2));
    io_service.run();
    io_service.reset();

    BOOST_CHECK_EQUAL(g_ec, boost::system::error_code());
#if (BOOST_OS_BSD)
    BOOST_CHECK_THE_SAME_PATHS_RELATIVE(g_ev.path, test_file1);
    BOOST_CHECK_EQUAL(g_ev.type, boost::asio::dir_monitor_event::removed);
#else
    BOOST_CHECK_THE_SAME_PATHS_RELATIVE(g_ev.path, test_file2);
    BOOST_CHECK_EQUAL(g_ev.type, boost::asio::dir_monitor_event::renamed_new_name);
#endif
}

#if (!BOOST_OS_BSD) //bsd kqueue implementation only listens for directory change events, and altering the file won't trigger a directory change event 
BOOST_AUTO_TEST_CASE(dir_monitor_async_modify_file)
{
    directory dir(TEST_DIR1 "_async_modify_file");
    const boost::filesystem::path test_file2 = dir.create_file(TEST_FILE2);

    boost::asio::dir_monitor dm(io_service);
    dm.add_directory(TEST_DIR1 "_async_modify_file");

    dir.write_file(TEST_FILE2, TEST_FILE1);

    while (true) {
        Reset();

        dm.async_monitor(boost::bind(&dir_event_handler, boost::placeholders::_1, boost::placeholders::_2));
        io_service.run();
        io_service.reset();
#if (BOOST_OS_MACOS) //only the file will cause an event, the dm has been fixed to prevent the directory creation event
        if (g_ev.type == boost::asio::dir_monitor_event::added) {
            continue;
        }
#endif
        BOOST_CHECK_EQUAL(g_ec, boost::system::error_code());
        BOOST_CHECK_THE_SAME_PATHS_RELATIVE(g_ev.path, test_file2);
        BOOST_CHECK_EQUAL(g_ev.type, boost::asio::dir_monitor_event::modified);
        break;
    }
}
#endif


BOOST_AUTO_TEST_CASE(dir_monitor_async_remove_file)
{
    directory dir(TEST_DIR1 "_async_remove_file");
    const boost::filesystem::path test_file1 = dir.create_file(TEST_FILE1);

    boost::asio::dir_monitor dm(io_service);
    dm.add_directory(TEST_DIR1 "_async_remove_file");

    dir.remove_file(TEST_FILE1);

    while (true) {
        Reset();

        dm.async_monitor(boost::bind(&dir_event_handler, boost::placeholders::_1, boost::placeholders::_2));
        io_service.run();
        io_service.reset();
#if (BOOST_OS_MACOS) //only the file will cause an event, the dm has been fixed to prevent the directory creation event
        if (g_ev.type == boost::asio::dir_monitor_event::added) {
            continue;
        }
#endif
        BOOST_CHECK_EQUAL(g_ec, boost::system::error_code());
        BOOST_CHECK_THE_SAME_PATHS_RELATIVE(g_ev.path, test_file1);
        BOOST_CHECK_EQUAL(g_ev.type, boost::asio::dir_monitor_event::removed);
        break;
    }
}

BOOST_AUTO_TEST_CASE(dir_monitor_async_multiple_events)
{
    directory dir(TEST_DIR1 "_async_multiple_events");

    boost::asio::dir_monitor dm(io_service);
    dm.add_directory(TEST_DIR1 "_async_multiple_events");

    const boost::filesystem::path test_file1 = dir.create_file(TEST_FILE1);
#if (BOOST_OS_BSD)
    boost::this_thread::sleep(boost::posix_time::milliseconds(500));
#endif
    const boost::filesystem::path test_file2 = dir.rename_file(TEST_FILE1, TEST_FILE2);

    Reset();
    dm.async_monitor(boost::bind(&dir_event_handler, boost::placeholders::_1, boost::placeholders::_2));
    io_service.run();
    io_service.reset();
    BOOST_CHECK_EQUAL(g_ec, boost::system::error_code());
    BOOST_CHECK_THE_SAME_PATHS_RELATIVE(g_ev.path, test_file1);
    BOOST_CHECK_EQUAL(g_ev.type, boost::asio::dir_monitor_event::added);

    Reset();
    dm.async_monitor(boost::bind(&dir_event_handler, boost::placeholders::_1, boost::placeholders::_2));
    io_service.run();
    io_service.reset();
    BOOST_CHECK_EQUAL(g_ec, boost::system::error_code());
#if (BOOST_OS_BSD) //bsd does not have any rename events, only added and removed
    BOOST_CHECK_THE_SAME_PATHS_RELATIVE(g_ev.path, test_file2);
    BOOST_CHECK_EQUAL(g_ev.type, boost::asio::dir_monitor_event::added);
#else
    BOOST_CHECK_THE_SAME_PATHS_RELATIVE(g_ev.path, test_file1);
    BOOST_CHECK_EQUAL(g_ev.type, boost::asio::dir_monitor_event::renamed_old_name);
#endif

    Reset();
    dm.async_monitor(boost::bind(&dir_event_handler, boost::placeholders::_1, boost::placeholders::_2));
    io_service.run();
    io_service.reset();
    BOOST_CHECK_EQUAL(g_ec, boost::system::error_code());
#if (BOOST_OS_BSD)
    BOOST_CHECK_THE_SAME_PATHS_RELATIVE(g_ev.path, test_file1);
    BOOST_CHECK_EQUAL(g_ev.type, boost::asio::dir_monitor_event::removed);
#else
    BOOST_CHECK_THE_SAME_PATHS_RELATIVE(g_ev.path, test_file2);
    BOOST_CHECK_EQUAL(g_ev.type, boost::asio::dir_monitor_event::renamed_new_name);
#endif
}

static void aborted_async_call_handler(const boost::system::error_code &ec, const boost::asio::dir_monitor_event &ev)
{
    (void)ev;
    BOOST_CHECK_EQUAL(ec, boost::asio::error::operation_aborted);
}

BOOST_AUTO_TEST_CASE(dir_monitor_async_aborted_async_call)
{
    directory dir(TEST_DIR1 "_async_aborted_async_call");

    {
        boost::asio::dir_monitor dm(io_service);
        dm.add_directory(TEST_DIR1 "_async_aborted_async_call");

        dm.async_monitor(aborted_async_call_handler);
    }

    io_service.run();
    io_service.reset();
}

static void blocked_async_call_handler_with_local_ioservice(const boost::system::error_code &ec, const boost::asio::dir_monitor_event &ev)
{
    (void)ev;
    BOOST_CHECK_EQUAL(ec, boost::asio::error::operation_aborted);
}

BOOST_AUTO_TEST_CASE(dir_monitor_async_blocked_async_call)
{
    directory dir(TEST_DIR1 "_async_blocked_async_call");
    boost::thread t;

    {
        boost::asio::io_service local_io_service;

        boost::asio::dir_monitor dm(local_io_service);
        dm.add_directory(TEST_DIR1 "_async_blocked_async_call");

        dm.async_monitor(blocked_async_call_handler_with_local_ioservice);

        // run() is invoked on another thread to make async_monitor() call a blocking function.
        // When dm and io_service go out of scope they should be destroyed properly without
        // a thread being blocked.
        t = boost::thread(boost::bind(&boost::asio::io_service::run, boost::ref(local_io_service)));
        boost::this_thread::sleep(boost::posix_time::seconds(1));
    }
    t.join();
}

static void unregister_directory_handler(const boost::system::error_code &ec, const boost::asio::dir_monitor_event &ev)
{
    (void)ev;
    BOOST_CHECK_EQUAL(ec, boost::asio::error::operation_aborted);
}

BOOST_AUTO_TEST_CASE(dir_monitor_async_unregister_directory)
{
    directory dir(TEST_DIR1 "_async_unregister_directory");
    boost::thread t;

    {
        boost::asio::dir_monitor dm(io_service);
        dm.add_directory(TEST_DIR1 "_async_unregister_directory");
        dm.remove_directory(TEST_DIR1 "_async_unregister_directory");

        dir.create_file(TEST_FILE1);

        dm.async_monitor(unregister_directory_handler);

        // run() is invoked on another thread to make this test case return. Without using
        // another thread run() would block as the file was created after remove_directory()
        // had been called.
        t = boost::thread(boost::bind(&boost::asio::io_service::run, boost::ref(io_service)));
        boost::this_thread::sleep(boost::posix_time::seconds(1));
    }

    t.join();
    io_service.reset();
}

//Added to support UTF-8 Paths
BOOST_AUTO_TEST_CASE(dir_monitor_async_unregister_directory_as_path)
{
    directory dir(TEST_DIR1 "_async_unregister_directory_as_path");
    boost::filesystem::path testDirAsPath1(TEST_DIR1 "_async_unregister_directory_as_path");
    boost::thread t;

    {
        boost::asio::dir_monitor dm(io_service);
        dm.add_directory_as_path(testDirAsPath1);
        dm.remove_directory_as_path(testDirAsPath1);

        dir.create_file(TEST_FILE1);

        dm.async_monitor(unregister_directory_handler);

        // run() is invoked on another thread to make this test case return. Without using
        // another thread run() would block as the file was created after remove_directory()
        // had been called.
        t = boost::thread(boost::bind(&boost::asio::io_service::run, boost::ref(io_service)));
        boost::this_thread::sleep(boost::posix_time::seconds(1));
    }

    t.join();
    io_service.reset();
}

static void two_dir_monitors_handler(const boost::system::error_code &ec, const boost::asio::dir_monitor_event &ev)
{
    (void)ev;
    BOOST_CHECK_EQUAL(ec, boost::asio::error::operation_aborted);
}

BOOST_AUTO_TEST_CASE(dir_monitor_async_two_dir_monitors)
{
    directory dir1(TEST_DIR1 "_async_two_dir_monitors1");
    directory dir2(TEST_DIR2 "_async_two_dir_monitors2");
    boost::thread t;

    {
        boost::asio::dir_monitor dm1(io_service);
        dm1.add_directory(TEST_DIR1 "_async_two_dir_monitors1");

        boost::asio::dir_monitor dm2(io_service);
        dm2.add_directory(TEST_DIR2 "_async_two_dir_monitors2");

        dir2.create_file(TEST_FILE1);

        dm1.async_monitor(two_dir_monitors_handler);

        // run() is invoked on another thread to make this test case return. Without using
        // another thread run() would block as the directory the file was created in is
        // monitored by dm2 while async_monitor() was called for dm1.
        t = boost::thread(boost::bind(&boost::asio::io_service::run, boost::ref(io_service)));
        boost::this_thread::sleep(boost::posix_time::seconds(1));
    }

    t.join();
    io_service.reset();
}

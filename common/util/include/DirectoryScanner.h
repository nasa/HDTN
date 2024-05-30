/**
 * @file DirectoryScanner.h
 * @author  Brian Tomko <brian.j.tomko@nasa.gov>
 *
 * @copyright Copyright (c) 2021 United States Government as represented by
 * the National Aeronautics and Space Administration.
 * No copyright is claimed in the United States under Title 17, U.S.Code.
 * All Other Rights Reserved.
 *
 * @section LICENSE
 * Released under the NASA Open Source Agreement (NOSA)
 * See LICENSE.md in the source root directory for more information.
 *
 * @section DESCRIPTION
 *
 * This DirectoryScanner class encapsulates the appropriate functionality
 * to scan for existing files within a directory recursively as well as
 * to detect when new files have been added to the directory.
 */

#ifndef _DIRECTORY_SCANNER_H
#define _DIRECTORY_SCANNER_H 1

#include <string>
#include <boost/filesystem/path.hpp>
#include <boost/asio.hpp>
#include <boost/thread.hpp>
#include <memory>
#include <list>
#include <set>
#include <map>
#include <queue>
#include "dir_monitor/dir_monitor.hpp"
#include "hdtn_util_export.h"

class DirectoryScanner {
private:
    DirectoryScanner() = delete;
public:
    typedef std::list<boost::filesystem::path> path_list_t;
    typedef std::set<boost::filesystem::path> path_set_t;
    typedef std::pair<uintmax_t, unsigned int> filesize_queuecount_pair_t;
    typedef std::map<boost::filesystem::path, filesize_queuecount_pair_t> path_to_size_map_t;
    typedef std::pair<boost::posix_time::ptime, path_to_size_map_t::iterator> ptime_plus_mapit_pair_t;
    typedef std::queue<ptime_plus_mapit_pair_t> timer_queue_t;
    HDTN_UTIL_EXPORT DirectoryScanner(const boost::filesystem::path & rootFileOrFolderPath,
        bool includeExistingFiles, bool includeNewFiles, unsigned int recurseDirectoriesDepth,
        boost::asio::io_service & ioServiceRef, const uint64_t recheckFileSizeDurationMilliseconds);
    HDTN_UTIL_EXPORT ~DirectoryScanner();
    HDTN_UTIL_EXPORT std::size_t GetNumberOfFilesToSend() const;
    HDTN_UTIL_EXPORT std::size_t GetNumberOfCurrentlyMonitoredDirectories() const;
    HDTN_UTIL_EXPORT bool GetNextFilePath(boost::filesystem::path& nextFilePathAbsolute, boost::filesystem::path& nextFilePathRelative);
    HDTN_UTIL_EXPORT bool GetNextFilePathTimeout(boost::filesystem::path& nextFilePathAbsolute,
        boost::filesystem::path& nextFilePathRelative, const boost::posix_time::time_duration & timeout);
    HDTN_UTIL_EXPORT void InterruptTimedWait();
    HDTN_UTIL_EXPORT friend std::ostream& operator<<(std::ostream& os, const path_list_t& o);
    HDTN_UTIL_EXPORT friend std::ostream& operator<<(std::ostream& os, const path_set_t& o);
    HDTN_UTIL_EXPORT const path_list_t & GetListOfFilesAbsolute() const;
    HDTN_UTIL_EXPORT path_list_t GetListOfFilesRelativeCopy() const;
    HDTN_UTIL_EXPORT const path_set_t& GetSetOfMonitoredDirectoriesAbsolute() const;
    HDTN_UTIL_EXPORT path_set_t GetSetOfMonitoredDirectoriesRelativeCopy() const;
private:
    HDTN_UTIL_NO_EXPORT bool GetNextFilePath_NotThreadSafe(boost::filesystem::path& nextFilePathAbsolute, boost::filesystem::path& nextFilePathRelative);
    HDTN_UTIL_NO_EXPORT void Reload();
    HDTN_UTIL_NO_EXPORT void Clear();
    HDTN_UTIL_NO_EXPORT void OnDirectoryChangeEvent(const boost::system::error_code& ec, const boost::asio::dir_monitor_event& ev);
    HDTN_UTIL_NO_EXPORT void OnRecheckFileSize_TimerExpired(const boost::system::error_code& e);
    HDTN_UTIL_NO_EXPORT void TryAddNewFile(const boost::filesystem::path& p);
    HDTN_UTIL_NO_EXPORT void IterateDirectories(const boost::filesystem::path& rootDirectory, const unsigned int startingRecursiveDepthIndex, const bool addFiles);
private:
    boost::mutex m_pathsOfFilesListMutex;
    boost::condition_variable m_pathsOfFilesListCv;
    path_list_t m_pathsOfFilesList;
    path_list_t::iterator m_currentFilePathIterator;
    const boost::filesystem::path m_rootFileOrFolderPath;
    boost::filesystem::path m_relativeToPath;
    const bool m_includeExistingFiles;
    const bool m_includeNewFiles;
    const unsigned int m_recurseDirectoriesDepth;
    boost::asio::dir_monitor m_dirMonitor;
    boost::asio::deadline_timer m_timerNewFileComplete;
    const boost::posix_time::time_duration m_timeDurationToRecheckFileSize;
    path_set_t m_currentlyMonitoredDirectoryPaths;
    path_to_size_map_t m_currentlyPendingFilesToAddMap;
    path_set_t m_newFilePathsAddedSet;
    timer_queue_t m_currentlyPendingFilesToAddTimerQueue;
};

#endif //_DIRECTORY_SCANNER_H

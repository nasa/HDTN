/**
 * @file DirectoryScanner.cpp
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
 */

#include <string.h>
#include "DirectoryScanner.h"
#include "Logger.h"
#include <boost/make_unique.hpp>
#include <boost/filesystem/fstream.hpp>
#include <boost/filesystem/operations.hpp>
#include <boost/version.hpp>

#if (BOOST_VERSION >= 107200)
#include <boost/filesystem/directory.hpp>
#endif

static constexpr hdtn::Logger::SubProcess subprocess = hdtn::Logger::SubProcess::none;

DirectoryScanner::DirectoryScanner(const boost::filesystem::path& rootFileOrFolderPath,
    bool includeExistingFiles, bool includeNewFiles, unsigned int recurseDirectoriesDepth,
    boost::asio::io_service& ioServiceRef, const uint64_t recheckFileSizeDurationMilliseconds) :
    m_currentFilePathIterator(m_pathsOfFilesList.end()),
    m_rootFileOrFolderPath((rootFileOrFolderPath.has_parent_path()) ?
        rootFileOrFolderPath :
        (boost::filesystem::path(".") / rootFileOrFolderPath)),
    m_includeExistingFiles(includeExistingFiles),
    m_includeNewFiles(includeNewFiles),
    m_recurseDirectoriesDepth(recurseDirectoriesDepth),
    m_dirMonitor(ioServiceRef),
    m_timerNewFileComplete(ioServiceRef),
    m_timeDurationToRecheckFileSize(boost::posix_time::milliseconds(recheckFileSizeDurationMilliseconds))
{
    Reload();
}

DirectoryScanner::~DirectoryScanner() {
    Clear();
}

std::size_t DirectoryScanner::GetNumberOfFilesToSend() const {
    return m_pathsOfFilesList.size();
}
std::size_t DirectoryScanner::GetNumberOfCurrentlyMonitoredDirectories() const {
    return m_currentlyMonitoredDirectoryPaths.size();
}

const DirectoryScanner::path_list_t& DirectoryScanner::GetListOfFilesAbsolute() const {
    return m_pathsOfFilesList;
}

DirectoryScanner::path_list_t DirectoryScanner::GetListOfFilesRelativeCopy() const {
    path_list_t pl;
    for (path_list_t::const_iterator it = m_pathsOfFilesList.cbegin();
        it != m_pathsOfFilesList.cend(); ++it)
    {
        pl.emplace_back(boost::filesystem::relative((*it), m_relativeToPath));
    }
    return pl;
}

const DirectoryScanner::path_set_t& DirectoryScanner::GetSetOfMonitoredDirectoriesAbsolute() const {
    return m_currentlyMonitoredDirectoryPaths;
}

DirectoryScanner::path_set_t DirectoryScanner::GetSetOfMonitoredDirectoriesRelativeCopy() const {
    path_set_t ps;
    for (path_set_t::const_iterator it = m_currentlyMonitoredDirectoryPaths.cbegin();
        it != m_currentlyMonitoredDirectoryPaths.cend(); ++it)
    {
        ps.emplace(boost::filesystem::relative((*it), m_relativeToPath));
    }
    return ps;
}

std::ostream& operator<<(std::ostream& os, const DirectoryScanner::path_list_t& o) {
    for (DirectoryScanner::path_list_t::const_iterator it = o.cbegin(); it != o.cend(); ++it) {
        os << (*it) << "\n";
    }
    return os;
}
std::ostream& operator<<(std::ostream& os, const DirectoryScanner::path_set_t& o) {
    for (DirectoryScanner::path_set_t::const_iterator it = o.cbegin(); it != o.cend(); ++it) {
        os << (*it) << "\n";
    }
    return os;
}

bool DirectoryScanner::GetNextFilePath_NotThreadSafe(boost::filesystem::path& nextFilePathAbsolute, boost::filesystem::path& nextFilePathRelative) {
    if (m_currentFilePathIterator == m_pathsOfFilesList.end()) {
        return false; //stopping criteria
    }
    nextFilePathAbsolute = std::move(*m_currentFilePathIterator);
    nextFilePathRelative = boost::filesystem::relative(nextFilePathAbsolute, m_relativeToPath);
    path_list_t::iterator toEraseIt = m_currentFilePathIterator;
    ++m_currentFilePathIterator;
    m_pathsOfFilesList.erase(toEraseIt);
    return true;
}

bool DirectoryScanner::GetNextFilePath(boost::filesystem::path& nextFilePathAbsolute, boost::filesystem::path& nextFilePathRelative) {
    boost::mutex::scoped_lock lock(m_pathsOfFilesListMutex);
    return GetNextFilePath_NotThreadSafe(nextFilePathAbsolute, nextFilePathRelative);
}

bool DirectoryScanner::GetNextFilePathTimeout(boost::filesystem::path& nextFilePathAbsolute,
    boost::filesystem::path& nextFilePathRelative, const boost::posix_time::time_duration& timeout)
{
    boost::mutex::scoped_lock lock(m_pathsOfFilesListMutex);
    if (!GetNextFilePath_NotThreadSafe(nextFilePathAbsolute, nextFilePathRelative)) {
        m_pathsOfFilesListCv.timed_wait(lock, timeout); //lock mutex (above) before checking condition
    }
    return GetNextFilePath_NotThreadSafe(nextFilePathAbsolute, nextFilePathRelative);
}

void DirectoryScanner::InterruptTimedWait() {
    {
        boost::mutex::scoped_lock lock(m_pathsOfFilesListMutex);
    }
    m_pathsOfFilesListCv.notify_one();
}

void DirectoryScanner::Clear() {
    //clear directories monitored by dir_monitor
    for (path_set_t::const_iterator it = m_currentlyMonitoredDirectoryPaths.cbegin(); it != m_currentlyMonitoredDirectoryPaths.cend(); ++it) {
        m_dirMonitor.remove_directory_as_path(*it); //does not appear to throw
    }
    m_currentlyMonitoredDirectoryPaths.clear();

    try {
        m_timerNewFileComplete.cancel();
    }
    catch (boost::system::system_error& e) {
        LOG_ERROR(subprocess) << "unable to cancel m_timerNewFileComplete: " << e.what();
    }
    

    m_pathsOfFilesList.clear();
    m_currentFilePathIterator = m_pathsOfFilesList.end();
}

//must set m_rootfileOrFolderPathString first
void DirectoryScanner::Reload() {
    Clear();
    
    if (boost::filesystem::is_directory(m_rootFileOrFolderPath)) {
        m_relativeToPath = m_rootFileOrFolderPath;
        if (m_includeNewFiles) {
            try {
                m_dirMonitor.add_directory_as_path(m_rootFileOrFolderPath);
                m_currentlyMonitoredDirectoryPaths.emplace(m_rootFileOrFolderPath);
            }
            catch (std::exception& e) {
                LOG_INFO(subprocess) << e.what();
            }
        }
        IterateDirectories(m_rootFileOrFolderPath, 0, m_includeExistingFiles);
        m_pathsOfFilesList.sort();
    }
    else if (boost::filesystem::is_regular_file(m_rootFileOrFolderPath) && (m_rootFileOrFolderPath.extension().size() > 1)) { //just one file
        if (m_rootFileOrFolderPath.size() <= 255) {
            if (m_includeExistingFiles) {
                m_pathsOfFilesList.emplace_back(m_rootFileOrFolderPath);
                m_relativeToPath = m_rootFileOrFolderPath.parent_path(); //guaranteed to have a parent path per DirectoryScanner constructor
            }
        }
        else {
            LOG_ERROR(subprocess) << m_rootFileOrFolderPath << " is too long";
        }
    }
    else {
        LOG_FATAL(subprocess) << m_rootFileOrFolderPath << " is not a directory nor a file with an extension";
        return;
    }

    m_currentFilePathIterator = m_pathsOfFilesList.begin();

    if (!m_currentlyMonitoredDirectoryPaths.empty()) {
        m_dirMonitor.async_monitor(boost::bind(&DirectoryScanner::OnDirectoryChangeEvent, this, boost::placeholders::_1, boost::placeholders::_2));
    }
}

void DirectoryScanner::IterateDirectories(const boost::filesystem::path& rootDirectory, const unsigned int startingRecursiveDepthIndex, const bool addFiles) {
    for (boost::filesystem::recursive_directory_iterator dirIt(rootDirectory), eod; dirIt != eod; ++dirIt) {
        const boost::filesystem::path& p = dirIt->path();
        const bool isFile = boost::filesystem::is_regular_file(p);
        const bool isDirectory = boost::filesystem::is_directory(p);
        const unsigned int depth = static_cast<unsigned int>(dirIt.depth()) + startingRecursiveDepthIndex;
        if (isDirectory) {
            if (depth >= m_recurseDirectoriesDepth) { //don't iterate files within that will have depth 1 greater than this directory that contains them
#if (BOOST_VERSION >= 107200)
                dirIt.disable_recursion_pending(); //identical to no_push()
#else
                dirIt.no_push(); //deprecated in versions 1.72 and above
#endif
            }
            else {
                if (m_includeNewFiles) {
                    if (m_currentlyMonitoredDirectoryPaths.emplace(p).second) {
                        try {
                            m_dirMonitor.add_directory_as_path(p);
                        }
                        catch (std::exception& e) {
                            LOG_INFO(subprocess) << e.what();
                            m_currentlyMonitoredDirectoryPaths.erase(p);
                        }
                    }
                }
            }
        }
        else if (isFile && (p.extension().size() > 1)) {
            if (p.size() <= 255) {
                if (startingRecursiveDepthIndex == 0) { //existing files
                    if (m_newFilePathsAddedSet.emplace(p).second) { //keep permanent record of found files
                        if (addFiles) {
                            m_pathsOfFilesList.emplace_back(p);
                        }
                    }
                }
                else { //new files, use timer scheme to check filesize not changed
                    TryAddNewFile(p);
                }
            }
            else {
                LOG_INFO(subprocess) << "skipping " << p;
            }
        }
    }
}

void DirectoryScanner::OnDirectoryChangeEvent(const boost::system::error_code& ec, const boost::asio::dir_monitor_event& ev) {
    if (!ec) {
        const boost::filesystem::path relPath = boost::filesystem::relative(ev.path, m_relativeToPath);
        const unsigned int recursionDepthRelative = static_cast<unsigned int>(std::distance(relPath.begin(), relPath.end()) - 1);
        if ((ev.type == boost::asio::dir_monitor_event::added) || (ev.type == boost::asio::dir_monitor_event::modified)) {
            if (boost::filesystem::is_directory(ev.path)) {
                if (m_currentlyMonitoredDirectoryPaths.count(ev.path) == 0) {
                    if (recursionDepthRelative >= m_recurseDirectoriesDepth) {} //don't iterate files within that will have depth 1 greater than this directory that contains them
                    else {
                        try {
                            m_dirMonitor.add_directory_as_path(ev.path);
                            m_currentlyMonitoredDirectoryPaths.emplace(ev.path);
                        }
                        catch (std::exception& e) {
                            LOG_INFO(subprocess) << e.what();
                        }

                        //now that the directory is added, iterate in case a new directory was added before the event listener was added
                        IterateDirectories(ev.path, recursionDepthRelative + 1, true); //+1 because entering directory
                    }
                }
            }
            else if (boost::filesystem::is_regular_file(ev.path)) {
                if (recursionDepthRelative > m_recurseDirectoriesDepth) { 
                    //don't add a file that is too deep (this behavior was noticed on certain linux distros)
                    //std::cout << "EVENT SKIPFILE depth=" << recursionDepthRelative << " p=" << relPath << "\n";
                }
                else {
                    TryAddNewFile(ev.path);
                }
            }
        }
        else if ((ev.type == boost::asio::dir_monitor_event::removed) || (ev.type == boost::asio::dir_monitor_event::renamed_old_name)) {
            if (m_currentlyMonitoredDirectoryPaths.count(ev.path)) { //also tests if (boost::filesystem::is_directory(ev.path)) { but no longer exists
                m_dirMonitor.remove_directory_as_path(ev.path);
                m_currentlyMonitoredDirectoryPaths.erase(ev.path);
            }
        }
        else if (ev.type == boost::asio::dir_monitor_event::renamed_new_name) {
            if (boost::filesystem::is_directory(ev.path)) {
                if (m_currentlyMonitoredDirectoryPaths.count(ev.path) == 0) {
                    if (recursionDepthRelative >= m_recurseDirectoriesDepth) {} //don't iterate files within that will have depth 1 greater than this directory that contains them
                    else {
                        try {
                            m_dirMonitor.add_directory_as_path(ev.path);
                            m_currentlyMonitoredDirectoryPaths.emplace(ev.path);
                        }
                        catch (std::exception& e) {
                            LOG_INFO(subprocess) << e.what();
                        }
                    }
                }
            }
            else if (boost::filesystem::is_regular_file(ev.path)) { //on linux, sometimes if you move a file into a directory it causes a rename event
                if (recursionDepthRelative > m_recurseDirectoriesDepth) {
                    //don't add a file that is too deep (this behavior was noticed on certain linux distros)
                    //std::cout << "EVENT SKIPFILE depth=" << recursionDepthRelative << " p=" << relPath << "\n";
                }
                else {
                    TryAddNewFile(ev.path);
                }
            }
        }
        m_dirMonitor.async_monitor(boost::bind(&DirectoryScanner::OnDirectoryChangeEvent, this, boost::placeholders::_1, boost::placeholders::_2));
    }
    else if (ec == boost::asio::error::operation_aborted) {
        //m_dirMonitor.async_monitor(boost::bind(&DirectoryScanner::OnDirectoryChangeEvent, this, boost::placeholders::_1, boost::placeholders::_2));
    }
    else {
        LOG_ERROR(subprocess) << "Error code " << ec;
    }
}

void DirectoryScanner::TryAddNewFile(const boost::filesystem::path& p) {
    if (m_newFilePathsAddedSet.count(p) == 0) { //if not yet already added
        const uintmax_t fileSize = boost::filesystem::file_size(p); //mark the initial file size, check it again to make sure it's the same after timer to make sure file is not growing
        std::pair<path_to_size_map_t::iterator, bool> retVal = m_currentlyPendingFilesToAddMap.emplace(p, filesize_queuecount_pair_t(fileSize, 1));
        filesize_queuecount_pair_t& filesizeQueuecountPairRef = retVal.first->second;
        const bool alreadyInQueue = (!retVal.second); //insertion didn't happen (already pending in the size change timer)
        filesizeQueuecountPairRef.second += alreadyInQueue; //invalidate previous event for this path if already in queue (sometimes a new file will trigger multiple directory events such as "added" then "modified")
        filesizeQueuecountPairRef.first = fileSize; //make sure using latest fileSize
        const bool timerIsStopped = m_currentlyPendingFilesToAddTimerQueue.empty();
        m_currentlyPendingFilesToAddTimerQueue.emplace(boost::posix_time::microsec_clock::universal_time() + m_timeDurationToRecheckFileSize, retVal.first);
        if (timerIsStopped) {
            m_timerNewFileComplete.expires_at(m_currentlyPendingFilesToAddTimerQueue.front().first);
            m_timerNewFileComplete.async_wait(boost::bind(&DirectoryScanner::OnRecheckFileSize_TimerExpired, this, boost::asio::placeholders::error));
        }
        //ev.path.size();
    }
}

void DirectoryScanner::OnRecheckFileSize_TimerExpired(const boost::system::error_code& e) {
    if (e != boost::asio::error::operation_aborted) {
        // Timer was not cancelled, take necessary action.
        path_to_size_map_t::iterator thisExpiredIt = m_currentlyPendingFilesToAddTimerQueue.front().second;
        m_currentlyPendingFilesToAddTimerQueue.pop();
        const boost::filesystem::path & thisPath = thisExpiredIt->first;
        filesize_queuecount_pair_t & thisFilesizeQueueCount = thisExpiredIt->second;
        if (--thisFilesizeQueueCount.second) { //queuecount not zero (still remaining event(s) for this path within the queue)
            //ignore this invalid expired event
        }
        else { //queuecount now zero (last event for this path removed from queue)
            //check filesize
            const uintmax_t prevFileSize = thisFilesizeQueueCount.first;
            const uintmax_t nowFileSize = boost::filesystem::file_size(thisPath);
            if (prevFileSize == nowFileSize) { //last event for this file and filesize remained same for a period of time
                //add the new file
                bool fileIsOpenable;
                {
                    boost::filesystem::ifstream ifstr(thisPath, std::ifstream::in | std::ifstream::binary);
                    fileIsOpenable = ifstr.good();
                    ifstr.close();
                }
                if (fileIsOpenable) {
                    if (m_newFilePathsAddedSet.emplace(thisPath).second) {
                        {
                            boost::mutex::scoped_lock lock(m_pathsOfFilesListMutex);
                            const bool iteratorAtEnd = (m_currentFilePathIterator == m_pathsOfFilesList.end());
                            m_pathsOfFilesList.emplace_back(thisPath);
                            if (iteratorAtEnd) {
                                m_currentFilePathIterator = std::prev(m_pathsOfFilesList.end());
                            }
                        }
                        m_pathsOfFilesListCv.notify_one();
                    }
                    m_currentlyPendingFilesToAddMap.erase(thisExpiredIt); //references now invalid
                }
                else { //file not openable (perhaps permission denied during copy)
                    LOG_INFO(subprocess) << "file not openable yet: " << thisPath;
                    thisFilesizeQueueCount.second = 1;
                    m_currentlyPendingFilesToAddTimerQueue.emplace(boost::posix_time::microsec_clock::universal_time() + m_timeDurationToRecheckFileSize, thisExpiredIt);
                }
            }
            else { //file size mismatch.. wait again to see if the file stops changing sizes
                thisFilesizeQueueCount.second = 1;
                m_currentlyPendingFilesToAddTimerQueue.emplace(boost::posix_time::microsec_clock::universal_time() + m_timeDurationToRecheckFileSize, thisExpiredIt);
            }
        }

        //restart timer if queue not empty
        if (!m_currentlyPendingFilesToAddTimerQueue.empty()) {
            m_timerNewFileComplete.expires_at(m_currentlyPendingFilesToAddTimerQueue.front().first);
            m_timerNewFileComplete.async_wait(boost::bind(&DirectoryScanner::OnRecheckFileSize_TimerExpired, this, boost::asio::placeholders::error));
        }
    }
}

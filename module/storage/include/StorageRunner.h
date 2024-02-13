/**
 * @file StorageRunner.h
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
 * This StorageRunner class is used for launching just the HDTN storage module into its own process.
 * The StorageRunner provides a blocking Run function which creates and
 * initializes an Storage object by processing/using the various command line arguments.
 * StorageRunner is only used when running HDTN in distributed mode in which there
 * is a single process dedicated to Storage.
 * StorageRunner also provides a signal handler listener to capture Ctrl+C (SIGINT) events
 * for clean termination.
 */

#ifndef _STORAGE_RUNNER_H
#define _STORAGE_RUNNER_H 1

#include <stdint.h>
#include "ZmqStorageInterface.h"
#include "Logger.h"
#include <atomic>

class StorageRunner {
public:
    STORAGE_LIB_EXPORT StorageRunner();
    STORAGE_LIB_EXPORT ~StorageRunner();
    STORAGE_LIB_EXPORT bool Run(int argc, const char* const argv[], std::atomic<bool>& running, bool useSignalHandler);
    std::size_t m_totalBundlesErasedFromStorage;
    std::size_t m_totalBundlesSentToEgressFromStorage;

    STORAGE_LIB_EXPORT std::size_t GetCurrentNumberOfBundlesDeletedFromStorage();

private:
    void MonitorExitKeypressThreadFunction();
    std::atomic<bool> m_runningFromSigHandler;
};


#endif //_STORAGE_RUNNER_H

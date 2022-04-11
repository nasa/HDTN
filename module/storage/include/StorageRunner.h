/**
 * @file StorageRunner.h
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
 *
 * @section DESCRIPTION
 *
 * This StorageRunner class is used for launching just the HDTN storage module into its own process.
 */

#ifndef _STORAGE_RUNNER_H
#define _STORAGE_RUNNER_H 1

#include <stdint.h>
#include "ZmqStorageInterface.h"
#include "Logger.h"

class StorageRunner {
public:
    StorageRunner();
    ~StorageRunner();
    bool Run(int argc, const char* const argv[], volatile bool & running, bool useSignalHandler);
    std::size_t m_totalBundlesErasedFromStorage;
    std::size_t m_totalBundlesSentToEgressFromStorage;

    std::size_t GetCurrentNumberOfBundlesDeletedFromStorage();

private:
    void MonitorExitKeypressThreadFunction();
    std::unique_ptr<ZmqStorageInterface> m_storagePtr;
    volatile bool m_runningFromSigHandler;
};


#endif //_STORAGE_RUNNER_H

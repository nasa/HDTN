/**
 * @file BpSendFileRunner.h
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
 * This BpSendFileRunner class is used for launching the BpSendFile class into its own process.
 * The BpSendFileRunner provides a blocking Run function which creates and
 * initializes a BpSendFile object by processing/using the various command line arguments.
 * BpSendFileRunner also provides a signal handler listener to capture Ctrl+C (SIGINT) events
 * for clean termination.
 */

#ifndef _BP_SEND_FILE_RUNNER_H
#define _BP_SEND_FILE_RUNNER_H 1

#include <stdint.h>
#include "BpSendFile.h"
#include <atomic>

class BpSendFileRunner {
public:
    BpSendFileRunner();
    ~BpSendFileRunner();
    bool Run(int argc, const char* const argv[], std::atomic<bool>& running, bool useSignalHandler);
    uint64_t m_bundleCount;
    uint64_t m_totalBundlesAcked;

    OutductFinalStats m_outductFinalStats;


private:
    void MonitorExitKeypressThreadFunction();

    std::atomic<bool> m_runningFromSigHandler;
};


#endif //_BP_SEND_FILE_RUNNER_H

/**
 * @file BPingRunner.h
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
 * This BPingRunner class is used for launching the BPing class into its own process.
 * The BPingRunner provides a blocking Run function which creates and
 * initializes a BPing object by processing/using the various command line arguments.
 * BPingRunner also provides a signal handler listener to capture Ctrl+C (SIGINT) events
 * for clean termination.
 */

#ifndef _BPING_RUNNER_H
#define _BPING_RUNNER_H 1

#include <stdint.h>
#include "BPing.h"
#include <atomic>

class BPingRunner {
public:
    BPingRunner();
    ~BPingRunner();
    bool Run(int argc, const char* const argv[], std::atomic<bool>& running, bool useSignalHandler);

private:
    void MonitorExitKeypressThreadFunction();

    std::atomic<bool> m_runningFromSigHandler;
};


#endif //_BPING_RUNNER_H

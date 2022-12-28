/**
 * @file BpGenAsyncRunner.h
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
 * This BpGenAsyncRunner class is used for launching the BpGenAsync class into its own process.
 * The BpGenAsyncRunner provides a blocking Run function which creates and
 * initializes a BpGenAsync object by processing/using the various command line arguments.
 * BpGenAsyncRunner also provides a signal handler listener to capture Ctrl+C (SIGINT) events
 * for clean termination.
 */

#ifndef _BPGEN_ASYNC_RUNNER_H
#define _BPGEN_ASYNC_RUNNER_H 1

#include <stdint.h>
#include "BpGenAsync.h"


class BpGenAsyncRunner {
public:
    BpGenAsyncRunner();
    ~BpGenAsyncRunner();
    bool Run(int argc, const char* const argv[], volatile bool & running, bool useSignalHandler);
    uint64_t m_bundleCount;
    uint64_t m_totalBundlesAcked;

    OutductFinalStats m_outductFinalStats;


private:
    void MonitorExitKeypressThreadFunction();

    volatile bool m_runningFromSigHandler;
};


#endif //_BPGEN_ASYNC_RUNNER_H

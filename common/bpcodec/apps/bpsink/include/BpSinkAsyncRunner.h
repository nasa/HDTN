/**
 * @file BpSinkAsyncRunner.h
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
 * This BpSinkAsyncRunner class is used for launching the BpSinkAsync class into its own process.
 * The BpSinkAsyncRunner provides a blocking Run function which creates and
 * initializes a BpSinkAsync object by processing/using the various command line arguments.
 * BpSinkAsyncRunner also provides a signal handler listener to capture Ctrl+C (SIGINT) events
 * for clean termination.
 */

#ifndef _BPSINK_ASYNC_RUNNER_H
#define _BPSINK_ASYNC_RUNNER_H 1

#include <stdint.h>
#include "BpSinkAsync.h"


class BpSinkAsyncRunner {
public:
    BpSinkAsyncRunner();
    ~BpSinkAsyncRunner();
    bool Run(int argc, const char* const argv[], volatile bool & running, bool useSignalHandler);
    uint64_t m_totalBytesRx;
    uint64_t m_receivedCount;
    uint64_t m_duplicateCount;
    FinalStatsBpSink m_FinalStatsBpSink;

private:
    void MonitorExitKeypressThreadFunction();

    volatile bool m_runningFromSigHandler;
};


#endif //_BPSINK_ASYNC_RUNNER_H

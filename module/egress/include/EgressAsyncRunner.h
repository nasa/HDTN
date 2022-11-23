/**
 * @file EgressAsyncRunner.h
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
 * This EgressAsyncRunner class is used for launching just the HDTN Egress module into its own process.
 * The EgressAsyncRunner provides a blocking Run function which creates and
 * initializes an Egress object by processing/using the various command line arguments.
 * EgressAsyncRunner is only used when running HDTN in distributed mode in which there
 * is a single process dedicated to Egress.
 * EgressAsyncRunner also provides a signal handler listener to capture Ctrl+C (SIGINT) events
 * for clean termination.
 */

#ifndef _EGRESS_ASYNC_RUNNER_H
#define _EGRESS_ASYNC_RUNNER_H 1

#include <stdint.h>
#include "Logger.h"
#include "egress_async_lib_export.h"

class EgressAsyncRunner {
public:
    EGRESS_ASYNC_LIB_EXPORT EgressAsyncRunner();
    EGRESS_ASYNC_LIB_EXPORT ~EgressAsyncRunner();
    EGRESS_ASYNC_LIB_EXPORT bool Run(int argc, const char* const argv[], volatile bool & running, bool useSignalHandler);
    uint64_t m_bundleCount;
    uint64_t m_bundleData;
    uint64_t m_messageCount;

private:
    EGRESS_ASYNC_LIB_NO_EXPORT void MonitorExitKeypressThreadFunction();

    volatile bool m_runningFromSigHandler;
};


#endif //_EGRESS_ASYNC_RUNNER_H

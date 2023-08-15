/**
 * @file IngressAsyncRunner.h
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
 * This IngressAsyncRunner class is used for launching just the HDTN Ingress module into its own process.
 * The IngressAsyncRunner provides a blocking Run function which creates and
 * initializes an Ingress object by processing/using the various command line arguments.
 * IngressAsyncRunner is only used when running HDTN in distributed mode in which there
 * is a single process dedicated to Ingress.
 * IngressAsyncRunner also provides a signal handler listener to capture Ctrl+C (SIGINT) events
 * for clean termination.
 */

#ifndef _INGRESS_ASYNC_RUNNER_H
#define _INGRESS_ASYNC_RUNNER_H 1

#include <stdint.h>
#include "ingress_async_lib_export.h"
#include <atomic>

class IngressAsyncRunner {
public:
    INGRESS_ASYNC_LIB_EXPORT IngressAsyncRunner();
    INGRESS_ASYNC_LIB_EXPORT ~IngressAsyncRunner();
    INGRESS_ASYNC_LIB_EXPORT bool Run(int argc, const char* const argv[], std::atomic<bool>& running, bool useSignalHandler);
    uint64_t m_bundleCountStorage;
    uint64_t m_bundleCountEgress;
    uint64_t m_bundleCount;
    uint64_t m_bundleData;

private:
    INGRESS_ASYNC_LIB_NO_EXPORT void MonitorExitKeypressThreadFunction();

    std::atomic<bool> m_runningFromSigHandler;
};


#endif //_INGRESS_ASYNC_RUNNER_H

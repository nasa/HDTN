/**
 * @file RouterRunner.h
 * @author Nadia Kortas <nadia.kortas@nasa.gov>
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
 * This RouterRunner class is used for launching just the HDTN Router module into its own process.
 * The RouterRunner provides a blocking Run function which creates and
 * initializes a Router object by processing/using the various command line arguments.
 * RouterRunner is only used when running HDTN in distributed mode in which there
 * is a single process dedicated to Router.
 * RouterRunner also provides a signal handler listener to capture Ctrl+C (SIGINT) events
 * for clean termination.
 */

#ifndef _ROUTER_RUNNER_H
#define _ROUTER_RUNNER_H 1

#include <stdint.h>
#include "Logger.h"
#include "router_lib_export.h"

class RouterRunner {
public:
    ROUTER_LIB_EXPORT RouterRunner();
    ROUTER_LIB_EXPORT ~RouterRunner();
    ROUTER_LIB_EXPORT bool Run(int argc, const char* const argv[], volatile bool & running, bool useSignalHandler);

private:
    ROUTER_LIB_NO_EXPORT void MonitorExitKeypressThreadFunction();

    volatile bool m_runningFromSigHandler;
};


#endif //_ROUTER_RUNNER_H

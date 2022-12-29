/**
 * @file SchedulerRunner.h
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
 * This SchedulerRunner class is used for launching just the HDTN Scheduler module into its own process.
 * The SchedulerRunner provides a blocking Run function which creates and
 * initializes a Scheduler object by processing/using the various command line arguments.
 * SchedulerRunner is only used when running HDTN in distributed mode in which there
 * is a single process dedicated to Scheduler.
 * SchedulerRunner also provides a signal handler listener to capture Ctrl+C (SIGINT) events
 * for clean termination.
 */

#ifndef _SCHEDULER_RUNNER_H
#define _SCHEDULER_RUNNER_H 1

#include <stdint.h>
#include "Logger.h"
#include "scheduler_lib_export.h"

class SchedulerRunner {
public:
    SCHEDULER_LIB_EXPORT SchedulerRunner();
    SCHEDULER_LIB_EXPORT ~SchedulerRunner();
    SCHEDULER_LIB_EXPORT bool Run(int argc, const char* const argv[], volatile bool & running, bool useSignalHandler);

private:
    SCHEDULER_LIB_NO_EXPORT void MonitorExitKeypressThreadFunction();

    volatile bool m_runningFromSigHandler;
};


#endif //_SCHEDULER_RUNNER_H

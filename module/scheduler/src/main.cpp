/**
 * @file main.cpp
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
 * This file provides the "int main()" function to wrap SchedulerRunner
 * and forward command line arguments to SchedulerRunner.
 * This file is only used when running HDTN in distributed mode in which there
 * is a single process dedicated to the Scheduler module.
 */

#include "SchedulerRunner.h"
#include "Logger.h"

int main(int argc, char *argv[]) {
    hdtn::Logger::initializeWithProcess(hdtn::Logger::Process::scheduler);
    SchedulerRunner runner;
    volatile bool running;
    runner.Run(argc, argv, running, true);
    return 0;
}

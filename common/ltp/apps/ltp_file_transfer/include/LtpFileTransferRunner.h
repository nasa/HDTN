/**
 * @file LtpFileTransferRunner.h
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
 * This LtpFileTransferRunner class is used for launching an Ltp File Transfer test application into its own process.
 * The LtpFileTransferRunner provides a blocking Run function which creates and
 * initializes the test application by processing/using the various command line arguments.
 * LtpFileTransferRunner also provides a signal handler listener to capture Ctrl+C (SIGINT) events
 * for clean termination.
 */

#ifndef _BPGEN_ASYNC_RUNNER_H
#define _BPGEN_ASYNC_RUNNER_H 1

#include <stdint.h>
#include <atomic>

class LtpFileTransferRunner {
public:
    LtpFileTransferRunner();
    ~LtpFileTransferRunner();
    bool Run(int argc, const char* const argv[], std::atomic<bool>& running, bool useSignalHandler);
    
    //FinalStats m_FinalStats;


private:
    void MonitorExitKeypressThreadFunction();

    std::atomic<bool> m_runningFromSigHandler;
};


#endif //_BPGEN_ASYNC_RUNNER_H

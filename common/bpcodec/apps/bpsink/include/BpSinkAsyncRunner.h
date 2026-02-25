/**
 * @file BpSinkAsyncRunner.h
 * @author  Brian Tomko <brian.j.tomko@nasa.gov>
 *
 * @copyright Copyright (c) 2026 United States Government as represented by
 * the National Aeronautics and Space Administration.
 * No copyright is claimed in the United States under Title 17, U.S.Code.
 * All Other Rights Reserved.
 *
 * @section LICENSE
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
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
#include <atomic>

class BpSinkAsyncRunner {
public:
    BpSinkAsyncRunner();
    ~BpSinkAsyncRunner();
    bool Run(int argc, const char* const argv[], std::atomic<bool>& running, bool useSignalHandler);
    uint64_t m_totalBytesRx;
    uint64_t m_receivedCount;
    uint64_t m_duplicateCount;
    FinalStatsBpSink m_FinalStatsBpSink;

private:
    void MonitorExitKeypressThreadFunction();

    std::atomic<bool> m_runningFromSigHandler;
};


#endif //_BPSINK_ASYNC_RUNNER_H

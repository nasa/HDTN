/**
 * @file BpReceiveStreamRunner.h
 *
 * @copyright Copyright (c) 2021 United States Government as represented by
 * the National Aeronautics and Space Administration.
 * No copyright is claimed in the United States under Title 17, U.S.Code.
 * All Other Rights Reserved.
 *
 * @section LICENSE
 * Released under the NASA Open Source Agreement (NOSA)
 * See LICENSE.md in the source root directory for more information.
 */

#pragma once

#include "BpReceiveStream.h"


class BpReceiveStreamRunner {
public:
    BpReceiveStreamRunner();
    ~BpReceiveStreamRunner();
    bool Run(int argc, const char* const argv[], std::atomic<bool> & running, bool useSignalHandler);
    uint64_t m_totalBytesRx;

private:
    void MonitorExitKeypressThreadFunction();

    std::atomic<bool> m_runningFromSigHandler;
};



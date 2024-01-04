/**
 * @file BpSendPacketRunner.h
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
 * This BpSendPacketRunner class is used for launching the BpSendPacket class into its own process.
 * The BpSendPacketRunner provides a blocking Run function which creates and
 * initializes a BpSendPacket object by processing/using the various command line arguments.
 * BpSendPacketRunner also provides a signal handler listener to capture Ctrl+C (SIGINT) events
 * for clean termination.
 */

#ifndef _BP_SEND_PACKET_RUNNER_H
#define _BP_SEND_PACKET_RUNNER_H 1

#include <stdint.h>
#include "BpSendPacket.h"
#include <atomic>

class BpSendPacketRunner {
public:
    BpSendPacketRunner();
    ~BpSendPacketRunner();
    bool Run(int argc, const char* const argv[], std::atomic<bool>& running, bool useSignalHandler);
    uint64_t m_bundleCount;
    uint64_t m_totalBundlesAcked;

    OutductFinalStats m_outductFinalStats;


private:
    void MonitorExitKeypressThreadFunction();

    std::atomic<bool> m_runningFromSigHandler;
};


#endif //_BP_SEND_PACKET_RUNNER_H

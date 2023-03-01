/**
 * @file HdtnOneProcessRunner.h
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
 * This HdtnOneProcessRunner class is used for launching most of the HDTN modules/components into a single process.
 * The HdtnOneProcessRunner provides a blocking Run function which creates and
 * initializes the Ingress, Egress, Storage, and GUI modules/objects
 * by processing/using the various command line arguments.
 * HdtnOneProcessRunner is only used when running HDTN in single-process mode in which there
 * is a single process which contains most of HDTN (i.e. the Ingress, Egress, Storage, and GUI modules).
 * HdtnOneProcessRunner also provides a signal handler listener to capture Ctrl+C (SIGINT) events
 * for clean termination.
 */

#ifndef _HDTN_ONE_PROCESS_RUNNER_H
#define _HDTN_ONE_PROCESS_RUNNER_H 1

#include <stdint.h>


class HdtnOneProcessRunner {
public:
    HdtnOneProcessRunner();
    ~HdtnOneProcessRunner();
    bool Run(int argc, const char* const argv[], volatile bool & running, bool useSignalHandler);

    //ingress
    uint64_t m_ingressBundleCountStorage;
    uint64_t m_ingressBundleCountEgress;
    uint64_t m_ingressBundleCount;
    uint64_t m_ingressBundleData;

    //egress
    uint64_t m_egressTotalBundlesGivenToOutducts;
    uint64_t m_egressTotalBundleBytesGivenToOutducts;

    //storage
    std::size_t m_totalBundlesErasedFromStorage;
    std::size_t m_totalBundlesSentToEgressFromStorage;

private:
    

    void MonitorExitKeypressThreadFunction();

    volatile bool m_runningFromSigHandler;
};


#endif //_HDTN_ONE_PROCESS_RUNNER_H

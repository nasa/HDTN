/**
 * @file UdpDelaySimRunner.h
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
 * This UdpDelaySimRunner class processes command line args and spins up
 * a single instance of UdpDelaySim within its own process with ctrl-c interrupt support.
 */

#ifndef _UDP_DELAY_SIM_RUNNER_H
#define _UDP_DELAY_SIM_RUNNER_H 1

#include <stdint.h>
#include "udp_delay_sim_lib_export.h"

class UdpDelaySimRunner {
public:
    UDP_DELAY_SIM_LIB_EXPORT UdpDelaySimRunner();
    UDP_DELAY_SIM_LIB_EXPORT ~UdpDelaySimRunner();
    UDP_DELAY_SIM_LIB_EXPORT bool Run(int argc, const char* const argv[], volatile bool & running, bool useSignalHandler);
    
private:
    UDP_DELAY_SIM_LIB_NO_EXPORT void MonitorExitKeypressThreadFunction();

    volatile bool m_runningFromSigHandler;
};


#endif //_UDP_DELAY_SIM_RUNNER_H

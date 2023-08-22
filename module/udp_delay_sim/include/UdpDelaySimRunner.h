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
#include <atomic>

class UdpDelaySimRunner {
public:
    /// Default constructor
    UDP_DELAY_SIM_LIB_EXPORT UdpDelaySimRunner();
    /// Default destructor
    UDP_DELAY_SIM_LIB_EXPORT ~UdpDelaySimRunner();
    
    /** Run the delay simulation.
     *
     * @param argc The number of command-line arguments.
     * @param argv The array of command-line arguments.
     * @param running The simulation running flag.
     * @param useSignalHandler Whether to activate the signal handler.
     * @return True if the simulation exited cleanly, or False otherwise.
     */
    UDP_DELAY_SIM_LIB_EXPORT bool Run(int argc, const char* const argv[], std::atomic<bool>& running, bool useSignalHandler);
    
private:
    /** Set the exit flag from the signal handler.
     *
     * Forwards intent to exit by setting m_runningFromSigHandler to False due to exit keypress being caught by the signal handler.
     */
    UDP_DELAY_SIM_LIB_NO_EXPORT void MonitorExitKeypressThreadFunction();

    /// Signal handler flag, whether the simulation should keep running
    std::atomic<bool> m_runningFromSigHandler;
};


#endif //_UDP_DELAY_SIM_RUNNER_H

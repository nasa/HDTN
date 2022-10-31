/***************************************************************************
 * NASA Glenn Research Center, Cleveland, OH
 * Released under the NASA Open Source Agreement (NOSA)
 * May  2021
 *
 * Scheduler Main-
 ****************************************************************************
 */

#include "scheduler.h"
#include "Logger.h"

int main(int argc, char *argv[]) {
    hdtn::Logger::initializeWithProcess(hdtn::Logger::Process::scheduler);
    Scheduler scheduler;
    volatile bool running;
	
    int   returnCode = scheduler.Run(argc, argv, running, true); 

    return returnCode;
}

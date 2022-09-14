/***************************************************************************
 * NASA Glenn Research Center, Cleveland, OH
 * Released under the NASA Open Source Agreement (NOSA)
 * May  2021
 *
 * Scheduler Main-
 ****************************************************************************
 */

#include "scheduler.h"

int main(int argc, char *argv[]) {
    Scheduler scheduler;
    volatile bool running;
	
    int   returnCode = scheduler.Run(argc, argv, running, true); 

    return returnCode;
}

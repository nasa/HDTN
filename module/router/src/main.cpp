/***************************************************************************
 * NASA Glenn Research Center, Cleveland, OH
 * Released under the NASA Open Source Agreement (NOSA)
 * May  2021
 *
 * Routing  Main-
 ****************************************************************************
 */

#include "router.h"
#include "Logger.h"

int main(int argc, char *argv[]) {
    hdtn::Logger::initializeWithProcess(hdtn::Logger::Process::router);
    Router router;
    volatile bool running;
	
    int   returnCode = router.Run(argc, argv, running, true); 

    return returnCode;
}

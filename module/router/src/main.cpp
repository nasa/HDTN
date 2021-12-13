/***************************************************************************
 * NASA Glenn Research Center, Cleveland, OH
 * Released under the NASA Open Source Agreement (NOSA)
 * May  2021
 *
 * Routing  Main-
 ****************************************************************************
 */

#include "router.h"

int main(int argc, char *argv[]) {
    Router router;
    std::string jsonFileName;
    volatile bool running;
	
    int   returnCode = router.Run(argc, argv, running, jsonFileName, true); 

    return returnCode;
}

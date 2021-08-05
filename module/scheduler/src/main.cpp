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
    std::string jsonFileName;
    int returnCode = scheduler.ProcessComandLine(argc,(const char **)argv,jsonFileName);
    if (returnCode == 0) {
        returnCode = scheduler.ProcessContactsFile(jsonFileName);
    }
    return returnCode;
}

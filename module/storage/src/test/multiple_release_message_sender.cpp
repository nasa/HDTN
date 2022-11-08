/**
 * @file multiple_release_message_sender.h
 * @author  Jeff Follo
 *
 * @copyright Copyright © 2021 United States Government as represented by
 * the National Aeronautics and Space Administration.
 * No copyright is claimed in the United States under Title 17, U.S.Code.
 * All Other Rights Reserved.
 *
 * @section LICENSE
 * Released under the NASA Open Source Agreement (NOSA)
 * See LICENSE.md in the source root directory for more information.
 */

#include "ReleaseSender.h"
#include "Logger.h"

int main(int argc, char *argv[]) {
    hdtn::Logger::initializeWithProcess(hdtn::Logger::Process::releasemessagesender);
    ReleaseSender releaseSender;
    std::string jsonFileName;
    int returnCode = releaseSender.ProcessComandLine(argc,(const char **)argv,jsonFileName);
    if (returnCode == 0) {
        returnCode = releaseSender.ProcessEventFile(jsonFileName);
    }
    return returnCode;
}

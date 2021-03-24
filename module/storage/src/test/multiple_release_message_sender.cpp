#include "ReleaseSender.h"

int main(int argc, char *argv[]) {
    ReleaseSender releaseSender;
    std::string jsonFileName;
    int returnCode = releaseSender.ProcessComandLine(argc,argv,jsonFileName);
    if (returnCode == 0) {
        returnCode = releaseSender.ProcessEventFile(jsonFileName);
    }
    return returnCode;
}

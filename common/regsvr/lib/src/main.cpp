#include <iostream>

#include "reg.hpp"

int main(int argc, char* argv[]) {
    hdtn::hdtn_regsvr regsvr;
    regsvr.init("tcp://127.0.0.1:10140", "test", 10141, "PUSH");
    regsvr.reg();
    hdtn::hdtn_regsvr regsvr2;
    regsvr2.init("tcp://127.0.0.1:10140", "test", 10142, "PUSH");
    regsvr2.reg();
    hdtn::hdtn_entries res = regsvr.query();
    for (auto entry : res) {
        std::cout << entry.address << ":" << entry.port << ":" << entry.mode << std::endl;
    }
    regsvr.dereg();
    regsvr2.dereg();
    return 0;
}

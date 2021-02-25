#include <iostream>

#include "reg.hpp"

int main(int argc, char* argv[]) {
    hdtn::HdtnRegsvr regsvr;
    regsvr.Init("tcp://127.0.0.1:10140", "test", 10141, "PUSH");
    regsvr.Reg();
    hdtn::HdtnRegsvr regsvr2;
    regsvr2.Init("tcp://127.0.0.1:10140", "test", 10142, "PUSH");
    regsvr2.Reg();

    if(hdtn::HdtnEntries_ptr res = regsvr.Query()) {
        const hdtn::HdtnEntryList_t & entryList = res->m_hdtnEntryList;
        for (hdtn::HdtnEntryList_t::const_iterator it = entryList.cbegin(); it != entryList.cend(); ++it) {
            const hdtn::HdtnEntry & entry = *it;
            std::cout << entry.address << ":" << entry.port << ":" << entry.mode << std::endl;
        }
    }
    else {
        std::cerr << "error: null query" << std::endl;
    }
    regsvr.Dereg();
    regsvr2.Dereg();
    return 0;
}

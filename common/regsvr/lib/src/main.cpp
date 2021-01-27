#include <iostream>
#include "reg.hpp"

int main(int argc, char* argv[]) {
  hdtn::HdtnRegsvr regsvr;
  regsvr.Init("tcp://127.0.0.1:10140", "test", 10141, "PUSH");
  regsvr.Reg();
  hdtn::HdtnRegsvr regsvr2;
  regsvr2.Init("tcp://127.0.0.1:10140", "test", 10142, "PUSH");
  regsvr2.Reg();
  hdtn::hdtn_entries res = regsvr.Query();
  for (auto entry : res) {
    std::cout << entry.address << ":" << entry.port << ":" << entry.mode
              << std::endl;
  }
  regsvr.Dereg();
  regsvr2.Dereg();
  return 0;
}

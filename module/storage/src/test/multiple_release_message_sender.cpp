#include <boost/asio.hpp>
#include <boost/thread.hpp>
#include <boost/program_options.hpp>
#include <boost/algorithm/string.hpp>
#include <cstdlib>
#include <iostream>

#include "message.hpp"
#include "paths.hpp"
#include "reg.hpp"
#include "Environment.h"


int main(int argc, char *argv[]) {

    std::string jsonFileName = (Environment::GetPathHdtnSourceRoot() /
      "module/storage/src/test/releaseMessages1.json").string();


    std::cout << "Running muliple_release_message_sender.  " << std::endl << std::flush;
    std::cout << "jsonFileName:  " << jsonFileName << std::endl << std::flush;

    return 0;
}

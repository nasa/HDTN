#include "logging.hpp"
#include <string>

namespace hdtn3{
    std::string datetime() {
        time_t rawtime;
        struct tm * timeinfo;
        char buffer[80];

        time (&rawtime);
        timeinfo = localtime(&rawtime);

        strftime(buffer,80,"%d-%m-%Y-%H:%M:%S",timeinfo);
        return std::string(buffer);
    }
}

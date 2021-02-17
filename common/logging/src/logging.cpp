#include "logging.hpp"

#include <string>

namespace hdtn {

std::string Datetime() {
#ifndef  _WIN32



    time_t rawtime;
    struct tm* timeinfo;
    char buffer[80];

    time(&rawtime);
    timeinfo = localtime(&rawtime);

    strftime(buffer, 80, "%d-%m-%Y-%H:%M:%S", timeinfo);
    return std::string(buffer);
#else
	return "";
#endif // ! _WIN32
}
}  // namespace hdtn

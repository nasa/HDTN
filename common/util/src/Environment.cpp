#include <Environment.h>

std::string Environment::GetValue(const std::string & variableName) {
    std::string value = "";
    if (const char * const variableValue = std::getenv(variableName.c_str())) {
        value = variableValue;
    }
    return value;
}

boost::filesystem::path Environment::GetPathHdtnSourceRoot() {
    std::string hdtnSourceRoot = Environment::GetValue("HDTN_SOURCE_ROOT");
    return boost::filesystem::path(hdtnSourceRoot);
}

boost::filesystem::path Environment::GetPathHdtnBuildRoot() {
    std::string hdtnSourceRoot = Environment::GetValue("HDTN_BUILD_ROOT");
    return boost::filesystem::path(hdtnSourceRoot);
}



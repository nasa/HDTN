#ifndef ENVIRONMENT_H
#define ENVIRONMENT_H

#include <cstdlib>
#include <string>
#include <boost/filesystem.hpp>
#include "hdtn_util_export.h"

class HDTN_UTIL_EXPORT Environment {
protected:
    Environment() = default;
    virtual ~Environment() = default;
public:
    static std::string GetValue(const std::string & variableName);
    static boost::filesystem::path GetPathHdtnSourceRoot();
    static boost::filesystem::path GetPathHdtnBuildRoot();
};

#endif /* ENVIRONMENT_H */


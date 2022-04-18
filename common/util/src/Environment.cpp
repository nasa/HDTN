/**
 * @file Environment.cpp
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



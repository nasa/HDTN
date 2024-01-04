/**
 * @file Environment.h
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
 *
 * @section DESCRIPTION
 *
 * This Environment static class is a utility used to get system environmental variables,
 * including HDTN_SOURCE_ROOT and HDTN_BUILD_ROOT.
 */

#ifndef ENVIRONMENT_H
#define ENVIRONMENT_H 1

#include <cstdlib>
#include <string>
#include <boost/filesystem.hpp>
#include "hdtn_util_export.h"

class HDTN_UTIL_EXPORT Environment {
protected:
    Environment() = delete;
public:
    static std::string GetValue(const std::string & variableName);
    static boost::filesystem::path GetPathHdtnSourceRoot();
    static boost::filesystem::path GetPathHdtnBuildRoot();
    static boost::filesystem::path GetPathContactPlans();
    static boost::filesystem::path GetPathGuiDocumentRoot();
};

#endif /* ENVIRONMENT_H */


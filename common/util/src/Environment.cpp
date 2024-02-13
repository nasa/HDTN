/**
 * @file Environment.cpp
 * @author  Jeff Follo
 *
 * @copyright Copyright (c) 2021 United States Government as represented by
 * the National Aeronautics and Space Administration.
 * No copyright is claimed in the United States under Title 17, U.S.Code.
 * All Other Rights Reserved.
 *
 * @section LICENSE
 * Released under the NASA Open Source Agreement (NOSA)
 * See LICENSE.md in the source root directory for more information.
 */

#include <Environment.h>

#ifndef INSTALL_DATA_DIR
#warning "INSTALL_DATA_DIR not set, using /usr/local/share"
#define INSTALL_DATA_DIR /usr/local/share
#endif
#define str_(s) #s
#define str(s) str_(s)

static const std::string InstallDataDir{str(INSTALL_DATA_DIR)};

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

boost::filesystem::path Environment::GetPathContactPlans() {
    if (const char * const s = std::getenv("HDTN_SOURCE_ROOT")) {
        return boost::filesystem::path(s) / "module/router/contact_plans";
    } else {
        return boost::filesystem::path(InstallDataDir) / "contact_plans";
    }
}

boost::filesystem::path Environment::GetPathGuiDocumentRoot() {
    if (const char * const s = std::getenv("HDTN_SOURCE_ROOT")) {
        return boost::filesystem::path(s) / "module/telem_cmd_interface/src/gui";
    } else {
        return boost::filesystem::path(InstallDataDir) / "gui";
    }
}

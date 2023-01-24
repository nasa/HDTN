/**
 * @file TelemetryRunnerProgramOptions.cpp
 *
 * @copyright Copyright © 2023 United States Government as represented by
 * the National Aeronautics and Space Administration.
 * No copyright is claimed in the United States under Title 17, U.S.Code.
 * All Other Rights Reserved.
 *
 * @section LICENSE
 * Released under the NASA Open Source Agreement (NOSA)
 * See LICENSE.md in the source root directory for more information.
 *
 */

#include <boost/filesystem.hpp>

#include "TelemetryRunnerProgramOptions.h"
#include "Environment.h"
#include "Logger.h"


static constexpr hdtn::Logger::SubProcess subprocess = hdtn::Logger::SubProcess::telem;
static const boost::filesystem::path GUI_HTML_FILE_NAME = "web_gui.html";


TelemetryRunnerProgramOptions::TelemetryRunnerProgramOptions()
    : m_guiDocumentRoot(""), m_guiPortNumber("")
{
}

bool TelemetryRunnerProgramOptions::ParseFromVariableMap(boost::program_options::variables_map& vm)
{
#ifdef USE_WEB_INTERFACE
    m_guiDocumentRoot = GetDocumentRootAndValidate(vm);
    m_guiPortNumber = GetPortNumberAsString(vm);
    if (m_guiDocumentRoot == "" || m_guiPortNumber == "")
    {
        return false;
    }
#endif
    return true;
}

void TelemetryRunnerProgramOptions::AppendToDesc(boost::program_options::options_description& desc)
{
#ifdef USE_WEB_INTERFACE
    desc.add_options()("document-root", boost::program_options::value<boost::filesystem::path>()->default_value((Environment::GetPathHdtnSourceRoot() / "module" / "telem_cmd_interface" / "src" / "gui").string()), "Document Root.")("port-number", boost::program_options::value<uint16_t>()->default_value(8086), "Port number.");
#endif
}

boost::filesystem::path TelemetryRunnerProgramOptions::GetDocumentRootAndValidate(boost::program_options::variables_map& vm)
{
    try
    {
        const boost::program_options::variable_value val = vm["document-root"];
        if (val.empty()) {
            return "";
        }
        const boost::filesystem::path documentRoot = val.as<boost::filesystem::path>();
        const boost::filesystem::path htmlMainFilePath = documentRoot / GUI_HTML_FILE_NAME;
        if (boost::filesystem::is_regular_file(htmlMainFilePath))
        {
            return documentRoot;
        }
        else
        {
            LOG_FATAL(subprocess) << "Cannot find " << htmlMainFilePath << " : make sure document-root is set properly";
        }
    }
    catch (boost::bad_any_cast &e)
    {
        LOG_FATAL(subprocess) << "invalid program option error: " << e.what();
    }
    return "";
}

std::string TelemetryRunnerProgramOptions::GetPortNumberAsString(boost::program_options::variables_map& vm)
{
    try
    {
        const boost::program_options::variable_value val = vm["port-number"];
        if (val.empty()) {
            return "";
        }
        const uint16_t portNumber = val.as<uint16_t>();
        return boost::lexical_cast<std::string>(portNumber);
    }
    catch (boost::bad_any_cast &e)
    {
        LOG_FATAL(subprocess) << "invalid program options error: " << e.what();
        return "";
    }
}

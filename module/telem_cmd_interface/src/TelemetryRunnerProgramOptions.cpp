/**
 * @file TelemetryRunnerProgramOptions.cpp
 *
 * @copyright Copyright (c) 2023 United States Government as represented by
 * the National Aeronautics and Space Administration.
 * No copyright is claimed in the United States under Title 17, U.S.Code.
 * All Other Rights Reserved.
 *
 * @section LICENSE
 * Released under the NASA Open Source Agreement (NOSA)
 * See LICENSE.md in the source root directory for more information.
 *
 */

#include "TelemetryRunnerProgramOptions.h"
#include "Environment.h"
#include "Logger.h"
#include <boost/filesystem/operations.hpp>

static constexpr hdtn::Logger::SubProcess subprocess = hdtn::Logger::SubProcess::telem;
static const boost::filesystem::path GUI_HTML_FILE_NAME = "index.html";


TelemetryRunnerProgramOptions::TelemetryRunnerProgramOptions()
    : m_guiDocumentRoot(""), m_guiPortNumber("")
{
}

bool TelemetryRunnerProgramOptions::ParseFromVariableMap(boost::program_options::variables_map& vm)
{
#ifdef USE_WEB_INTERFACE
    m_guiDocumentRoot = GetDocumentRootAndValidate(vm);
    m_guiPortNumber = GetPortNumberAsString(vm);
    m_hdtnDistributedConfigPtr = GetHdtnDistributedConfigPtr(vm); //could be null if not distributed
    if (m_guiDocumentRoot == "" || m_guiPortNumber == "") {
        return false;
    }
# ifdef BEAST_WEBSOCKET_SERVER_SUPPORT_SSL
    GetSslPathsAndValidate(vm, m_sslPaths);
# endif
#else
    (void)vm;
#endif
    return true;
}

void TelemetryRunnerProgramOptions::AppendToDesc(boost::program_options::options_description& desc)
{
#ifdef USE_WEB_INTERFACE
    desc.add_options()
        ("document-root", boost::program_options::value<boost::filesystem::path>()->default_value(Environment::GetPathGuiDocumentRoot()), "Document Root.")
        ("port-number", boost::program_options::value<uint16_t>()->default_value(8086), "Port number.")
# ifdef BEAST_WEBSOCKET_SERVER_SUPPORT_SSL
        ("gui-certificate-pem-file", boost::program_options::value<boost::filesystem::path>()->default_value(""), "GUI Server certificate file in PEM format (not preferred)")
        ("gui-certificate-chain-pem-file", boost::program_options::value<boost::filesystem::path>()->default_value(""), "GUI Server certificate chain file in PEM format (preferred)")
        ("gui-private-key-pem-file", boost::program_options::value<boost::filesystem::path>()->default_value(""), "GUI Server certificate file in PEM format")
        ("gui-dh-pem-file", boost::program_options::value<boost::filesystem::path>()->default_value(""), "GUI Server Diffie Hellman parameters file in PEM format")
# endif
        ;
#else
    (void)desc; //unused parameter
#endif
}

boost::filesystem::path TelemetryRunnerProgramOptions::GetDocumentRootAndValidate(boost::program_options::variables_map& vm) {
    try {
        const boost::program_options::variable_value val = vm["document-root"];
        if (val.empty()) {
            return "";
        }
        const boost::filesystem::path documentRoot = val.as<boost::filesystem::path>();
        const boost::filesystem::path htmlMainFilePath = documentRoot / GUI_HTML_FILE_NAME;
        if (boost::filesystem::is_regular_file(htmlMainFilePath)) {
            return documentRoot;
        }
        else {
            LOG_FATAL(subprocess) << "Cannot find " << htmlMainFilePath << " : make sure document-root is set properly";
        }
    }
    catch (boost::bad_any_cast &e) {
        LOG_FATAL(subprocess) << "invalid program option error: " << e.what();
    }
    return "";
}

std::string TelemetryRunnerProgramOptions::GetPortNumberAsString(boost::program_options::variables_map& vm) {
    try {
        const boost::program_options::variable_value val = vm["port-number"];
        if (val.empty()) {
            return "";
        }
        const uint16_t portNumber = val.as<uint16_t>();
        return boost::lexical_cast<std::string>(portNumber);
    }
    catch (boost::bad_any_cast &e) {
        LOG_FATAL(subprocess) << "invalid program options error: " << e.what();
        return "";
    }
}

HdtnDistributedConfig_ptr TelemetryRunnerProgramOptions::GetHdtnDistributedConfigPtr(boost::program_options::variables_map& vm) {
    HdtnDistributedConfig_ptr hdtnDistributedConfig;
    if (vm.count("hdtn-distributed-config-file")) {
        const boost::filesystem::path distributedConfigFileName = vm["hdtn-distributed-config-file"].as<boost::filesystem::path>();
        hdtnDistributedConfig = HdtnDistributedConfig::CreateFromJsonFilePath(distributedConfigFileName);
        if (!hdtnDistributedConfig) {
            LOG_ERROR(subprocess) << "error loading HDTN distributed config file: " << distributedConfigFileName;
        }
    }
    return hdtnDistributedConfig;
}

#ifdef BEAST_WEBSOCKET_SERVER_SUPPORT_SSL
void TelemetryRunnerProgramOptions::GetSslPathsAndValidate(boost::program_options::variables_map& vm, SslPaths& sslPaths) {
    sslPaths.m_valid = false;
    try {
        sslPaths.m_certificatePemFile = vm["gui-certificate-pem-file"].as<boost::filesystem::path>();
        sslPaths.m_certificateChainPemFile = vm["gui-certificate-chain-pem-file"].as<boost::filesystem::path>();
        sslPaths.m_privateKeyPemFile = vm["gui-private-key-pem-file"].as<boost::filesystem::path>();
        sslPaths.m_diffieHellmanParametersPemFile = vm["gui-dh-pem-file"].as<boost::filesystem::path>();
        if (sslPaths.m_certificatePemFile.size() && sslPaths.m_certificateChainPemFile.size()) {
            LOG_FATAL(subprocess) << "gui-certificate-pem-file and gui-certificate-chain-pem-file cannot both be specified";
        }
        else {
            sslPaths.m_valid = (sslPaths.m_certificatePemFile.size() || sslPaths.m_certificateChainPemFile.size())
                && sslPaths.m_privateKeyPemFile.size()
                && sslPaths.m_diffieHellmanParametersPemFile.size();
        }
    }
    catch (boost::bad_any_cast& e) {
        LOG_FATAL(subprocess) << "invalid program option error: " << e.what();
    }
}
#endif

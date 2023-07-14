/**
 * @file Telemetry.cpp
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

#include "Telemetry.h"
#include "TelemetryDefinitions.h"
#include "TelemetryRunner.h"
#include "TelemetryRunnerProgramOptions.h"
#include "Logger.h"
#include "SignalHandler.h"


static constexpr hdtn::Logger::SubProcess subprocess = hdtn::Logger::SubProcess::telem;


Telemetry::Telemetry()
    : m_runningFromSigHandler(false)
{}

bool Telemetry::Run(int argc, const char *const argv[], volatile bool &running)
{
    running = true;

    boost::program_options::options_description desc("Allowed options");
    TelemetryRunnerProgramOptions options;
    HdtnConfig_ptr hdtnConfig;
    try {
        desc.add_options()("help", "Produce help message.");
        TelemetryRunnerProgramOptions::AppendToDesc(desc);
        desc.add_options() //TODO should this be added here
            ("hdtn-config-file", boost::program_options::value<boost::filesystem::path>()->default_value("hdtn.json"), "HDTN Configuration File.")
            ("hdtn-distributed-config-file", boost::program_options::value<boost::filesystem::path>()->default_value("hdtn_distributed.json"), "HDTN Distributed Mode Configuration File.");
        boost::program_options::variables_map vm;
        boost::program_options::store(
            boost::program_options::parse_command_line(argc, argv, desc, boost::program_options::command_line_style::unix_style | boost::program_options::command_line_style::case_insensitive),
            vm);
        boost::program_options::notify(vm);
        if (vm.count("help")) {
            std::cout << desc << "\n";
            return false;
        }
        if (!options.ParseFromVariableMap(vm)) {
            return false;
        }

        const boost::filesystem::path configFileName = vm["hdtn-config-file"].as<boost::filesystem::path>();
        hdtnConfig = HdtnConfig::CreateFromJsonFilePath(configFileName);
        if (!hdtnConfig) {
            LOG_ERROR(subprocess) << "error loading config file: " << configFileName;
            return false;
        }
    }
    catch (boost::bad_any_cast & e) {
            LOG_ERROR(subprocess) << "invalid data error: " << e.what();
            LOG_ERROR(subprocess) << desc;
            return false;
    }
    catch (std::exception& e) {
            LOG_ERROR(subprocess) << "error: " << e.what();
            return false;
    }
    catch (...) {
            LOG_ERROR(subprocess) << "Exception of unknown type!";
            return false;
    }

    TelemetryRunner telemetryRunner;
    if (!telemetryRunner.Init(*hdtnConfig, NULL, options)) {
        return false;
    }

    m_runningFromSigHandler = true;
    SignalHandler sigHandler(boost::bind(&Telemetry::MonitorExitKeypressThreadFunc, this));
    sigHandler.Start(false);
    while (running && m_runningFromSigHandler)
    {
        boost::this_thread::sleep(boost::posix_time::millisec(250));
        sigHandler.PollOnce();
    }
    telemetryRunner.Stop();
    return true;
}

void Telemetry::MonitorExitKeypressThreadFunc()
{
    LOG_INFO(subprocess) << "Keyboard Interrupt.. exiting\n";
    m_runningFromSigHandler = false;
}

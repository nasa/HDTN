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
    desc.add_options()("help", "Produce help message.");
    TelemetryRunnerProgramOptions::AppendToDesc(desc);
    boost::program_options::variables_map vm;
    boost::program_options::store(
        boost::program_options::parse_command_line(argc, argv, desc, boost::program_options::command_line_style::unix_style | boost::program_options::command_line_style::case_insensitive),
        vm);
    boost::program_options::notify(vm);
    if (vm.count("help")) {
        std::cout << desc << "\n";
        return false;
    }
    TelemetryRunnerProgramOptions options;
    if (!options.ParseFromVariableMap(vm)) {
        return false;
    }

    TelemetryRunner telemetryRunner;
    if (!telemetryRunner.Init(NULL, options)) {
        return false;
    }

    m_runningFromSigHandler = true;
    SignalHandler sigHandler(boost::bind(&Telemetry::MonitorExitKeypressThreadFunc, this));
    sigHandler.Start(false);
    while (running && m_runningFromSigHandler && !telemetryRunner.ShouldExit())
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

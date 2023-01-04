/***************************************************************************
 * NASA Glenn Research Center, Cleveland, OH
 * Released under the NASA Open Source Agreement (NOSA)
 * May  2021
 *
 ****************************************************************************
 */

#include "Logger.h"

/**
 * Contains string values that correspond to the "Process" enum
 * in Logger.h. If a new process is added, a string representation
 * should be added here.
 */
static const std::string process_strings[static_cast<unsigned int>(hdtn::Logger::Process::none) + 1] =
{
    "bpgen",
    "bping",
    "bpreceivefile",
    "bpsendfile",
    "bpsink",
    "ltpfiletransfer",
    "egress",
    "telem",
    "hdtn",
    "ingress",
    "router",
    "scheduler",
    "storage",
    "releasemessagesender",
    "storagespeedtest",
    "udpdelaysim",
    "unittest",
    ""
};

/**
 * Contains string values that correspond to the "SubProcess" enum
 * in Logger.h. If a new sub-process is added, a string representation
 * should be added here.
 */
static const std::string subprocess_strings[static_cast<unsigned int>(hdtn::Logger::SubProcess::none) + 1] =
{
    "egress",
    "ingress",
    "router",
    "scheduler",
    "storage",
    "telem",
    "gui",
    "unittest",
    ""
};

static const uint32_t file_rotation_size = 5 * 1024 * 1024; //5 MiB
static const uint8_t console_message_offset = 9;

// Namespaces recommended by the Boost log library
namespace logging = boost::log;
namespace src = boost::log::sources;
namespace expr = boost::log::expressions;
namespace sinks = boost::log::sinks;
namespace attrs = boost::log::attributes;
namespace keywords = boost::log::keywords;

namespace hdtn{

BOOST_LOG_ATTRIBUTE_KEYWORD(severity, "Severity", logging::trivial::severity_level);

/**
 * Overloads the stream operator to support the Process enum
 */
static std::ostream& operator<< (std::ostream& strm, const Logger::Process& process)
{
    return strm << Logger::toString(process);
}

/**
 * Overloads the stream operator to support the SubProcess enum
 */
static std::ostream& operator<< (std::ostream& strm, const Logger::SubProcess& subprocess)
{
    return strm << Logger::toString(subprocess);;
}

Logger::Logger()
{
    init();
}

Logger::~Logger(){}

std::unique_ptr<Logger> Logger::logger_; //initialized to "null"
boost::mutex Logger::mutexSingletonInstance_;
volatile bool Logger::loggerSingletonFullyInitialized_ = false;
Logger::process_attr_t Logger::process_attr(Logger::Process::none);
Logger::subprocess_attr_t Logger::subprocess_attr(Logger::SubProcess::none);
Logger::file_attr_t Logger::file_attr("");
Logger::line_attr_t Logger::line_attr(-1);

void Logger::initializeWithProcess(Logger::Process process) {
    Logger::process_attr = process_attr_t(process);
    ensureInitialized();
}

void Logger::ensureInitialized()
{
    if (!loggerSingletonFullyInitialized_) { //fast way to bypass a mutex lock all the time
        //first thread that uses the logger gets to create the logger
        boost::mutex::scoped_lock theLock(mutexSingletonInstance_);
        if (!loggerSingletonFullyInitialized_) { //check it again now that mutex is locked
            logger_.reset(new Logger());
            loggerSingletonFullyInitialized_ = true;
        }
    }
}

Logger* Logger::getInstance()
{
    ensureInitialized();
    return logger_.get();
}

void Logger::init()
{
    //To prevent crash on termination
    boost::filesystem::path::imbue(std::locale("C"));

    registerAttributes();

    #ifdef LOG_TO_PROCESS_FILE
        createFileSinkForProcess(getProcessAttributeVal());
    #endif

    #ifdef LOG_TO_SUBPROCESS_FILES
        createFileSinkForSubProcess(Logger::SubProcess::egress);
        createFileSinkForSubProcess(Logger::SubProcess::ingress);
        createFileSinkForSubProcess(Logger::SubProcess::storage);
        createFileSinkForSubProcess(Logger::SubProcess::router);
        createFileSinkForSubProcess(Logger::SubProcess::scheduler);
    #endif

    #ifdef LOG_TO_ERROR_FILE
        createFileSinkForLevel(logging::trivial::severity_level::error);
    #endif

    #ifdef LOG_TO_CONSOLE
        createStdoutSink();
        createStderrSink();
    #endif

    logging::add_common_attributes(); //necessary for timestamp
}

void Logger::registerAttributes()
{   
    boost::log::trivial::logger::get().add_attribute("Process", Logger::process_attr);
    boost::log::trivial::logger::get().add_attribute("SubProcess", Logger::subprocess_attr);
    boost::log::trivial::logger::get().add_attribute("Line", Logger::line_attr);
    boost::log::trivial::logger::get().add_attribute("File", Logger::file_attr);
}

void Logger::createFileSinkForProcess(Logger::Process process)
{
    //Create hdtn log backend
    boost::shared_ptr<sinks::text_file_backend> sink_backend = 
        boost::make_shared<sinks::text_file_backend>(
            keywords::file_name = "logs/" + Logger::toString(process) + "_%5N.log",
            keywords::rotation_size = file_rotation_size
        );
 
    //Wrap log backend into frontend
    boost::shared_ptr<sink_t> sink = boost::make_shared<sink_t>(sink_backend);
    sink->set_formatter(Logger::processFileFormatter());
    sink->set_filter(expr::attr<Logger::Process>("Process") == process);
    sink->locked_backend()->auto_flush(true);
    logging::core::get()->add_sink(sink);
}

logging::formatter Logger::processFileFormatter() 
{
    static const logging::formatter fmt = expr::stream
        << expr::if_ (expr::has_attr<Logger::SubProcess>("SubProcess")
            && expr::attr<Logger::SubProcess>("SubProcess") != Logger::SubProcess::none)
        [
            expr::stream << "[ " << expr::attr<Logger::SubProcess>("SubProcess") << "]"
        ]
        << "[ " << expr::format_date_time<boost::posix_time::ptime>("TimeStamp","%Y-%m-%d %H:%M:%S") << "]"
        << "[ " << severity << "]: " << expr::smessage;
    return fmt;
}

void Logger::createFileSinkForSubProcess(const Logger::SubProcess subprocess)
{
    logging::formatter module_log_fmt = expr::stream
        << "[ " << expr::format_date_time<boost::posix_time::ptime>("TimeStamp","%Y-%m-%d %H:%M:%S")
        << "][ " << severity << "]: "<< expr::smessage;

    boost::shared_ptr<sinks::text_file_backend> sink_backend =
        boost::make_shared<sinks::text_file_backend>(
            keywords::file_name = "logs/" + Logger::toString(subprocess) + "_%5N.log",
            keywords::rotation_size = file_rotation_size
        );
    boost::shared_ptr<sink_t> sink = boost::make_shared<sink_t>(sink_backend);

    sink->set_formatter(module_log_fmt);
    sink->set_filter(expr::attr<Logger::SubProcess>("SubProcess") == subprocess);
    sink->locked_backend()->auto_flush(true);
    logging::core::get()->add_sink(sink);
}

void Logger::createFileSinkForLevel(logging::trivial::severity_level level)
{
    boost::shared_ptr<sinks::text_file_backend> sink_backend = boost::make_shared<sinks::text_file_backend>(
        keywords::file_name = std::string("logs/") + logging::trivial::to_string(level) + "_%5N.log",
        keywords::rotation_size = file_rotation_size
    );
    boost::shared_ptr<sink_t> sink = boost::make_shared<sink_t>(sink_backend);

    sink->set_formatter(Logger::levelFileFormatter());
    sink->set_filter(severity == level);
    sink->locked_backend()->auto_flush(true);
    logging::core::get()->add_sink(sink);
}

logging::formatter Logger::levelFileFormatter()
{
    static const logging::formatter fmt = expr::stream
        << expr::if_ (expr::has_attr<Logger::Process>("Process"))
        [
            expr::stream << "[ " << expr::attr<Logger::Process>("Process") << "]"
        ]
        << expr::if_ (expr::has_attr<Logger::SubProcess>("SubProcess")
            && expr::attr<Logger::SubProcess>("SubProcess") != Logger::SubProcess::none
        )
        [
            expr::stream << "[ " << expr::attr<Logger::SubProcess>("SubProcess") << "]"
        ]
        << "[ " << expr::format_date_time<boost::posix_time::ptime>("TimeStamp","%Y-%m-%d %H:%M:%S") << "]"
        << "[ " << expr::attr<std::string>("File") << ":"
        << expr::attr<int>("Line") << "]: " << expr::smessage;
    return fmt;
}

std::string Logger::toString(Logger::Process process)
{
    static constexpr uint32_t num_processes = sizeof(process_strings)/sizeof(*process_strings);
    int process_val = static_cast<typename std::underlying_type<Logger::Process>::type>(process);
    if (process_val > num_processes) {
        return "";
    }
    return process_strings[process_val];
}

std::string Logger::toString(Logger::SubProcess subprocess)
{
    static constexpr uint32_t num_subprocesses = sizeof(subprocess_strings)/sizeof(*subprocess_strings);
    int subprocess_val = static_cast<typename std::underlying_type<Logger::Process>::type>(subprocess);
    if (subprocess_val > num_subprocesses) {
        return "";
    }
    return subprocess_strings[subprocess_val];
}

Logger::SubProcess Logger::fromString(std::string subprocess) {
    static constexpr uint32_t num_modules = sizeof(subprocess_strings)/sizeof(*subprocess_strings);
    for (uint32_t i=0; i<num_modules; i++) {
        if (subprocess.compare(subprocess_strings[i]) == 0) {
            return Logger::SubProcess(i);
        }
    }
    return Logger::SubProcess::none;
}

void Logger::createStdoutSink() {
    boost::shared_ptr<sinks::text_ostream_backend> stdout_sink_backend =
        boost::make_shared<sinks::text_ostream_backend>();
    stdout_sink_backend->add_stream(
        boost::shared_ptr<std::ostream>(&std::cout, boost::null_deleter())
    );

    typedef sinks::synchronous_sink< sinks::text_ostream_backend > sink_t;
    boost::shared_ptr<sink_t> stdout_sink = boost::make_shared<sink_t>(stdout_sink_backend);

    stdout_sink->set_filter(severity != logging::trivial::severity_level::error &&
        severity != logging::trivial::severity_level::fatal);
    stdout_sink->set_formatter(Logger::consoleFormatter());
    stdout_sink->locked_backend()->auto_flush(true);
    logging::core::get()->add_sink(stdout_sink);
}

void Logger::createStderrSink() {
    boost::shared_ptr<sinks::text_ostream_backend> stderr_sink_backend =
        boost::make_shared<sinks::text_ostream_backend>();
    stderr_sink_backend->add_stream(
        boost::shared_ptr<std::ostream>(&std::cerr, boost::null_deleter())
    );

    typedef sinks::synchronous_sink< sinks::text_ostream_backend > sink_t;
    boost::shared_ptr<sink_t> stderr_sink = boost::make_shared<sink_t>(stderr_sink_backend);

    stderr_sink->set_filter(severity == logging::trivial::severity_level::error ||
        severity == logging::trivial::severity_level::fatal);
    stderr_sink->set_formatter(Logger::consoleFormatter());
    stderr_sink->locked_backend()->auto_flush(true);
    logging::core::get()->add_sink(stderr_sink);
}

boost::log::formatter Logger::consoleFormatter() {
   static const logging::formatter fmt = expr::stream
        << "[ " << std::setw(console_message_offset) << std::left
        << expr::if_ (
            expr::has_attr<Logger::SubProcess>("SubProcess")
            && expr::attr<Logger::SubProcess>("SubProcess") != Logger::SubProcess::none
        )
        [
            expr::stream << expr::attr<Logger::SubProcess>("SubProcess")
        ]
        .else_
        [
            expr::stream << expr::attr<Logger::Process>("Process")
        ]
        << "]"
        << "[ " << severity << "]: " << expr::smessage;

    return fmt;
}

Logger::Process Logger::getProcessAttributeVal()
{
    logging::value_ref< Logger::Process > val = logging::extract< Logger::Process >
        (Logger::process_attr.get_value());
    if (val) {
        return val.get();
    }
    return Logger::Process::none;
}

void Logger::logInfo(const std::string & subprocess, const std::string & message)
{
    _ADD_LOG_ATTRIBUTES(fromString(subprocess));
    BOOST_LOG_TRIVIAL(debug) << message;
}

void Logger::logNotification(const std::string & subprocess, const std::string & message)
{
    _ADD_LOG_ATTRIBUTES(fromString(subprocess));
    BOOST_LOG_TRIVIAL(info) << message;
}

void Logger::logWarning(const std::string & subprocess, const std::string & message)
{
    _ADD_LOG_ATTRIBUTES(fromString(subprocess));
    BOOST_LOG_TRIVIAL(warning) << message;
}

void Logger::logError(const std::string & subprocess, const std::string & message)
{
    _ADD_LOG_ATTRIBUTES(fromString(subprocess));
    BOOST_LOG_TRIVIAL(error) << message;
}

void Logger::logCritical(const std::string & subprocess, const std::string & message)
{
    _ADD_LOG_ATTRIBUTES(fromString(subprocess));
    BOOST_LOG_TRIVIAL(fatal) << message;
}


} //namespace hdtn

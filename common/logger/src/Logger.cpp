/***************************************************************************
 * NASA Glenn Research Center, Cleveland, OH
 * Released under the NASA Open Source Agreement (NOSA)
 * May  2021
 *
 ****************************************************************************
 */

#include "Logger.h"

#include "HdtnVersion.hpp"
#include <boost/preprocessor/slot/slot.hpp>
#include <boost/preprocessor/stringize.hpp>
#define BOOST_PP_VALUE HDTN_VERSION_MAJOR
#include BOOST_PP_ASSIGN_SLOT(1)
#undef BOOST_PP_VALUE

#define BOOST_PP_VALUE HDTN_VERSION_MINOR
#include BOOST_PP_ASSIGN_SLOT(2)
#undef BOOST_PP_VALUE

#define BOOST_PP_VALUE HDTN_VERSION_PATCH
#include BOOST_PP_ASSIGN_SLOT(3)
#undef BOOST_PP_VALUE

#ifdef HDTN_COMMIT_SHA
#define HDTN_COMMIT_SHA_STRING BOOST_PP_STRINGIZE(HDTN_COMMIT_SHA)
#endif


#define HDTN_VERSION_STRING BOOST_PP_STRINGIZE( \
    BOOST_PP_CAT(BOOST_PP_SLOT(1), \
    BOOST_PP_CAT(., \
    BOOST_PP_CAT(BOOST_PP_SLOT(2), \
    BOOST_PP_CAT(., BOOST_PP_SLOT(3))))))



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
    "storage",
    "releasemessagesender",
    "storagespeedtest",
    "udpdelaysim",
    "unittest",
    "bpsendstream",
    "bprecvstream",
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
    "storage",
    "telem",
    "gui",
    "unittest",
    "cli"
    ""
};

static const uint32_t file_rotation_size = 5 * 1024 * 1024; //5 MiB
static const uint8_t console_message_offset_process = 9;
static const uint8_t console_message_offset_severity = 5;

// Namespaces recommended by the Boost log library
namespace logging = boost::log;
namespace src = boost::log::sources;
namespace expr = boost::log::expressions;
namespace sinks = boost::log::sinks;
namespace attrs = boost::log::attributes;
namespace keywords = boost::log::keywords;

namespace hdtn{

BOOST_LOG_ATTRIBUTE_KEYWORD(severity, "Severity", logging::trivial::severity_level) //no ending ";"
BOOST_LOG_ATTRIBUTE_KEYWORD(channel, "Channel", Logger::SubProcess) //no ending ";"

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
    return strm << Logger::toString(subprocess);
}

Logger::Logger()
{
    init();
}

Logger::~Logger(){}

std::unique_ptr<Logger> Logger::logger_; //initialized to "null"
boost::mutex Logger::mutexSingletonInstance_;
std::atomic<bool> Logger::loggerSingletonFullyInitialized_(false);
Logger::process_attr_t Logger::process_attr(Logger::Process::none);
Logger::severity_channel_logger_t Logger::m_severityChannelLogger;

const std::string& Logger::GetHdtnVersionAsString() {
    static const std::string hdtnVersionString = HDTN_VERSION_STRING;
    return hdtnVersionString;
}

void Logger::initializeWithProcess(Logger::Process process) {
    Logger::process_attr = process_attr_t(process);
    ensureInitialized();
    LOG_INFO(Logger::SubProcess::none) << "This is HDTN version " HDTN_VERSION_STRING;
#ifdef HDTN_COMMIT_SHA_STRING
    LOG_INFO(Logger::SubProcess::none) << "HDTN Git commit SHA-1 is: " HDTN_COMMIT_SHA_STRING;
#endif
}

void Logger::ensureInitialized() noexcept {
    while (!loggerSingletonFullyInitialized_.load(std::memory_order_acquire)) { //fast way to bypass a mutex lock all the time
        try {
            //first thread that uses the logger gets to create the logger
            boost::mutex::scoped_lock theLock(mutexSingletonInstance_);
            if (!loggerSingletonFullyInitialized_) { //check it again now that mutex is locked
                logger_.reset(new Logger());
                loggerSingletonFullyInitialized_ = true;
            }
        }
        catch (const boost::lock_error&) {
            continue;
        }
        catch (const std::exception&) {
            continue;
        }
    }
}

void Logger::init()
{
    //To prevent crash on termination
    boost::filesystem::path::imbue(std::locale("C"));

    Logger::m_severityChannelLogger = Logger::severity_channel_logger_t(
        keywords::channel = Logger::SubProcess::none
    );
    registerAttributes();

    #ifdef LOG_TO_PROCESS_FILE
        createFileSinkForProcess(getProcessAttributeVal());
    #endif

    #ifdef LOG_TO_SUBPROCESS_FILES
        createFileSinkForSubProcess(Logger::SubProcess::egress);
        createFileSinkForSubProcess(Logger::SubProcess::ingress);
        createFileSinkForSubProcess(Logger::SubProcess::storage);
        createFileSinkForSubProcess(Logger::SubProcess::router);
        createFileSinkForSubProcess(Logger::SubProcess::telem);
    #endif

    #ifdef LOG_TO_ERROR_FILE
        createFileSinkForLevel(logging::trivial::severity_level::error);
        createFileSinkForLevel(logging::trivial::severity_level::fatal);
    #endif

    #ifdef LOG_TO_CONSOLE
        createStdoutSink();
    #endif

    logging::add_common_attributes(); //necessary for timestamp
}

void Logger::registerAttributes()
{   
    m_severityChannelLogger.add_attribute("Process", Logger::process_attr);
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
        << "[ " << std::setw(console_message_offset_process) << std::left
        << expr::if_ (channel != Logger::SubProcess::none)
        [
            expr::stream << channel
        ]
        .else_
        [
            expr::stream << expr::attr<Logger::Process>("Process")
        ]
        << "]"
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
    sink->set_filter(channel == subprocess);
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
        << expr::if_ (channel != Logger::SubProcess::none)
        [
            expr::stream << "[ " << channel << "]"
        ]
        << "[ " << expr::format_date_time<boost::posix_time::ptime>("TimeStamp","%Y-%m-%d %H:%M:%S") << "]"
        << "[ " << expr::attr<std::string>("File") << ":"
        << expr::attr<int>("Line") << "]: " << expr::smessage;
    return fmt;
}

std::string Logger::toString(Logger::Process process)
{
    static constexpr uint32_t num_processes = sizeof(process_strings)/sizeof(*process_strings);
    const uint32_t process_val = static_cast<typename std::underlying_type<Logger::Process>::type>(process);
    if (process_val >= num_processes) {
        return "";
    }
    return process_strings[process_val];
}

std::string Logger::toString(Logger::SubProcess subprocess)
{
    static constexpr uint32_t num_subprocesses = sizeof(subprocess_strings)/sizeof(*subprocess_strings);
    const uint32_t subprocess_val = static_cast<typename std::underlying_type<Logger::SubProcess>::type>(subprocess);
    if (subprocess_val >= num_subprocesses) {
        return "";
    }
    return subprocess_strings[subprocess_val];
}

void Logger::createStdoutSink() {
    boost::shared_ptr<sinks::text_ostream_backend> stdout_sink_backend =
        boost::make_shared<sinks::text_ostream_backend>();
    stdout_sink_backend->add_stream(
        boost::shared_ptr<std::ostream>(&std::cout, boost::null_deleter())
    );

    typedef sinks::synchronous_sink< sinks::text_ostream_backend > ostream_sink_t;
    boost::shared_ptr<ostream_sink_t> stdout_sink = boost::make_shared<ostream_sink_t>(stdout_sink_backend);

    stdout_sink->set_filter(expr::has_attr<logging::trivial::severity_level>("Severity"));
    stdout_sink->set_formatter(Logger::consoleFormatter());
    stdout_sink->locked_backend()->auto_flush(true);
    logging::core::get()->add_sink(stdout_sink);
}

boost::log::formatter Logger::consoleFormatter() {
   static const logging::formatter fmt = expr::stream
        << "[ " << std::setw(console_message_offset_process) << std::left
        << expr::if_ (channel != Logger::SubProcess::none)
        [
            expr::stream << channel
        ]
        .else_
        [
            expr::stream << expr::attr<Logger::Process>("Process")
        ]
        << "]"
        << "[ " << std::setw(console_message_offset_severity) << std::left 
        << severity << "]: " << expr::smessage;

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


} //namespace hdtn

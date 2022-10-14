/***************************************************************************
 * NASA Glenn Research Center, Cleveland, OH
 * Released under the NASA Open Source Agreement (NOSA)
 * May  2021
 *
 ****************************************************************************
 */

#include "Logger.h"

/**
 * Contains string values that correspond to the "Module" enum
 * in Logger.h. If a new module is added, a string representation
 * should be added here.
 */
static const std::string module_strings[] =
{
    "egress",
    "ingress",
    "router",
    "scheduler",
    "storage",
};

static const uint32_t file_rotation_size = 5 * 1024 * 1024; //5 MiB

// Namespaces recommended by the Boost log library
namespace logging = boost::log;
namespace src = boost::log::sources;
namespace expr = boost::log::expressions;
namespace sinks = boost::log::sinks;
namespace attrs = boost::log::attributes;
namespace keywords = boost::log::keywords;

namespace hdtn{

BOOST_LOG_ATTRIBUTE_KEYWORD(severity, "Severity", logging::trivial::severity_level)

/**
 * Overloads the stream operator to support the Module enum
 */
std::ostream& operator<< (std::ostream& strm, Logger::Module module)
{
    return strm << Logger::toString(module);
}

Logger::Logger()
{
    init();
}

Logger::~Logger(){}

Logger* Logger::logger_ = nullptr;
Logger::module_attr_t Logger::module_attr(Logger::Module(-1));
Logger::file_attr_t Logger::file_attr("");
Logger::line_attr_t Logger::line_attr(-1);

bool Logger::ensureInitialized()
{
    if(Logger::logger_ == nullptr)
    {
        Logger::logger_ = new Logger();
        return true;
    }
    return false;
}

Logger* Logger::getInstance()
{
    if(Logger::logger_ == nullptr)
    {
        Logger::logger_ = new Logger();
    }

    return Logger::logger_;
}

void Logger::init()
{
    //To prevent crash on termination
    boost::filesystem::path::imbue(std::locale("C"));

    registerAttributes();

    #ifdef LOG_TO_SINGLE_FILE
        createFileSinkForFullHdtnLog();
    #endif

    #ifdef LOG_TO_MODULE_FILES
        createFileSinkForModule(Logger::Module::egress);
        createFileSinkForModule(Logger::Module::ingress);
        createFileSinkForModule(Logger::Module::storage);
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
    boost::log::trivial::logger::get().add_attribute("Module", Logger::module_attr);
    boost::log::trivial::logger::get().add_attribute("Line", Logger::line_attr);
    boost::log::trivial::logger::get().add_attribute("File", Logger::file_attr);
}

void Logger::createFileSinkForFullHdtnLog()
{
    logging::formatter hdtn_log_fmt = expr::stream
        << "[" << expr::attr<Logger::Module>("Module") << "]["
        << expr::format_date_time<boost::posix_time::ptime>("TimeStamp","%Y-%m-%d %H:%M:%S")
        << "][" << severity << "]: " << expr::smessage;

    //Create hdtn log backend
    boost::shared_ptr<sinks::text_file_backend> sink_backend = 
        boost::make_shared<sinks::text_file_backend>(
            keywords::file_name = "logs/hdtn_%5N.log",
            keywords::rotation_size = file_rotation_size
        );
 
    //Wrap log backend into frontend
    boost::shared_ptr<sink_t> sink = boost::make_shared<sink_t>(sink_backend);
    sink->set_formatter(hdtn_log_fmt);
    sink->locked_backend()->auto_flush(true);
    logging::core::get()->add_sink(sink);
}

void Logger::createFileSinkForModule(const Logger::Module module)
{
    logging::formatter module_log_fmt = expr::stream
        << "[" << expr::format_date_time<boost::posix_time::ptime>("TimeStamp","%Y-%m-%d %H:%M:%S")
        << "][" << severity << "]: "<< expr::smessage;

    boost::shared_ptr<sinks::text_file_backend> sink_backend =
        boost::make_shared<sinks::text_file_backend>(
            keywords::file_name = "logs/" + Logger::toString(module) + "_%5N.log",
            keywords::rotation_size = file_rotation_size
        );
    boost::shared_ptr<sink_t> sink = boost::make_shared<sink_t>(sink_backend);

    sink->set_formatter(module_log_fmt);
    sink->set_filter(expr::attr<Logger::Module>("Module") == module);
    sink->locked_backend()->auto_flush(true);
    logging::core::get()->add_sink(sink);
}

void Logger::createFileSinkForLevel(logging::trivial::severity_level level)
{
    logging::formatter severity_log_fmt = expr::stream
        << "[" << expr::attr<Logger::Module>("Module") << "]["
        << expr::format_date_time<boost::posix_time::ptime>("TimeStamp","%Y-%m-%d %H:%M:%S")
        << "][" << expr::attr<std::string>("File") << ":"
        << expr::attr<int>("Line") << "]: " << expr::smessage;

    boost::shared_ptr<sinks::text_file_backend> sink_backend = boost::make_shared<sinks::text_file_backend>(
        keywords::file_name = std::string("logs/") + logging::trivial::to_string(level) + "_%5N.log",
        keywords::rotation_size = file_rotation_size
    );
    boost::shared_ptr<sink_t> sink = boost::make_shared<sink_t>(sink_backend);

    sink->set_formatter(severity_log_fmt);
    sink->set_filter(severity == level);
    sink->locked_backend()->auto_flush(true);
    logging::core::get()->add_sink(sink);
}

std::string Logger::toString(Logger::Module module)
{
    static constexpr uint32_t num_modules = sizeof(module_strings)/sizeof(*module_strings);
    int module_val = static_cast<typename std::underlying_type<Logger::Module>::type>(module);
    if (module_val > num_modules) {
        return "";
    }
    return module_strings[module_val];
}

Logger::Module Logger::fromString(std::string module) {
    uint32_t num_modules = sizeof(module_strings)/sizeof(*module_strings);
    for (int i=0; i<num_modules; i++) {
        if (module.compare(module_strings[i]) == 0) {
            return Logger::Module(i);
        }
    }
    return Logger::Module(-1);
}

void Logger::createStdoutSink() {
    logging::formatter fmt = expr::stream
        << "[" << expr::attr<Logger::Module>("Module") << "]"
        << "[" << severity << "]: " << expr::smessage;

    boost::shared_ptr<sinks::text_ostream_backend> stdout_sink_backend =
        boost::make_shared<sinks::text_ostream_backend>();
    stdout_sink_backend->add_stream(
        boost::shared_ptr<std::ostream>(&std::cout, boost::null_deleter())
    );

    typedef sinks::synchronous_sink< sinks::text_ostream_backend > sink_t;
    boost::shared_ptr<sink_t> stdout_sink = boost::make_shared<sink_t>(stdout_sink_backend);

    stdout_sink->set_filter(severity != logging::trivial::severity_level::error &&
        severity != logging::trivial::severity_level::fatal);
    stdout_sink->set_formatter(fmt);
    stdout_sink->locked_backend()->auto_flush(true);
    logging::core::get()->add_sink(stdout_sink);
}

void Logger::createStderrSink() {
    logging::formatter fmt = expr::stream
        << "[" << expr::attr<Logger::Module>("Module") << "]"
        << "[" << severity << "]: " << expr::smessage;

    boost::shared_ptr<sinks::text_ostream_backend> stderr_sink_backend =
        boost::make_shared<sinks::text_ostream_backend>();
    stderr_sink_backend->add_stream(
        boost::shared_ptr<std::ostream>(&std::cerr, boost::null_deleter())
    );

    typedef sinks::synchronous_sink< sinks::text_ostream_backend > sink_t;
    boost::shared_ptr<sink_t> stderr_sink = boost::make_shared<sink_t>(stderr_sink_backend);

    stderr_sink->set_filter(severity == logging::trivial::severity_level::error ||
        severity == logging::trivial::severity_level::fatal);
    stderr_sink->set_formatter(fmt);
    stderr_sink->locked_backend()->auto_flush(true);
    logging::core::get()->add_sink(stderr_sink);
}

void Logger::logInfo(const std::string & module, const std::string & message)
{
    BOOST_LOG_SCOPED_THREAD_TAG("Module", fromString(module));
    BOOST_LOG_SEV(log_, logging::trivial::severity_level::debug) << message;
}

void Logger::logNotification(const std::string & module, const std::string & message)
{
    BOOST_LOG_SCOPED_THREAD_TAG("Module", fromString(module));
    BOOST_LOG_SEV(log_, logging::trivial::severity_level::info) << message;
}

void Logger::logWarning(const std::string & module, const std::string & message)
{
    BOOST_LOG_SCOPED_THREAD_TAG("Module", fromString(module));
    BOOST_LOG_SEV(log_, logging::trivial::severity_level::warning) << message;
}

void Logger::logError(const std::string & module, const std::string & message)
{
    BOOST_LOG_SCOPED_THREAD_TAG("Module", fromString(module));
    BOOST_LOG_SEV(log_, logging::trivial::severity_level::error) << message;
}

void Logger::logCritical(const std::string & module, const std::string & message)
{
    BOOST_LOG_SCOPED_THREAD_TAG("Module", fromString(module));
    BOOST_LOG_SEV(log_, logging::trivial::severity_level::fatal) << message;
}


} //namespace hdtn

/***************************************************************************
 * NASA Glenn Research Center, Cleveland, OH
 * Released under the NASA Open Source Agreement (NOSA)
 * May  2021
 *
 ****************************************************************************
 */

#include "Logger.h"

static const char* severity_strings[] =
{
    "Info",
    "Notification",
    "Warning",
    "Error",
    "Critical"
};

static const int log_file_rotation_size = 5 * 1024 * 1024; //5 MiB

namespace logging = boost::log;
namespace src = boost::log::sources;
namespace expr = boost::log::expressions;
namespace sinks = boost::log::sinks;
namespace attrs = boost::log::attributes;
namespace keywords = boost::log::keywords;

namespace hdtn{

BOOST_LOG_ATTRIBUTE_KEYWORD(severity, "Severity", logging::trivial::severity_level)
BOOST_LOG_ATTRIBUTE_KEYWORD(module_attr, "Module", std::string)

Logger::Logger()
{
    init();
}

Logger::~Logger(){}

Logger* Logger::logger_ = nullptr;

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

    //Create sinks
    createConsoleLogSink();
    createMainLogFileSink();
    createLogFileSinkForModule("egress");
    createLogFileSinkForModule("ingress");
    createLogFileSinkForModule("storage");
    createLogFileSinkForSeverity(logging::trivial::severity_level::error);

    logging::add_common_attributes(); //necessary for timestamp
}

void Logger::createMainLogFileSink()
{
    //Set logging format
    logging::formatter fmt = expr::stream
        << "[" << module_attr << "]["
        << expr::format_date_time<boost::posix_time::ptime>("TimeStamp","%Y-%m-%d %H:%M:%S")
        << "][" << severity << "]: \t" << expr::smessage;

    //Create sink and attach to logger
    boost::shared_ptr<sinks::text_file_backend> sink_backend = 
        boost::make_shared<sinks::text_file_backend>(
            keywords::file_name = "logs/hdtn_%5N.log",
            keywords::rotation_size = log_file_rotation_size
        );
    boost::shared_ptr<sink_t> sink(new sink_t(sink_backend));
    sink->set_formatter(fmt);
    sink->locked_backend()->auto_flush(true);
    logging::core::get()->add_sink(sink);
}

void Logger::createConsoleLogSink()
{
    //Set logging format
    logging::formatter fmt = expr::stream
        << "[" << module_attr << "]["
        << expr::format_date_time<boost::posix_time::ptime>("TimeStamp","%Y-%m-%d %H:%M:%S")
        << "][" << severity << "]: \t" << expr::smessage;

    //Create sink for cout
    boost::shared_ptr<sinks::text_ostream_backend> cout_sink_backend =
        boost::make_shared<sinks::text_ostream_backend>();
    cout_sink_backend->add_stream(
        boost::shared_ptr<std::ostream>(&std::cout, boost::null_deleter())
    );
    typedef sinks::synchronous_sink< sinks::text_ostream_backend > sink_t;
    boost::shared_ptr<sink_t> cout_sink(new sink_t(cout_sink_backend));
    cout_sink->set_filter(severity != logging::trivial::severity_level::error && severity != logging::trivial::severity_level::fatal);
    cout_sink->set_formatter(fmt);
    cout_sink->locked_backend()->auto_flush(true);
    logging::core::get()->add_sink(cout_sink);

    //Create sink for cerr
    boost::shared_ptr<sinks::text_ostream_backend> cerr_sink_backend =
        boost::make_shared<sinks::text_ostream_backend>();
    cerr_sink_backend->add_stream(
        boost::shared_ptr<std::ostream>(&std::cerr, boost::null_deleter())
    );
    typedef sinks::synchronous_sink< sinks::text_ostream_backend > sink_t;
    boost::shared_ptr<sink_t> cerr_sink(new sink_t(cerr_sink_backend));
    cout_sink->set_filter(severity == logging::trivial::severity_level::error || severity == logging::trivial::severity_level::fatal);
    cerr_sink->set_formatter(fmt);
    cerr_sink->locked_backend()->auto_flush(true);
    logging::core::get()->add_sink(cerr_sink);
}

void Logger::createLogFileSinkForModule(const std::string & module)
{
    logging::formatter module_log_fmt = expr::stream
        << "[" << expr::format_date_time<boost::posix_time::ptime>("TimeStamp","%Y-%m-%d %H:%M:%S")
        << "][" << severity << "]: "<< expr::smessage;

    boost::shared_ptr<sinks::text_file_backend> sink_backend =
        boost::make_shared<sinks::text_file_backend>(
            keywords::file_name = "logs/" + module + "_%5N.log",
            keywords::rotation_size = log_file_rotation_size
        );
    boost::shared_ptr<sink_t> sink(new sink_t(sink_backend));

    sink->set_formatter(module_log_fmt);
    sink->set_filter(module_attr == module);
    sink->locked_backend()->auto_flush(true);
    logging::core::get()->add_sink(sink);
}

void Logger::createLogFileSinkForSeverity(logging::trivial::severity_level level)
{
    logging::formatter severity_log_fmt = expr::stream
        << "[" << module_attr << "]["
        << expr::format_date_time<boost::posix_time::ptime>("TimeStamp","%Y-%m-%d %H:%M:%S")
        << "][" << severity << "]: \t" << expr::smessage;

     boost::shared_ptr<sinks::text_file_backend> sink_backend = boost::make_shared<sinks::text_file_backend>(
        keywords::file_name = "logs/" + std::string(severity_strings[level]) + "_%5N.log",
        keywords::rotation_size = log_file_rotation_size
     );
    boost::shared_ptr<sink_t> sink(new sink_t(sink_backend));
    sink->set_formatter(severity_log_fmt);
    sink->set_filter(severity == level);
    sink->locked_backend()->auto_flush(true);
    logging::core::get()->add_sink(sink);
}

void Logger::logInfo(const std::string & module, const std::string & message)
{
    BOOST_LOG_SCOPED_THREAD_TAG("Module", module);
    // BOOST_LOG_SEV(log_, info) << message;
}

void Logger::logNotification(const std::string & module, const std::string & message)
{
    BOOST_LOG_SCOPED_THREAD_TAG("Module", module);
    // BOOST_LOG_SEV(log_, notification) << message;
}

void Logger::logWarning(const std::string & module, const std::string & message)
{
    BOOST_LOG_SCOPED_THREAD_TAG("Module", module);
    // BOOST_LOG_SEV(log_, warning) << message;
}

void Logger::logError(const std::string & module, const std::string & message)
{
    BOOST_LOG_SCOPED_THREAD_TAG("Module", module);
    // BOOST_LOG_SEV(log_, error) << message;
}

void Logger::logCritical(const std::string & module, const std::string & message)
{
    BOOST_LOG_SCOPED_THREAD_TAG("Module", module);
    // BOOST_LOG_SEV(log_, critical) << message;
}


} //namespace hdtn

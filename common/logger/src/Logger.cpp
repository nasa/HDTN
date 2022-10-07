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

namespace logging = boost::log;
namespace src = boost::log::sources;
namespace expr = boost::log::expressions;
namespace sinks = boost::log::sinks;
namespace attrs = boost::log::attributes;
namespace keywords = boost::log::keywords;

namespace hdtn{

BOOST_LOG_ATTRIBUTE_KEYWORD(severity, "Severity", logging::trivial::severity_level)
BOOST_LOG_ATTRIBUTE_KEYWORD(module_attr, "Module", std::string)
BOOST_LOG_ATTRIBUTE_KEYWORD(file_attr, "File", std::string)
BOOST_LOG_ATTRIBUTE_KEYWORD(line_attr, "Line", unsigned int)

Logger::Logger()
{
    init();
}

Logger::~Logger(){}

Logger* Logger::logger_ = nullptr;

void Logger::ensureInitialized()
{
    if(Logger::logger_ == nullptr)
    {
        Logger::logger_ = new Logger();
    }
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

    //set format for complete HDTN Log
    logging::formatter hdtn_log_fmt = expr::stream
        << "[" << module_attr << "]["
        << expr::format_date_time<boost::posix_time::ptime>("TimeStamp","%Y-%m-%d %H:%M:%S")
        << "][" << severity << "]: \t" << expr::smessage;

    //Create hdtn log backend
    boost::shared_ptr<sinks::text_file_backend> sink_backend = 
        boost::make_shared<sinks::text_file_backend>(
            keywords::file_name = "logs/hdtn_%5N.log",
            keywords::rotation_size = 5 * 1024 * 1024 //5 MiB
        );
 
    //Wrap log backend into frontend
    boost::shared_ptr<sink_t> sink(new sink_t(sink_backend));
    sink->set_formatter(hdtn_log_fmt);
    sink->locked_backend()->auto_flush(true);
    logging::core::get()->add_sink(sink);

    //Add and format module log files
    createModuleLogFile("egress");
    createModuleLogFile("ingress");
    createModuleLogFile("storage");

    //Add error log
    createSeverityLogFile(logging::trivial::severity_level::error);
    logging::add_common_attributes(); //necessary for timestamp

    createConsoleLogSink();
}

void Logger::createModuleLogFile(const std::string & module)
{
    logging::formatter module_log_fmt = expr::stream
        << "[" << expr::format_date_time<boost::posix_time::ptime>("TimeStamp","%Y-%m-%d %H:%M:%S")
        << "][" << severity << "]: "<< expr::smessage;

    boost::shared_ptr<sinks::text_file_backend> sink_backend =
        boost::make_shared<sinks::text_file_backend>(
            keywords::file_name = "logs/" + module + "_%5N.log",
            keywords::rotation_size = 5 * 1024 * 1024 //5 MiB
        );
    boost::shared_ptr<sink_t> sink(new sink_t(sink_backend));

    sink->set_formatter(module_log_fmt);
    sink->set_filter(module_attr == module);
    sink->locked_backend()->auto_flush(true);
    logging::core::get()->add_sink(sink);
}

void Logger::createSeverityLogFile(logging::trivial::severity_level level)
{
    logging::formatter severity_log_fmt = expr::stream
        << "[" << module_attr << "]["
        << expr::format_date_time<boost::posix_time::ptime>("TimeStamp","%Y-%m-%d %H:%M:%S")
        << "][" << severity << "]: \t" << expr::smessage;

     boost::shared_ptr<sinks::text_file_backend> sink_backend = boost::make_shared<sinks::text_file_backend>(
        keywords::file_name = "logs/" + std::string(severity_strings[level]) + "_%5N.log",
        keywords::rotation_size = 5 * 1024 * 1024 //5 MiB
     );
    boost::shared_ptr<sink_t> sink(new sink_t(sink_backend));
    sink->set_formatter(severity_log_fmt);
    sink->set_filter(severity == level);
    sink->locked_backend()->auto_flush(true);
    logging::core::get()->add_sink(sink);
}

void Logger::createConsoleLogSink()
{
    //Set logging format
    logging::formatter fmt = expr::stream
        << "[" << module_attr << "]"
        << "[" << severity << "]: \t" << expr::smessage;

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

void Logger::logInfo(const std::string & module, const std::string & message)
{
    BOOST_LOG_SCOPED_THREAD_TAG("Module", module);
    BOOST_LOG_SEV(log_, logging::trivial::severity_level::info) << message;
}

void Logger::logNotification(const std::string & module, const std::string & message)
{
    BOOST_LOG_SCOPED_THREAD_TAG("Module", module);
    BOOST_LOG_SEV(log_, logging::trivial::severity_level::debug) << message;
}

void Logger::logWarning(const std::string & module, const std::string & message)
{
    BOOST_LOG_SCOPED_THREAD_TAG("Module", module);
    BOOST_LOG_SEV(log_, logging::trivial::severity_level::warning) << message;
}

void Logger::logError(const std::string & module, const std::string & message)
{
    BOOST_LOG_SCOPED_THREAD_TAG("Module", module);
    BOOST_LOG_SEV(log_, logging::trivial::severity_level::error) << message;
}

void Logger::logCritical(const std::string & module, const std::string & message)
{
    BOOST_LOG_SCOPED_THREAD_TAG("Module", module);
    BOOST_LOG_SEV(log_, logging::trivial::severity_level::fatal) << message;
}


} //namespace hdtn

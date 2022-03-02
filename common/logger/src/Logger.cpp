#include "Logger.h"
//#define BOOST_LOG_DYN_LINK 1

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

//Representation of severity level to the stream
std::ostream& operator<< (std::ostream& strm, hdtn::severity_level level)
{
    if(static_cast<std::size_t>(level) < sizeof(severity_strings)/sizeof(*severity_strings))
        strm << severity_strings[level];
    else
        strm << static_cast<int>(level);

    return strm;
}

//Meant for logging modules using enums
/*
std::ostream& operator<< (std::ostream& strm, hdtn::module mod)
{
    static const char* strings[] =
    {
        "Egress",
        "Ingress",
        "Storage",
    };

    if(static_cast<std::size_t>(mod) < sizeof(strings)/sizeof(*strings))
        strm << strings[mod];
    else
        strm << static_cast<int>(mod);

    return strm;
}
*/

BOOST_LOG_ATTRIBUTE_KEYWORD(severity, "Severity", hdtn::severity_level)
BOOST_LOG_ATTRIBUTE_KEYWORD(module_attr, "Module", std::string)

//Not used by Logger but here anyway? I didn't write this but used in other code
/*
std::string Datetime(){
#ifndef _WIN32
    time_t rawtime;
    struct tm* timeinfo;
    char buffer[80];

    time(&rawtime);
    timeinfo = localtime(&rawtime);

    strftime(buffer, 80, "%d-%m-%Y-%H:%M:%S", timeinfo);
    return std::string(buffer);
#else
    return "";
#endif // ! _WIN32
}
*/

Logger::Logger()
{
    init();
}

//Logger::Logger(Logger const&){} - may need for singleton

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
    createSeverityLogFile(hdtn::severity_level::error);
    logging::add_common_attributes(); //necessary for timestamp

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

void Logger::createSeverityLogFile(hdtn::severity_level level)
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

void Logger::logInfo(const std::string & module, const std::string & message)
{
    BOOST_LOG_SCOPED_THREAD_TAG("Module", module);
    BOOST_LOG_SEV(log_, info) << message;
}

void Logger::logNotification(const std::string & module, const std::string & message)
{
    BOOST_LOG_SCOPED_THREAD_TAG("Module", module);
    BOOST_LOG_SEV(log_, notification) << message;
}

void Logger::logWarning(const std::string & module, const std::string & message)
{
    BOOST_LOG_SCOPED_THREAD_TAG("Module", module);
    BOOST_LOG_SEV(log_, warning) << message;
}

void Logger::logError(const std::string & module, const std::string & message)
{
    BOOST_LOG_SCOPED_THREAD_TAG("Module", module);
    BOOST_LOG_SEV(log_, error) << message;
}

void Logger::logCritical(const std::string & module, const std::string & message)
{
    BOOST_LOG_SCOPED_THREAD_TAG("Module", module);
    BOOST_LOG_SEV(log_, critical) << message;
}


} //namespace hdtn

#include "Logger.h"
#define BOOST_LOG_DYN_LINK 1


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
    static const char* strings[] = 
    {
        "Info",
        "Notification",
        "Warning",
        "Error",
        "Critical"
    };

    if(static_cast<std::size_t>(level) < sizeof(strings)/sizeof(*strings))
        strm << strings[level];
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
    //set format for complete HDTN Log
    logging::formatter hdtn_log_fmt = expr::stream
        << "[" << module_attr << "]["
        << expr::format_date_time<boost::posix_time::ptime>("TimeStamp","%Y-%m-%d %H:%M:%S")
        << "][" << severity << "]: \t" << expr::smessage;

    //Create hdtn log
    boost::shared_ptr<text_sink> sink = boost::make_shared<text_sink>();
    sink->locked_backend()->add_stream(
        boost::make_shared<std::ofstream>("hdtn.log"));
    sink->set_formatter(hdtn_log_fmt);
    sink->locked_backend()->auto_flush(true);
    logging::core::get()->add_sink(sink);

    //Add and format module log files
    createModuleLogFile("logs/egress");
    createModuleLogFile("logs/ingress");
    createModuleLogFile("logs/storage");

    logging::add_common_attributes(); //necessary for timestamp

}

void Logger::createModuleLogFile(std::string module)
{
    logging::formatter module_log_fmt = expr::stream
        << "[" << expr::format_date_time<boost::posix_time::ptime>("TimeStamp","%Y-%m-%d %H:%M:%S")
        << "][" << severity << "]: "<< expr::smessage;

    boost::shared_ptr<text_sink> sink = boost::make_shared<text_sink>();
    sink->locked_backend()->add_stream(
        boost::make_shared<std::ofstream>(module + ".log"));
    sink->set_formatter(module_log_fmt);
    sink->set_filter(module_attr == module);
    sink->locked_backend()->auto_flush(true);
    logging::core::get()->add_sink(sink);
}


void Logger::logInfo(std::string module, std::string message)
{
    BOOST_LOG_SCOPED_THREAD_TAG("Module", module);
    BOOST_LOG_SEV(log_, info) << message;
}

void Logger::logNotification(std::string module, std::string message)
{
    BOOST_LOG_SCOPED_THREAD_TAG("Module", module);
    BOOST_LOG_SEV(log_, notification) << message;
}

void Logger::logWarning(std::string module, std::string message)
{
    BOOST_LOG_SCOPED_THREAD_TAG("Module", module);
    BOOST_LOG_SEV(log_, warning) << message;
}

void Logger::logError(std::string module, std::string message)
{
    BOOST_LOG_SCOPED_THREAD_TAG("Module", module);
    BOOST_LOG_SEV(log_, error) << message;
}

void Logger::logCritical(std::string module, std::string message)
{
    BOOST_LOG_SCOPED_THREAD_TAG("Module", module);
    BOOST_LOG_SEV(log_, critical) << message;
}


} //namespace hdtn

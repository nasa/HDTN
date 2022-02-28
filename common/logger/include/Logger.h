#ifndef _HDTN_LOG_H
#define _HDTN_LOG_H
//#define BOOST_LOG_DYN_LINK 1

#include <cstddef>
#include <fstream>
#include <iomanip>
#include <ostream>
#include <string>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/log/attributes.hpp>
#include <boost/log/attributes/scoped_attribute.hpp>
#include <boost/log/core.hpp>
#include <boost/log/expressions.hpp>
#include <boost/log/sinks.hpp>
#include <boost/log/sinks/sync_frontend.hpp>
#include <boost/log/sinks/text_file_backend.hpp>
#include <boost/log/sinks/text_ostream_backend.hpp>
#include <boost/log/sources/basic_logger.hpp>
#include <boost/log/sources/record_ostream.hpp>
#include <boost/log/sources/severity_channel_logger.hpp>
#include <boost/log/sources/severity_logger.hpp>
#include <boost/log/support/date_time.hpp>
#include <boost/log/trivial.hpp>
#include <boost/log/utility/setup/common_attributes.hpp>
#include <boost/log/utility/setup/file.hpp>
#include <boost/log/utility/value_ref.hpp>
#include <boost/smart_ptr/make_shared_object.hpp>
#include <boost/smart_ptr/shared_ptr.hpp>
#include "log_lib_export.h"

namespace hdtn{


//Custom Severity Levels
enum severity_level
{
    info,
    notification,
    warning,
    error,
    critical
};




//Module Tags
/*enum module
{
    egress,
    ingress,
    storage
};*/

//typedef boost::log::sinks::synchronous_sink<boost::log::sinks::text_ostream_backend> text_sink;
typedef boost::log::sinks::synchronous_sink<boost::log::sinks::text_file_backend> sink_t;

class Logger
{
public:
    LOG_LIB_EXPORT static Logger* getInstance();
    LOG_LIB_EXPORT void logInfo(const std::string & module, const std::string & message);
    LOG_LIB_EXPORT void logNotification(const std::string & module, const std::string & message);
    LOG_LIB_EXPORT void logWarning(const std::string & module, const std::string & message);
    LOG_LIB_EXPORT void logError(const std::string & module, const std::string & message);
    LOG_LIB_EXPORT void logCritical(const std::string & module, const std::string & message);

private:
    LOG_LIB_EXPORT Logger();
    LOG_LIB_EXPORT Logger(Logger const&);
    LOG_LIB_EXPORT Logger& operator=(Logger const&);
    LOG_LIB_EXPORT virtual ~Logger();

    LOG_LIB_EXPORT void init();
    LOG_LIB_EXPORT void createModuleLogFile(const std::string & module);
    LOG_LIB_EXPORT void createSeverityLogFile(hdtn::severity_level level);
    boost::log::sources::severity_logger_mt<severity_level> log_; //mt for multithreaded
    static Logger* logger_; //singleton instance
};
}

#endif 

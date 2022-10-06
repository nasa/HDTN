/***************************************************************************
 * NASA Glenn Research Center, Cleveland, OH
 * Released under the NASA Open Source Agreement (NOSA)
 * May  2021
 *
 ***************************************************************************
 */

#ifndef _HDTN_LOG_H
#define _HDTN_LOG_H

#include <cstddef>
#include <fstream>
#include <iomanip>
#include <ostream>
#include <string>
#include <boost/core/null_deleter.hpp>
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

/*
 * Logging macros
 */
#define LOG_TRACE(module) \
    if (LOG_LEVEL > LOG_LEVEL_TRACE) {} \
    else _LOG_INTERNAL(module, trace)

#define LOG_DEBUG(module) \
    if (LOG_LEVEL > LOG_LEVEL_DEBUG) {} \
    else _LOG_INTERNAL(module, debug)

#define LOG_INFO(module) \
    if (LOG_LEVEL > LOG_LEVEL_INFO) {} \
    else _LOG_INTERNAL(module, info)

#define LOG_WARNING(module) \
    if (LOG_LEVEL > LOG_LEVEL_WARNING) {} \
    else _LOG_INTERNAL(module, warning)

#define LOG_ERROR(module) \
    if (LOG_LEVEL > LOG_LEVEL_ERROR) {} \
    else _LOG_INTERNAL(module, error)

#define LOG_FATAL(module) \
    if (LOG_LEVEL > LOG_LEVEL_FATAL) {} \
    else _LOG_INTERNAL(module, fatal)

#define _LOG_INTERNAL(module, level)\
    hdtn::Logger::ensureInitialized();\
    BOOST_LOG_SCOPED_THREAD_TAG("Module", module);\
    BOOST_LOG_TRIVIAL(level)

typedef boost::log::sinks::synchronous_sink<boost::log::sinks::text_file_backend> sink_t;

/**
 * @brief Logger class used to create log files and log messages.
 */
class Logger
{
public:
    LOG_LIB_EXPORT static void ensureInitialized();
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
    LOG_LIB_EXPORT ~Logger();

    LOG_LIB_EXPORT void init();
    LOG_LIB_EXPORT void createModuleLogFile(const std::string & module);
    LOG_LIB_EXPORT void createConsoleLogSink();

    /**
     * Creates a new log file for the requested severity level.
     * @param level The severity level of the logs stored in this file.
     */ 
    LOG_LIB_EXPORT void createSeverityLogFile(boost::log::trivial::severity_level level);

    boost::log::sources::severity_logger_mt<boost::log::trivial::severity_level> log_; //mt for multithreaded
    static Logger* logger_; //singleton instance
};
}

#endif 

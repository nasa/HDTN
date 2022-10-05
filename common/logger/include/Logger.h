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
#define LOG_TRACE \
    if (LOG_LEVEL > LOG_LEVEL_TRACE) {} \
    else BOOST_LOG_TRIVIAL(trace)

#define LOG_DEBUG \
    if (LOG_LEVEL > LOG_LEVEL_DEBUG) {} \
    else BOOST_LOG_TRIVIAL(debug)

#define LOG_INFO \
    if (LOG_LEVEL > LOG_LEVEL_INFO) {} \
    else BOOST_LOG_TRIVIAL(info)

#define LOG_WARNING \
    if (LOG_LEVEL > LOG_LEVEL_WARNING) {} \
    else BOOST_LOG_TRIVIAL(warning)

#define LOG_ERROR \
    if (LOG_LEVEL > LOG_LEVEL_ERROR) {} \
    else BOOST_LOG_TRIVIAL(error)

#define LOG_FATAL \
    if (LOG_LEVEL > LOG_LEVEL_FATAL) {} \
    else BOOST_LOG_TRIVIAL(fatal)

typedef boost::log::sinks::synchronous_sink<boost::log::sinks::text_file_backend> sink_t;

/**
 * @brief Logger class used to create log files and log messages.
 */
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
    LOG_LIB_EXPORT ~Logger();

    LOG_LIB_EXPORT void init();
    LOG_LIB_EXPORT void createLogFileSinkForModule(const std::string & module);
    LOG_LIB_EXPORT void createMainLogFileSink();
    LOG_LIB_EXPORT void createConsoleLogSink();

    /**
     * Creates a new log file for the requested severity level.
     * @param level The severity level of the logs stored in this file.
     */ 
    LOG_LIB_EXPORT void createLogFileSinkForSeverity(boost::log::trivial::severity_level level);

    static Logger* logger_; //singleton instance
};
}

#endif 

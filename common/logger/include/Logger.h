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

/**
 * The following macros provide access for logging at various levels
 */
#if LOG_LEVEL > LOG_LEVEL_TRACE
    #define LOG_TRACE(module) _NO_OP
#else
    #define LOG_TRACE(module) _LOG_INTERNAL(module, trace)
#endif
   
#if LOG_LEVEL > LOG_LEVEL_DEBUG
    #define LOG_DEBUG(module) _NO_OP
#else
    #define LOG_DEBUG(module) _LOG_INTERNAL(module, debug)
#endif

#if LOG_LEVEL > LOG_LEVEL_INFO
    #define LOG_INFO(module) _NO_OP
#else
    #define LOG_INFO(module) _LOG_INTERNAL(module, info)
#endif

#if LOG_LEVEL > LOG_LEVEL_WARNING
    #define LOG_WARNING(module) _NO_OP
#else
    #define LOG_WARNING(module) _LOG_INTERNAL(module, warning)
#endif

#if LOG_LEVEL > LOG_LEVEL_ERROR
    #define LOG_ERROR(module) _NO_OP
#else
    #define LOG_ERROR(module) _LOG_INTERNAL(module, error)
#endif

#if LOG_LEVEL > LOG_LEVEL_FATAL
    #define LOG_FATAL(module) _NO_OP
#else
    #define LOG_FATAL(module) _LOG_INTERNAL(module, fatal)
#endif

/**
 * _LOG_INTERNAL performs the actual logging. It should only
 * be used internally.
 */
#define _LOG_INTERNAL(module, lvl)\
    Logger::ensureInitialized();\
    _ADD_LOG_ATTRIBUTES(module);\
    BOOST_LOG_TRIVIAL(lvl)

/**
 * _NO_OP is a macro to efficiently discard a streaming expression. Since the "else"
 * branch is never taken, the compiler should optimize it out.
 */
#define _NO_OP if (true) {} else std::cout

/**
 * _ADD_LOG_ATTRIBUTES adds attributes to the logger
 */
#define _ADD_LOG_ATTRIBUTES(module)\
    boost::log::trivial::logger::get().add_attribute("File",\
         boost::log::attributes::constant<std::string>(__FILE__));\
    boost::log::trivial::logger::get().add_attribute("Module",\
        boost::log::attributes::constant<std::string>(module));\
    boost::log::trivial::logger::get().add_attribute("Line",\
        boost::log::attributes::constant<unsigned int>(__LINE__))

typedef boost::log::sinks::synchronous_sink<boost::log::sinks::text_file_backend> sink_t;

/**
 * @brief Logger class used to create log files and log messages.
 */
class Logger
{
public:
    /**
     * Initializes the logger if it hasn't been created yet. This is intended to be called from
     * the LOG_* macros.
     */
    LOG_LIB_EXPORT static void ensureInitialized();

    // Deprecated -- use LOG_* macros instead.
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

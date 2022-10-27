/***************************************************************************
 * NASA Glenn Research Center, Cleveland, OH
 * Released under the NASA Open Source Agreement (NOSA)
 * May  2021
 *
 ***************************************************************************
 */

#ifndef _HDTN_STATS_H
#define _HDTN_STATS_H

#include <boost/log/attributes.hpp>
#include <boost/log/core.hpp>
#include <boost/log/expressions.hpp>
#include <boost/log/sinks.hpp>
#include <boost/log/sinks/sync_frontend.hpp>
#include <boost/log/sources/record_ostream.hpp>
#include <boost/log/sources/logger.hpp>
#include <boost/log/support/date_time.hpp>
#include <boost/log/utility/setup/common_attributes.hpp>
#include "NoOpStream.h"
#include "stats_lib_export.h"

namespace hdtn{

/**
 * Macro that supports logging a stat value via
 * an ostream operator 
 */
#ifdef DO_STATS_LOGGING
    #define LOG_STAT(metric_name) _LOG_STAT(metric_name)
#else
    #define LOG_STAT(metric_name) _NO_OP_STREAM
#endif

#define _LOG_STAT(metric_name)\
    hdtn::StatsLogger::ensureInitialized();\
    hdtn::StatsLogger::metric_name_attr.set(metric_name);\
    BOOST_LOG(hdtn::StatsLogger::logger_)

/**
 * @brief StatsLogger class used to create log file for metrics
 */
class StatsLogger
{
public:
    /**
     * Initializes the StatsLogger if it hasn't been created yet
     */
    STATS_LIB_EXPORT static void ensureInitialized();

    /**
     * Underlying log source. This needs to be public so it can be called
     * from the macro, but shouldn't be used directly 
     */
    STATS_LIB_EXPORT static boost::log::sources::logger_mt logger_;

     /**
      * Thread-safe attributes used for logging.
      * Shared lock for read, exclusive lock for write.
     */
    typedef boost::log::attributes::mutable_constant<
        std::string,
        boost::shared_mutex,
        boost::unique_lock< boost::shared_mutex >,
        boost::shared_lock< boost::shared_mutex >
    > metric_name_attr_t;
    STATS_LIB_EXPORT static metric_name_attr_t metric_name_attr;

    STATS_LIB_EXPORT ~StatsLogger();
private:
    STATS_LIB_EXPORT StatsLogger();
    STATS_LIB_EXPORT StatsLogger(StatsLogger const&) = delete;
    STATS_LIB_EXPORT StatsLogger& operator=(StatsLogger const&) = delete;

    /**
     * Registers attributes used for log messages
     */
    STATS_LIB_EXPORT void registerAttributes();

    /**
     * Creates a multi-file sink for the metric_name attribute. Used
     * to split stats into separate files. 
     */
    STATS_LIB_EXPORT void createMultiFileLogSinkForMetricNameAttr();

    /**
     * Attributes for managing singleton instance 
     */
    static std::unique_ptr<StatsLogger> StatsLogger_; //singleton instance
    static boost::mutex mutexSingletonInstance_;
    static volatile bool StatsLoggerSingletonFullyInitialized_;
};
}

#endif 

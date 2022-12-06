/***************************************************************************
 * NASA Glenn Research Center, Cleveland, OH
 * Released under the NASA Open Source Agreement (NOSA)
 * May  2021
 *
 ***************************************************************************
 */

#ifndef _HDTN_STATS_H
#define _HDTN_STATS_H

#include <map>

#include <boost/log/attributes.hpp>
#include <boost/log/core.hpp>
#include <boost/log/expressions.hpp>
#include <boost/log/sinks.hpp>
#include <boost/log/sinks/sync_frontend.hpp>
#include <boost/log/sources/record_ostream.hpp>
#include <boost/log/sources/logger.hpp>
#include <boost/log/support/date_time.hpp>
#include <boost/log/utility/setup/common_attributes.hpp>
#include <boost/phoenix/function.hpp>
#include "stats_lib_export.h"

namespace hdtn{

/**
 * _NO_OP_STREAM_STATS is a macro to efficiently discard a streaming expression. Since the "else"
 * branch is never taken, the compiler should optimize it out.
 */
#define _NO_OP_STREAM_STATS if (true) {} else std::cout

/**
 * Macro that supports logging a stat value via
 * the ostream operator 
 */
#ifdef DO_STATS_LOGGING
    #define LOG_STAT(metricName) _LOG_STAT(metricName)
#else
    #define LOG_STAT(metricName) _NO_OP_STREAM_STATS
#endif

#define _LOG_STAT(metricName)\
    hdtn::StatsLogger::ensureInitialized(metricName);\
    hdtn::StatsLogger::metric_name_attr.set(metricName);\
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
    STATS_LIB_EXPORT static void ensureInitialized(std::string metricName);

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
    STATS_LIB_NO_EXPORT StatsLogger();
    STATS_LIB_NO_EXPORT StatsLogger(StatsLogger const&) = delete;
    STATS_LIB_NO_EXPORT StatsLogger& operator=(StatsLogger const&) = delete;

    /**
     * Registers attributes used for log messages
     */
    STATS_LIB_NO_EXPORT void registerAttributes();

    /**
     * Creates a file sink for the given metric name. Used
     * to split stats into separate files. 
     */
    STATS_LIB_NO_EXPORT void createFileSinkForMetric(std::string metricName);

    /**
     * Follows the boost open handler interface. Writes a header to the csv file on open.
     */
    STATS_LIB_NO_EXPORT static void writeHeader(boost::log::sinks::text_file_backend::stream_type& file);

    /**
     * Attributes for managing singleton instance 
     */
    static std::unique_ptr<StatsLogger> StatsLogger_; //singleton instance
    static boost::mutex mutexSingletonInstance_;
    static volatile bool StatsLoggerSingletonFullyInitialized_;
    static std::map<std::string, bool> m_initializedMetrics;

    /**
     * Log formatters 
     */
    struct timestampMs_t {
        int64_t operator()(boost::log::value_ref<boost::posix_time::ptime> const & date) const;
    };
    static boost::phoenix::function<StatsLogger::timestampMs_t> timestampMsFormatter;
};
}

#endif 

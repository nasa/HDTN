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
#include <iostream>

#include <boost/log/attributes.hpp>
#include <boost/log/core.hpp>
#include <boost/log/expressions.hpp>
#include <boost/log/sinks.hpp>
#include <boost/log/sinks/sync_frontend.hpp>
#include <boost/log/sources/record_ostream.hpp>
#include <boost/log/sources/logger.hpp>
#include <boost/log/support/date_time.hpp>
#include <boost/log/utility/manipulators/add_value.hpp>
#include <boost/log/utility/setup/common_attributes.hpp>
#include <boost/phoenix/function.hpp>
#include "stats_lib_export.h"

namespace hdtn{

/**
 * @brief StatsLogger class used to create log file for metrics
 */
class StatsLogger
{
public:
    /**
     * Represents a metric name/value pair. Handles storing and logging either
     * an int or float value
     */
    struct metric_t {
        public:
            STATS_LIB_EXPORT metric_t(std::string name, uint64_t val);
            STATS_LIB_EXPORT metric_t(std::string name, double val);
            STATS_LIB_EXPORT friend std::ostream& operator<< (std::ostream& strm, const StatsLogger::metric_t m);
            std::string name;

        private:
            uint64_t intval;
            double floatval;
            bool isFloat;
    };

    STATS_LIB_EXPORT static void Log(
        const std::string& fileName,
        const std::vector<StatsLogger::metric_t>& metrics
    );

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
     * Creates a file sink for the given file name. Used
     * to split stats into separate files. 
     */
    STATS_LIB_NO_EXPORT void createFileSink(
        const std::string &fileName,
        const std::vector<StatsLogger::metric_t>& metrics
    );

    /**
     * Initializes the StatsLogger if it hasn't been created yet
     */
    STATS_LIB_NO_EXPORT static void ensureInitialized(
        const std::string& fileName,
        const std::vector<StatsLogger::metric_t>& metrics
    );

    /**
     * Writes the header to a new log file 
     */
    STATS_LIB_NO_EXPORT static void writeHeader(
        const std::string& fileName,
        const std::vector<StatsLogger::metric_t>& metrics
    );

    /**
     * Underlying log source
     */
    STATS_LIB_EXPORT static boost::log::sources::logger_mt m_logger;

     /**
      * Thread-safe attributes used for logging.
      * Shared lock for read, exclusive lock for write.
     */
    typedef boost::log::attributes::mutable_constant<
        std::string,
        boost::shared_mutex,
        boost::unique_lock< boost::shared_mutex >,
        boost::shared_lock< boost::shared_mutex >
    > file_name_attr_t;
    STATS_LIB_EXPORT static file_name_attr_t file_name_attr;

    /**
     * Attributes for managing singleton instance 
     */
    static std::unique_ptr<StatsLogger> StatsLogger_; //singleton instance
    static boost::mutex mutexSingletonInstance_;
    static volatile bool StatsLoggerSingletonFullyInitialized_;
    static std::map<std::string, bool> m_initializedFiles;

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

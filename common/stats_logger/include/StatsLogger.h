/**
 * @file StatsLogger.h
 *
 * @copyright Copyright (c) 2026 United States Government as represented by
 * the National Aeronautics and Space Administration.
 * No copyright is claimed in the United States under Title 17, U.S.Code.
 * All Other Rights Reserved.
 *
 * @section LICENSE
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * @section DESCRIPTION
 *
 * This file is used to create log file for metrics.
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
#include <atomic>
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

    typedef boost::log::sinks::synchronous_sink< boost::log::sinks::text_file_backend > sink_t;

    /**
     * Logs a set of metrics to the specified file. A new file is created if it doesn't yet exist. 
     */
    STATS_LIB_EXPORT static void Log(
        const std::string& fileName,
        const std::vector<StatsLogger::metric_t>& metrics
    );

    /**
     * Clears all of the file sinks associated with this logger
     */
    STATS_LIB_EXPORT static void Reset();

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
    STATS_LIB_NO_EXPORT boost::shared_ptr< StatsLogger::sink_t > createFileSink(
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
    static std::atomic<bool> StatsLoggerSingletonFullyInitialized_;
    static std::map<std::string, boost::shared_ptr<StatsLogger::sink_t>> m_initializedFiles;

    /**
     * Log formatters 
     */
    struct timestampMs_t {
        int64_t operator()(boost::log::value_ref<boost::posix_time::ptime> const & date) const;
    };
    boost::phoenix::function<StatsLogger::timestampMs_t> m_timestampMsFormatter;
};
}

#endif 

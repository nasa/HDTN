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
    #define LOG_STAT(name) _LOG_STAT(name)
#else
    #define LOG_STAT(name) _NO_OP_STREAM
#endif

#define _LOG_STAT(name)\
    hdtn::StatsWriter::ensureInitialized();\
    hdtn::StatsWriter::name_attr.set(name);\
    BOOST_LOG(hdtn::StatsWriter::logger_)

/**
 * @brief StatsWriter class used to create log file for metrics
 */
class StatsWriter
{
public:
    /**
     * Initializes the StatsWriter if it hasn't been created yet
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
    > name_attr_t;
    STATS_LIB_EXPORT static name_attr_t name_attr;

    STATS_LIB_EXPORT ~StatsWriter();
private:
    STATS_LIB_EXPORT StatsWriter();
    STATS_LIB_EXPORT StatsWriter(StatsWriter const&) = delete;
    STATS_LIB_EXPORT StatsWriter& operator=(StatsWriter const&) = delete;

    /**
     * Registers attributes used for log messages
     */
    STATS_LIB_EXPORT void registerAttributes();

    /**
     * Creates a multi-file sink for the name attribute. Used
     * to split stats into separate files. 
     */
    STATS_LIB_EXPORT void createMultiFileLogSinkForNameAttr();

    /**
     * Attributes for managing singleton instance 
     */
    static std::unique_ptr<StatsWriter> statsWriter_; //singleton instance
    static boost::mutex mutexSingletonInstance_;
    static volatile bool statsWriterSingletonFullyInitialized_;
};
}

#endif 

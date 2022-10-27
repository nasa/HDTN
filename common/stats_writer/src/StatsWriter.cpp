/***************************************************************************
 * NASA Glenn Research Center, Cleveland, OH
 * Released under the NASA Open Source Agreement (NOSA)
 * May  2021
 *
 ****************************************************************************
 */

#include "StatsWriter.h"

namespace hdtn{

std::unique_ptr<StatsWriter> StatsWriter::statsWriter_; //initialized to "null"
boost::mutex StatsWriter::mutexSingletonInstance_;
volatile bool StatsWriter::statsWriterSingletonFullyInitialized_ = false;
StatsWriter::name_attr_t StatsWriter::name_attr("");
boost::log::sources::logger_mt StatsWriter::logger_;

StatsWriter::StatsWriter()
{
    registerAttributes();
    createMultiFileLogSinkForNameAttr();
    boost::log::add_common_attributes(); //necessary for timestamp
}

void StatsWriter::ensureInitialized()
{
    if (!statsWriterSingletonFullyInitialized_) { //fast way to bypass a mutex lock all the time
        //first thread that uses the stats writer gets to create the stats writer
        boost::mutex::scoped_lock theLock(mutexSingletonInstance_);
        if (!statsWriterSingletonFullyInitialized_) { //check it again now that mutex is locked
            statsWriter_.reset(new StatsWriter());
            statsWriterSingletonFullyInitialized_ = true;
        }
    }
}

void StatsWriter::registerAttributes()
{
    StatsWriter::logger_.add_attribute("Name", StatsWriter::name_attr);
}

void StatsWriter::createMultiFileLogSinkForNameAttr()
{
    boost::shared_ptr< boost::log::sinks::text_multifile_backend > backend =
        boost::make_shared< boost::log::sinks::text_multifile_backend >();
    
    backend->set_file_name_composer
    (
        boost::log::sinks::file::as_file_name_composer(
            boost::log::expressions::stream << "stats/" <<
                boost::log::expressions::attr< std::string > ("Name") << ".csv"
        )
    );

    typedef boost::log::sinks::synchronous_sink< boost::log::sinks::text_multifile_backend > sink_t;
    boost::shared_ptr< sink_t > sink = boost::make_shared<sink_t>(backend);
    sink->set_formatter
    (
        boost::log::expressions::stream
            << boost::log::expressions::attr< std::string >("Name")
            << "," << boost::log::expressions::format_date_time<boost::posix_time::ptime>("TimeStamp","%Y-%m-%d %H:%M:%S")
            << "," << boost::log::expressions::smessage
    );
    boost::log::core::get()->add_sink(sink);
}

StatsWriter::~StatsWriter(){}

}
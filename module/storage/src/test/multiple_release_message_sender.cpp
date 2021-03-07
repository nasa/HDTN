#include <boost/asio.hpp>
//#include <boost/thread.hpp>
//#include <boost/program_options.hpp>
#include <boost/algorithm/string.hpp>
#include <cstdlib>
#include <iostream>

#include "message.hpp"
#include "paths.hpp"
//#include "reg.hpp"
#include "Environment.h"
#include "JsonSerializable.h"

//#include <boost/make_shared.hpp>
#include <boost/foreach.hpp>
#include <boost/date_time.hpp>

typedef boost::shared_ptr<boost::asio::deadline_timer> SmartDeadlineTimer;

struct ReleaseMessageEvent_t {
    int id;
    int delay;
    std::string message;
};
typedef std::vector<ReleaseMessageEvent_t> ReleaseMessageEventVector_t;

void print(const boost::system::error_code&, int id) {

  // Get current system time
  boost::posix_time::ptime timeLocal = boost::posix_time::second_clock::local_time();
  std::cout <<  "id: " << id << " , expiry time: " << timeLocal << std::endl << std::flush;


}



int main(int argc, char *argv[]) {

    ReleaseMessageEventVector_t releaseMessageEventVector;

    std::string jsonFileName = (Environment::GetPathHdtnSourceRoot() /
      "module/storage/src/test/releaseMessages1.json").string();

    boost::property_tree::ptree pt = JsonSerializable::GetPropertyTreeFromJsonFile(jsonFileName);

    const boost::property_tree::ptree & releaseMessageEventsPt = pt.get_child("releaseMessageEvents", boost::property_tree::ptree()); //non-throw version
    releaseMessageEventVector.resize(releaseMessageEventsPt.size());
    unsigned int eventIndex = 0;
    BOOST_FOREACH(const boost::property_tree::ptree::value_type & eventPt, releaseMessageEventsPt) {
        ReleaseMessageEvent_t & releaseMessageEvent = releaseMessageEventVector[eventIndex++];
        releaseMessageEvent.message = eventPt.second.get<std::string>("message", "no_message"); //non-throw version
        releaseMessageEvent.id = eventPt.second.get<int>("id",0); //non-throw version
        releaseMessageEvent.delay = eventPt.second.get<int>("delay",0); //non-throw version
    }

    std::cout << "Running muliple_release_message_sender.  " << std::endl << std::flush;
    std::cout << "jsonFileName:  " << jsonFileName << std::endl << std::flush;

    for(int i=0; i<releaseMessageEventVector.size(); ++i) {
        std::cout << "id: " << releaseMessageEventVector[i].id;
        std::cout << ", delay: " << releaseMessageEventVector[i].delay;
        std::cout << ", message: " << releaseMessageEventVector[i].message << std::endl << std::flush;
    }

    boost::asio::io_service ioService;

    std::vector<SmartDeadlineTimer> vectorTimers;
    for(int i=0; i<releaseMessageEventVector.size(); ++i) {
        SmartDeadlineTimer dt(new boost::asio::deadline_timer(ioService));
        dt->expires_from_now(boost::posix_time::seconds(releaseMessageEventVector[i].delay));
       // dt->async_wait(boost::bind(&print,releaseMessageEventVector[i].id));


        dt->async_wait(boost::bind(print,boost::asio::placeholders::error, releaseMessageEventVector[i].id));

        vectorTimers.push_back(dt);
    }

    std::cout << "before run: " << std::endl << std::flush;

    ioService.run();

    std::cout << "after run: " << std::endl << std::flush;

    return 0;
}

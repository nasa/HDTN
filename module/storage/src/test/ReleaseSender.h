#ifndef RELEASESENDER_H
#define RELEASESENDER_H

#include <boost/asio.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/foreach.hpp>
#include <boost/date_time.hpp>
#include <boost/program_options.hpp>
#include <boost/make_unique.hpp>
#include <cstdlib>
#include <iostream>
#include "message.hpp"
#include "paths.hpp"
#include "Environment.h"
#include "JsonSerializable.h"
#include "reg.hpp"

typedef std::unique_ptr<boost::asio::deadline_timer> SmartDeadlineTimer;
struct ReleaseMessageEvent_t {
    int id;
    int delay;
    std::string message;
};
typedef std::vector<ReleaseMessageEvent_t> ReleaseMessageEventVector_t;

class ReleaseSender {
public:
    static const std::string DEFAULT_FILE;
    ReleaseSender();
    ~ReleaseSender();
    int ProcessEventFile(std::string jsonEventFileName);
    int ProcessComandLine(int argc, char *argv[], std::string& jsonEventFileName);
    static std::string GetFullyQualifiedFilename(std::string filename) {
        return (Environment::GetPathHdtnSourceRoot() / "module/storage/src/test/").string() + filename;
    }
    volatile bool m_timersFinished;
private:
    void ProcessEvent(const boost::system::error_code&, int id, std::string message, zmq::socket_t * ptrSocket);
};

#endif // RELEASESENDER_H

/**
 * @file ReleaseSender.h
 * @author  Jeff Follo
 *
 * @copyright Copyright © 2021 United States Government as represented by
 * the National Aeronautics and Space Administration.
 * No copyright is claimed in the United States under Title 17, U.S.Code.
 * All Other Rights Reserved.
 *
 * @section LICENSE
 * Released under the NASA Open Source Agreement (NOSA)
 * See LICENSE.md in the source root directory for more information.
 */

#ifndef RELEASESENDER_H
#define RELEASESENDER_H

#include <boost/asio.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/foreach.hpp>
#include <boost/date_time.hpp>
#include <boost/program_options.hpp>
#include <boost/make_unique.hpp>
#include <cstdlib>
#include "message.hpp"
#include "Environment.h"
#include "JsonSerializable.h"
#include "HdtnConfig.h"
#include "zmq.hpp"

typedef std::unique_ptr<boost::asio::deadline_timer> SmartDeadlineTimer;
struct ReleaseMessageEvent_t {
    cbhe_eid_t finalDestEid;
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
    int ProcessComandLine(int argc, const char *argv[], std::string& jsonEventFileName);
    static std::string GetFullyQualifiedFilename(std::string filename) {
        return (Environment::GetPathHdtnSourceRoot() / "module/storage/src/test/").string() + filename;
    }
    volatile bool m_timersFinished;
private:
    void ProcessEvent(const boost::system::error_code&, const cbhe_eid_t finalDestinationEid, std::string message, zmq::socket_t * ptrSocket);

    HdtnConfig m_hdtnConfig;
};

#endif // RELEASESENDER_H

/**
 * @file TelemetryConnectionPoller.h
 *
 * @copyright Copyright Ⓒ 2021 United States Government as represented by
 * the National Aeronautics and Space Administration.
 * No copyright is claimed in the United States under Title 17, U.S.Code.
 * All Other Rights Reserved.
 *
 * @section LICENSE
 * Released under the NASA Open Source Agreement (NOSA)
 * See LICENSE.md in the source root directory for more information.
 *
 * @section DESCRIPTION
 *
 * This TelemetryConnectionPoller class implements polling a set of TelemetryConnection objects in order to multiplex
 * input/output events.
 */

#ifndef TELEMETRY_POLLER_H
#define TELEMETRY_POLLER_H 1

#include <map>

#include "zmq.hpp"

#include "telem_lib_export.h"
#include "TelemetryConnection.h"

class TelemetryConnectionPoller
{
    public:
        TELEM_LIB_EXPORT ~TelemetryConnectionPoller();
        TELEM_LIB_EXPORT void AddConnection(TelemetryConnection& connection);
        TELEM_LIB_EXPORT bool PollConnections(unsigned int timeout);
        TELEM_LIB_EXPORT bool HasNewMessage(TelemetryConnection& connection);

        std::vector<zmq::pollitem_t> m_pollItems;
    private:
        TELEM_LIB_NO_EXPORT zmq::pollitem_t* FindPollItem(TelemetryConnection& connection);
        std::map<void*, unsigned int> m_connectionHandleToPollItemLocMap;
};

#endif //TELEMETRY_POLLER_H

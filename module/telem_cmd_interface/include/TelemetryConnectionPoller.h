/**
 * @file TelemetryConnectionPoller.h
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
 * This TelemetryConnectionPoller class implements polling a set of TelemetryConnection objects in order to multiplex
 * input/output events. This class wraps the zmq::poll functionality.
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

        /**
         * Adds a new connection to the poller
         * @param connection the connection to add
         */
        TELEM_LIB_EXPORT void AddConnection(TelemetryConnection& connection);

        /**
         * Polls all connections that have been added to the poller. Utilizes zmq::poll to multiplex
         * i/o.
         * @param timeout the max amount of time PollConnections will block while waiting for new
         * messages
         */
        TELEM_LIB_EXPORT bool PollConnections(unsigned int timeout);

        /**
         * Determines if a connection has a new message
         * @param connection the connection to check 
         */
        TELEM_LIB_EXPORT bool HasNewMessage(TelemetryConnection& connection);

        /**
         * m_pollItems should not be used directly, but are publicly available for unit testing 
         */
        std::vector<zmq::pollitem_t> m_pollItems;
    private:
        TELEM_LIB_NO_EXPORT zmq::pollitem_t* FindPollItem(TelemetryConnection& connection);
        std::map<void*, unsigned int> m_connectionHandleToPollItemLocMap;
};

#endif //TELEMETRY_POLLER_H

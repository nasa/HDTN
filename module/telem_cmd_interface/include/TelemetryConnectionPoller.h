/**
 * @file TelemetryConnectionPoller.h
 *
 * @copyright Copyright (c) 2023 United States Government as represented by
 * the National Aeronautics and Space Administration.
 * No copyright is claimed in the United States under Title 17, U.S.Code.
 * All Other Rights Reserved.
 *
 * @section LICENSE
 * Released under the NASA Open Source Agreement (NOSA)
 * See LICENSE.md in the source root directory for more information.
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

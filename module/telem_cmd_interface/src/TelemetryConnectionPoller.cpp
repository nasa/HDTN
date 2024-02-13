/**
 * @file TelemetryConnectionPoller.cpp
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
 */

#include "zmq.hpp"

#include "TelemetryConnectionPoller.h"
#include "TelemetryConnection.h"
#include "Logger.h"


hdtn::Logger::SubProcess subprocess = hdtn::Logger::SubProcess::telem;


void TelemetryConnectionPoller::AddConnection(TelemetryConnection& connection)
{   
    zmq::pollitem_t pollItem = {connection.GetSocketHandle(), 0, ZMQ_POLLIN, 0};
    m_connectionHandleToPollItemLocMap[connection.GetSocketHandle()] = static_cast<unsigned int>(m_pollItems.size());
    m_pollItems.push_back(pollItem);
}

bool TelemetryConnectionPoller::PollConnections(unsigned int timeout)
{
    try {
        int rc = zmq::poll(&m_pollItems[0], m_pollItems.size(), timeout);
        return rc > 0;
    }
    catch (zmq::error_t & e) {
        LOG_ERROR(subprocess) << "caught zmq::error_t: " << e.what();
        return false;
    }
}

bool TelemetryConnectionPoller::HasNewMessage(TelemetryConnection& connection)
{
    zmq::pollitem_t* item = FindPollItem(connection);
    if (!item) {
        return false;
    }
    return item->revents & ZMQ_POLLIN;
}

zmq::pollitem_t* TelemetryConnectionPoller::FindPollItem(TelemetryConnection& connection)
{
    std::map<void*, unsigned int>::const_iterator it = m_connectionHandleToPollItemLocMap.find(connection.GetSocketHandle());
    if (it != m_connectionHandleToPollItemLocMap.cend()) {
        unsigned int loc = it->second;
        return &m_pollItems[loc];
    }
    return NULL;
}

TelemetryConnectionPoller::~TelemetryConnectionPoller()
{
    m_pollItems.clear();
    m_connectionHandleToPollItemLocMap.clear();
}

#include "zmq.hpp"

#include "TelemetryConnectionPoller.h"
#include "TelemetryConnection.h"
#include "Logger.h"


hdtn::Logger::SubProcess subprocess = hdtn::Logger::SubProcess::telem;


void TelemetryConnectionPoller::AddConnection(TelemetryConnection& connection)
{   
    zmq::pollitem_t pollItem = {connection.GetSocketHandle(), 0, ZMQ_POLLIN, 0};
    m_connectionHandleToPollItemLocMap[connection.GetSocketHandle()] = m_pollItems.size();
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
    if (m_connectionHandleToPollItemLocMap.count(connection.GetSocketHandle())) {
        unsigned int loc = m_connectionHandleToPollItemLocMap[connection.GetSocketHandle()];
        return &m_pollItems[loc];
    }
    return NULL;
}

TelemetryConnectionPoller::~TelemetryConnectionPoller()
{
    m_pollItems.clear();
    m_connectionHandleToPollItemLocMap.clear();
}

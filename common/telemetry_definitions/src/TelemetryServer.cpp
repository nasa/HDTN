/**
 * @file TelemetryServer.cpp
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

#include "TelemetryServer.h"
#include "Logger.h"

static constexpr hdtn::Logger::SubProcess subprocess = hdtn::Logger::SubProcess::none;

static void CustomCleanupStdString(void* data, void* hint) {
    delete static_cast<std::string*>(hint);
}

static void CustomCleanupSharedPtrStdString(void* data, void* hint) {
    std::shared_ptr<std::string>* serializedRawPtrToSharedPtr = static_cast<std::shared_ptr<std::string>* >(hint);
    delete serializedRawPtrToSharedPtr; //reduce ref count and delete shared_ptr object
}


/**
 * TelemetryRequest 
 */

TelemetryRequest::TelemetryRequest(bool error) : m_error(error), m_more(false) {}

TelemetryRequest::TelemetryRequest(bool error, bool more, std::string& message, zmq::message_t& connectionId) : m_error(error), m_more(more) {
    m_connectionId.copy(connectionId);
    m_cmd = ApiCommand_t::CreateFromJson(message);
    if (!m_cmd) {
        m_error = true;
    }
}

void TelemetryRequest::SendResponse(std::shared_ptr<std::string> strPtr, std::unique_ptr<zmq::socket_t>& socket) {
    std::shared_ptr<std::string>* jsonRawPtrToSharedPtr = new std::shared_ptr<std::string>(std::move(strPtr));
    std::shared_ptr<std::string>& sharedPtrRef = *jsonRawPtrToSharedPtr;
    std::string& strAoctRef = *sharedPtrRef;
    zmq::message_t zmqTelemMessageWithDataStolen(&strAoctRef[0], strAoctRef.size(),
            CustomCleanupSharedPtrStdString, jsonRawPtrToSharedPtr);
    SendResponse(zmqTelemMessageWithDataStolen, socket);
}

void TelemetryRequest::SendResponse(const std::string& resp, std::unique_ptr<zmq::socket_t>& socket) {
    std::string* respPtr = new std::string(resp);
    std::string& strRef = *respPtr;
    zmq::message_t zmqJsonMessage(&strRef[0], respPtr->size(), CustomCleanupStdString, respPtr);
    SendResponse(zmqJsonMessage, socket);
}

void TelemetryRequest::SendResponse(zmq::message_t& msg, std::unique_ptr<zmq::socket_t>& socket) {
    const zmq::send_flags additionalFlags = (m_more ? zmq::send_flags::sndmore : zmq::send_flags::none);

    // Send the connection ID
    if (!socket->send(std::move(m_connectionId), zmq::send_flags::dontwait | zmq::send_flags::sndmore)) {
        LOG_ERROR(subprocess) << "can't send json telemetry to telem";
    }

    // Send the API command
    zmq::message_t apiCall = zmq::message_t(m_cmd->m_apiCall);
    if (!socket->send(std::move(apiCall), zmq::send_flags::dontwait | zmq::send_flags::sndmore)) {
        LOG_ERROR(subprocess) << "can't send json telemetry to telem";
    }

    // Send the message body
    if (!socket->send(std::move(msg), zmq::send_flags::dontwait | additionalFlags)) {
        LOG_ERROR(subprocess) << "can't send json telemetry to telem";
    }
}

void TelemetryRequest::SendResponseSuccess(std::unique_ptr<zmq::socket_t>& socket) {
    ApiResp_t response;
    response.m_success = true;
    const std::string str = response.ToJson();
    SendResponse(str, socket);
}

void TelemetryRequest::SendResponseError(const std::string& message, std::unique_ptr<zmq::socket_t>& socket) {
    ApiResp_t response;
    response.m_success = false;
    response.m_message = message;
    const std::string str = response.ToJson();
    SendResponse(str, socket);
}

bool TelemetryRequest::More() {
    return m_more;
}

bool TelemetryRequest::Error() {
    return m_error;
}

std::shared_ptr<ApiCommand_t> TelemetryRequest::Command() {
    return m_cmd;
}

/**
 * TelemetryServer
 */

TelemetryServer::TelemetryServer() {}

TelemetryRequest TelemetryServer::ReadRequest(std::unique_ptr<zmq::socket_t>& socket) {
    // First message is the connection ID
    zmq::message_t connectionId;
    if (!socket->recv(connectionId, zmq::recv_flags::dontwait)) {
        LOG_ERROR(subprocess) << "error receiving api message";
        return TelemetryRequest(true);
    }

    // Second message is the API command
    zmq::message_t apiMsg;
    if (!socket->recv(apiMsg, zmq::recv_flags::dontwait)) {
        LOG_ERROR(subprocess) << "error receiving api message";
        return TelemetryRequest(true);
    }

    std::string apiMsgAsJsonStr = apiMsg.to_string();
    bool more = apiMsg.more();
    return TelemetryRequest(false, apiMsg.more(), apiMsgAsJsonStr, connectionId);
}

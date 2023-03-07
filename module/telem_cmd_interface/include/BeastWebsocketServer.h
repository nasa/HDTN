/**
 * @file BeastWebsocketServer.h
 * @author  Brian Tomko <brian.j.tomko@nasa.gov>
 *
 * @copyright Copyright © 2022 United States Government as represented by
 * the National Aeronautics and Space Administration.
 * No copyright is claimed in the United States under Title 17, U.S.Code.
 * All Other Rights Reserved.
 *
 * @section LICENSE
 * Released under the NASA Open Source Agreement (NOSA)
 * See LICENSE.md in the source root directory for more information.
 *
 * @section DESCRIPTION
 * This BeastWebsocketServer class implements a websocket server and client application for
 * displaying telemetry metrics in a graphical user interface
 * Based on example from: 
 * //https://raw.githubusercontent.com/boostorg/beast/b07edea9d70ed5a613879ab94896ed9b7255f5a8/example/advanced/server-flex/advanced_server_flex.cpp
 */

#ifndef BEAST_WEBSOCKET_SERVER_H
#define BEAST_WEBSOCKET_SERVER_H 1

#include <cstring>
#include <memory>
#include <set>

#include <boost/thread.hpp>
#include <boost/filesystem.hpp>
#include <boost/program_options.hpp>
#include <boost/function.hpp>
#include "zmq.hpp"

#include <boost/asio/ssl.hpp>
#include "telem_lib_export.h"

#ifndef CLASS_VISIBILITY_TELEM_LIB
#  ifdef _WIN32
#    define CLASS_VISIBILITY_TELEM_LIB
#  else
#    define CLASS_VISIBILITY_TELEM_LIB TELEM_LIB_EXPORT
#  endif
#endif

class WebsocketSessionBase : private boost::noncopyable {
protected:
    WebsocketSessionBase() = delete;
    WebsocketSessionBase(uint32_t uniqueId) : m_uniqueId(uniqueId) {}
public:
    virtual ~WebsocketSessionBase() {}
    virtual void AsyncSendTextData(std::shared_ptr<std::string>&& stringPtr) = 0;
    virtual void AsyncClose() = 0;
protected:
    const uint32_t m_uniqueId;
};

typedef boost::function<void(WebsocketSessionBase& conn)> OnNewBeastWebsocketConnectionCallback_t;
typedef boost::function<bool(WebsocketSessionBase& conn, std::string& receivedString)> OnNewBeastWebsocketDataReceivedCallback_t;

class CLASS_VISIBILITY_TELEM_LIB BeastWebsocketServer {
    BeastWebsocketServer() = delete;
public:
    TELEM_LIB_EXPORT BeastWebsocketServer(boost::asio::ssl::context&& sslContext, bool sslContextIsValid);
    TELEM_LIB_EXPORT ~BeastWebsocketServer();

    TELEM_LIB_EXPORT bool Init(const boost::filesystem::path& documentRoot, const std::string& portNumberAsString,
        const OnNewBeastWebsocketConnectionCallback_t& connectionCallback, const OnNewBeastWebsocketDataReceivedCallback_t& dataCallback);
    TELEM_LIB_EXPORT void Stop();
    TELEM_LIB_EXPORT void SendTextDataToActiveWebsockets(const std::shared_ptr<std::string>& stringPtr);
    /*
    TELEM_LIB_EXPORT void SendTextDataToActiveWebsockets(const char* data, std::size_t size);
    TELEM_LIB_EXPORT void SendBinaryDataToActiveWebsockets(const char* data, std::size_t size);
*/
    

private:
    // Internal implementation class
    struct Impl;
private:
    // Pointer to the internal implementation
    std::unique_ptr<Impl> m_pimpl;
    

    
};


#endif

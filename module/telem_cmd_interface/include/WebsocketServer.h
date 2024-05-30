/**
 * @file WebsocketServer.h
 * @author  Brian Tomko <brian.j.tomko@nasa.gov>
 * @author  Ethan Schweinsberg <ethan.e.schweinsberg@nasa.gov>
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
 *
 * This WebsocketServer class provides abstraction to a generic websocket server
 * that allows a common interface regardless of whether hdtn is compiled
 * with boost::beast or CivetWeb.
 */

#ifndef WEBSOCKET_SERVER_H
#define WEBSOCKET_SERVER_H 1

#include "telem_lib_export.h"
#include <boost/filesystem/path.hpp>
#include <boost/function.hpp>
#include <boost/program_options.hpp>
#include <boost/core/noncopyable.hpp>


class WebsocketServer : private boost::noncopyable
{
    public:
        struct ProgramOptions
        {
        public:
            TELEM_LIB_EXPORT ProgramOptions();

            /**
             * Appends program options to an existing options_description object
             */
            TELEM_LIB_EXPORT static void AppendToDesc(boost::program_options::options_description& desc, const boost::filesystem::path* defaultWwwRoot = NULL);

            /**
             * Parses a variable map and stores the result
             */
            TELEM_LIB_EXPORT bool ParseFromVariableMap(boost::program_options::variables_map& vm);

            struct SslPaths {
                boost::filesystem::path m_certificatePemFile; //not preferred
                boost::filesystem::path m_certificateChainPemFile; //preferred
                boost::filesystem::path m_privateKeyPemFile;
                boost::filesystem::path m_diffieHellmanParametersPemFile;
                bool m_valid = false;
            };

        public:
            /**
             * Program options
             */
            boost::filesystem::path m_guiDocumentRoot;
            std::string m_guiPortNumber;
            SslPaths m_sslPaths;
        };

        class Connection {
        public:
            TELEM_LIB_EXPORT void SendTextDataToThisConnection(std::shared_ptr<std::string>&& stringPtr);
            TELEM_LIB_EXPORT void SendTextDataToThisConnection(const char* str, std::size_t strSize);
            TELEM_LIB_EXPORT void SendTextDataToThisConnection(std::string&& str);
            TELEM_LIB_EXPORT void SendTextDataToThisConnection(const std::string& str);
            TELEM_LIB_EXPORT void SendTextDataToThisConnection(const std::shared_ptr<std::string>& strPtr);

            // Internal implementation class and pointer
            void* m_pimpl;
        };

        typedef boost::function<void(WebsocketServer::Connection& conn)> OnNewWebsocketConnectionCallback_t;
        typedef boost::function<bool(WebsocketServer::Connection& conn, std::string& receivedString)> OnNewWebsocketTextDataReceivedCallback_t;

        TELEM_LIB_EXPORT WebsocketServer();
        TELEM_LIB_EXPORT ~WebsocketServer();

        /**
         * Starts the runner as an async thread
         * @param inprocContextPtr context to use for the inproc zmq connections
         * @param options program options for the runner
         */
        TELEM_LIB_EXPORT bool Init(const ProgramOptions& options,
            const OnNewWebsocketConnectionCallback_t& onNewWebsocketConnectionCallback,
            const OnNewWebsocketTextDataReceivedCallback_t& onNewWebsocketTextDataReceivedCallback);

        /**
         * Stops the runner
         */
        TELEM_LIB_EXPORT void Stop();
        TELEM_LIB_EXPORT bool EnabledAndValid() const noexcept;
        TELEM_LIB_EXPORT static bool IsCompiled() noexcept;
        TELEM_LIB_EXPORT static bool IsCompiledWithSsl() noexcept;

        TELEM_LIB_EXPORT void SendTextDataToActiveWebsockets(const char* str, std::size_t strSize);
        TELEM_LIB_EXPORT void SendTextDataToActiveWebsockets(std::string&& str);
        TELEM_LIB_EXPORT void SendTextDataToActiveWebsockets(const std::string& str);
        TELEM_LIB_EXPORT void SendTextDataToActiveWebsockets(const std::shared_ptr<std::string>& strPtr);

    private:
        // Internal implementation class and pointer
        class Impl;
        std::unique_ptr<Impl> m_pimpl;

        bool m_valid;
};

#endif //WEBSOCKET_SERVER_H

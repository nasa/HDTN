/**
 * @file SignalHandler.h
 * @author  Brian Tomko <brian.j.tomko@nasa.gov>
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
 *
 * This SignalHandler class provides a cross-platform means to capture CTRL-C events
 * (e.g. SIGINT, SIGTERM) and call a custom function when the event happens.
 */

#ifndef SIGNAL_HANDLER_H
#define SIGNAL_HANDLER_H 1
#include <boost/asio.hpp>
#include <boost/thread.hpp>
#include <boost/function.hpp>
#include <memory>
#include "hdtn_util_export.h"

class SignalHandler {
private:
    SignalHandler();
public:
    /**
     * Set the signal handler callback.
     * Register keyboard interrupt signals to listen for.
     * @param handleSignalFunction The signal handler callback.
     */
    HDTN_UTIL_EXPORT SignalHandler(boost::function<void() > handleSignalFunction);
    
    /// Release all underlying I/O resources
    HDTN_UTIL_EXPORT ~SignalHandler();
    
    /** Start the signal event listener.
     *
     * @param useDedicatedThread Whether to spawn a separate thread for the I/O.
     */
    HDTN_UTIL_EXPORT void Start(bool useDedicatedThread = true);

    /** Poll signal event listener.
     *
     * Only call when NOT using a dedicated I/O thread.
     * @return True if any signal events have occurred since last checked, or False otherwise.
     */
    HDTN_UTIL_EXPORT bool PollOnce();

private:
    /** Handle signal event.
     *
     * Invokes the signal event handler callback.
     */
    HDTN_UTIL_EXPORT void HandleSignal();

private:
    /// I/O execution context
    boost::asio::io_service m_ioService;
    /// Signal event listener
    boost::asio::signal_set m_signals;
    /// Thread that invokes m_ioService.run() (if using dedicated I/O thread)
    std::unique_ptr<boost::thread> m_ioServiceThreadPtr;
    /// Callback to invoke after a signal is received
    boost::function<void() > m_handleSignalFunction;
};
#endif

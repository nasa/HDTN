/**
 * @file SignalHandler.h
 * @author  Brian Tomko <brian.j.tomko@nasa.gov>
 *
 * @copyright Copyright © 2021 United States Government as represented by
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
 * This SignalHandler class provides a cross-platform means to capture CTRL-C events
 * (e.g. SIGINT, SIGTERM) and call a custom function when the event happens.
 */

#ifndef SIGNAL_HANDLER_H
#define SIGNAL_HANDLER_H 1
#include <boost/asio.hpp>
#include <boost/thread.hpp>
#include <boost/function.hpp>
#include "hdtn_util_export.h"

class SignalHandler {
private:
    SignalHandler();
public:
    HDTN_UTIL_EXPORT SignalHandler(boost::function<void() > handleSignalFunction);
    HDTN_UTIL_EXPORT ~SignalHandler();


    HDTN_UTIL_EXPORT void Start(bool useDedicatedThread = true);

    //use when useDedicatedThread is set to false
    //return true if signal handler called
    HDTN_UTIL_EXPORT bool PollOnce();

private:

    HDTN_UTIL_EXPORT void HandleSignal();

private:
    /// The io_service used to perform asynchronous operations.
    boost::asio::io_service m_ioService;

    /// The signal_set is used to register for process termination notifications.
    boost::asio::signal_set m_signals;

    boost::thread *m_ioServiceThread;

    boost::function<void() > m_handleSignalFunction;


};
#endif

#ifndef SIGNAL_HANDLER_H
#define SIGNAL_HANDLER_H
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

#include "SignalHandler.h"
#include <signal.h>
#include <iostream>

SignalHandler::SignalHandler(boost::function<void () > handleSignalFunction) :
m_ioService(), m_signals(m_ioService), m_ioServiceThread(NULL), m_handleSignalFunction(handleSignalFunction) {

	m_signals.add(SIGINT);
	m_signals.add(SIGTERM);
#if defined(SIGQUIT)
	m_signals.add(SIGQUIT);
#endif // defined(SIGQUIT)


}

SignalHandler::~SignalHandler() {
	m_ioService.stop();
	if (m_ioServiceThread) {
		m_ioServiceThread->join();
		delete m_ioServiceThread;
	}
}

void SignalHandler::Start() {
	m_signals.async_wait(boost::bind(&SignalHandler::HandleSignal, this));
	m_ioServiceThread = new boost::thread(boost::bind(&boost::asio::io_service::run, &m_ioService));
}

void SignalHandler::HandleSignal() {
	m_handleSignalFunction();
}
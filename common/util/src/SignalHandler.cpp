/**
 * @file SignalHandler.cpp
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
 */

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

void SignalHandler::Start(bool useDedicatedThread) {
	m_signals.async_wait(boost::bind(&SignalHandler::HandleSignal, this));
	if (useDedicatedThread) {
		m_ioServiceThread = new boost::thread(boost::bind(&boost::asio::io_service::run, &m_ioService));
	}
}

bool SignalHandler::PollOnce() {
	return (m_ioService.poll_one() > 0);
}

void SignalHandler::HandleSignal() {
	m_handleSignalFunction();
}
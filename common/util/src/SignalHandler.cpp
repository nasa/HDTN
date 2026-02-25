/**
 * @file SignalHandler.cpp
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
 */

#include "SignalHandler.h"
#include "Logger.h"
#include <boost/make_unique.hpp>
#include <signal.h>
#include <iostream>
#include "ThreadNamer.h"

static constexpr hdtn::Logger::SubProcess subprocess = hdtn::Logger::SubProcess::none;

SignalHandler::SignalHandler(boost::function<void () > handleSignalFunction) :
m_ioService(), m_signals(m_ioService), m_handleSignalFunction(handleSignalFunction) {

	static const std::pair<int, const char*> signalsToAdd[] = {
			{SIGINT, "SIGINT"}
			,{SIGTERM, "SIGTERM"}
#if defined(SIGQUIT)
			,{SIGQUIT, "SIGQUIT"}
#endif
	};
	static constexpr std::size_t numSignalsToAdd = sizeof(signalsToAdd) / sizeof(*signalsToAdd);
	for (std::size_t i = 0; i < numSignalsToAdd; ++i) {
		try {
			m_signals.add(signalsToAdd[i].first);
		}
		catch (const boost::system::system_error&) {
			LOG_ERROR(subprocess) << "Signal handler will be unable to catch " << signalsToAdd[i].second;
		}
	}


}

SignalHandler::~SignalHandler() {
	m_ioService.stop();
	if (m_ioServiceThreadPtr) {
		try {
			m_ioServiceThreadPtr->join();
			m_ioServiceThreadPtr.reset(); //delete it
		}
		catch (const boost::thread_resource_error&) {
			LOG_ERROR(subprocess) << "error stopping SignalHandler io_service";
		}
	}
}

void SignalHandler::Start(bool useDedicatedThread) {
	m_signals.async_wait(boost::bind(&SignalHandler::HandleSignal, this));
	if (useDedicatedThread) {
		m_ioServiceThreadPtr = boost::make_unique<boost::thread>(boost::bind(&boost::asio::io_service::run, &m_ioService));
		ThreadNamer::SetIoServiceThreadName(m_ioService, "ioServiceSignalHandler");
	}
}

bool SignalHandler::PollOnce() {
	return (m_ioService.poll_one() > 0);
}

void SignalHandler::HandleSignal() {
	m_handleSignalFunction();
}
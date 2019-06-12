#ifndef SIGNAL_HANDLER_H
#define SIGNAL_HANDLER_H
#include <boost/asio.hpp>
#include <boost/thread.hpp>
#include <boost/function.hpp>

class SignalHandler {
private:
	SignalHandler();
public:
	SignalHandler(boost::function<void () > handleSignalFunction);	
	~SignalHandler();
	

	void Start();

private:
	
	void HandleSignal();

private:
	/// The io_service used to perform asynchronous operations.
  boost::asio::io_service m_ioService;

  /// The signal_set is used to register for process termination notifications.
  boost::asio::signal_set m_signals;

  boost::thread *m_ioServiceThread;

  boost::function<void () > m_handleSignalFunction;

	
};


#endif
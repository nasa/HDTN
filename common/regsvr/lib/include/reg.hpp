#ifndef _HDTN3_REG_H
#define _HDTN3_REG_H

#include <string>
#include <list>
#include <zmq.hpp>

namespace hdtn
{

	struct hdtn_entry
	{
		std::string protocol;
		std::string address;
		std::string type;
		uint16_t port;
		std::string mode;
	};

	typedef std::list<hdtn_entry> hdtn_entries;

	class hdtn_regsvr
	{
	public:
		void init(std::string target, std::string svc, uint16_t port, std::string mode);
		bool reg();
		bool dereg();
		hdtn::hdtn_entries query(std::string target = "");

	private:
		zmq::context_t *_zmq_ctx;
		zmq::socket_t *_zmq_sock;
		std::string _type;
		std::string _mode;
		uint16_t _port;
	};

}; // namespace hdtn

#endif

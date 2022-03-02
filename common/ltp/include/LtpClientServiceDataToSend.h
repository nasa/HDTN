#ifndef LTP_CLIENT_SERVICE_DATA_TO_SEND_H
#define LTP_CLIENT_SERVICE_DATA_TO_SEND_H 1

#define LTP_CLIENT_SERVICE_DATA_TO_SEND_SUPPORT_ZMQ 1

#include <cstdint>
#include <vector>

#ifdef LTP_CLIENT_SERVICE_DATA_TO_SEND_SUPPORT_ZMQ
#include <zmq.hpp>
#endif
#include "ltp_lib_export.h"


class LtpClientServiceDataToSend {
public:
    LTP_LIB_EXPORT LtpClientServiceDataToSend(); //a default constructor: X()
    LTP_LIB_EXPORT LtpClientServiceDataToSend(std::vector<uint8_t> && vec);
    LTP_LIB_EXPORT LtpClientServiceDataToSend& operator=(std::vector<uint8_t> && vec); //a move assignment: operator=(X&&)
#ifdef LTP_CLIENT_SERVICE_DATA_TO_SEND_SUPPORT_ZMQ
    LTP_LIB_EXPORT LtpClientServiceDataToSend(zmq::message_t && zmqMessage);
    LTP_LIB_EXPORT LtpClientServiceDataToSend& operator=(zmq::message_t && zmqMessage); //a move assignment: operator=(X&&)
#endif
    LTP_LIB_EXPORT ~LtpClientServiceDataToSend(); //a destructor: ~X()
    LTP_LIB_EXPORT LtpClientServiceDataToSend(const LtpClientServiceDataToSend& o) = delete; //disallow copying
    LTP_LIB_EXPORT LtpClientServiceDataToSend(LtpClientServiceDataToSend&& o); //a move constructor: X(X&&)
    LTP_LIB_EXPORT LtpClientServiceDataToSend& operator=(const LtpClientServiceDataToSend& o) = delete; //disallow copying
    LTP_LIB_EXPORT LtpClientServiceDataToSend& operator=(LtpClientServiceDataToSend&& o); //a move assignment: operator=(X&&)
    LTP_LIB_EXPORT bool operator==(const std::vector<uint8_t> & vec) const; //operator ==
    LTP_LIB_EXPORT bool operator!=(const std::vector<uint8_t> & vec) const; //operator !=
    LTP_LIB_EXPORT uint8_t * data() const;
    LTP_LIB_EXPORT std::size_t size() const;

private:
    std::vector<uint8_t> m_vector;
#ifdef LTP_CLIENT_SERVICE_DATA_TO_SEND_SUPPORT_ZMQ
    zmq::message_t m_zmqMessage;
#endif
    uint8_t * m_ptrData;
    std::size_t m_size;
};

#endif // LTP_CLIENT_SERVICE_DATA_TO_SEND_H


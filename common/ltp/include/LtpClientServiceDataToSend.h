#ifndef LTP_CLIENT_SERVICE_DATA_TO_SEND_H
#define LTP_CLIENT_SERVICE_DATA_TO_SEND_H 1

#define LTP_CLIENT_SERVICE_DATA_TO_SEND_SUPPORT_ZMQ 1

#include <cstdint>
#include <vector>

#ifdef LTP_CLIENT_SERVICE_DATA_TO_SEND_SUPPORT_ZMQ
#include <zmq.hpp>
#endif



class LtpClientServiceDataToSend {
public:
    LtpClientServiceDataToSend(); //a default constructor: X()
    LtpClientServiceDataToSend(std::vector<uint8_t> && vec);
    LtpClientServiceDataToSend& operator=(std::vector<uint8_t> && vec); //a move assignment: operator=(X&&)
#ifdef LTP_CLIENT_SERVICE_DATA_TO_SEND_SUPPORT_ZMQ
    LtpClientServiceDataToSend(zmq::message_t && zmqMessage);
    LtpClientServiceDataToSend& operator=(zmq::message_t && zmqMessage); //a move assignment: operator=(X&&)
#endif
    ~LtpClientServiceDataToSend(); //a destructor: ~X()
    LtpClientServiceDataToSend(const LtpClientServiceDataToSend& o) = delete; //disallow copying
    LtpClientServiceDataToSend(LtpClientServiceDataToSend&& o); //a move constructor: X(X&&)
    LtpClientServiceDataToSend& operator=(const LtpClientServiceDataToSend& o) = delete; //disallow copying
    LtpClientServiceDataToSend& operator=(LtpClientServiceDataToSend&& o); //a move assignment: operator=(X&&)
    bool operator==(const std::vector<uint8_t> & vec) const; //operator ==
    bool operator!=(const std::vector<uint8_t> & vec) const; //operator !=
    uint8_t * data() const;
    std::size_t size() const;

private:
    std::vector<uint8_t> m_vector;
#ifdef LTP_CLIENT_SERVICE_DATA_TO_SEND_SUPPORT_ZMQ
    zmq::message_t m_zmqMessage;
#endif
    uint8_t * m_ptrData;
    std::size_t m_size;
};

#endif // LTP_CLIENT_SERVICE_DATA_TO_SEND_H


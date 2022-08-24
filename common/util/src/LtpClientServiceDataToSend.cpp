/**
 * @file LtpClientServiceDataToSend.cpp
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

#include "LtpClientServiceDataToSend.h"


LtpClientServiceDataToSend::LtpClientServiceDataToSend() : m_vector(0), m_ptrData(m_vector.data()), m_size(m_vector.size()) { } //default constructor
LtpClientServiceDataToSend::LtpClientServiceDataToSend(std::vector<uint8_t> && vec) : m_vector(std::move(vec)), m_ptrData(m_vector.data()), m_size(m_vector.size()) { }
LtpClientServiceDataToSend& LtpClientServiceDataToSend::operator=(std::vector<uint8_t> && vec) { //a move assignment: operator=(X&&)
    m_vector = std::move(vec);
#ifdef LTP_CLIENT_SERVICE_DATA_TO_SEND_SUPPORT_ZMQ
    if (m_zmqMessage.size()) {
        m_zmqMessage.rebuild();
    }
#endif
    m_ptrData = m_vector.data();
    m_size = m_vector.size();
    return *this;
}
#ifdef LTP_CLIENT_SERVICE_DATA_TO_SEND_SUPPORT_ZMQ
LtpClientServiceDataToSend::LtpClientServiceDataToSend(zmq::message_t && zmqMessage) : m_zmqMessage(std::move(zmqMessage)), m_ptrData((uint8_t*)m_zmqMessage.data()), m_size(m_zmqMessage.size()) { }
LtpClientServiceDataToSend& LtpClientServiceDataToSend::operator=(zmq::message_t && zmqMessage) { //a move assignment: operator=(X&&)
    m_vector.resize(0);
    m_zmqMessage = std::move(zmqMessage);
    m_ptrData = (uint8_t*)m_zmqMessage.data();
    m_size = m_zmqMessage.size();
    return *this;
}
#endif
LtpClientServiceDataToSend::~LtpClientServiceDataToSend() { } //a destructor: ~X()

LtpClientServiceDataToSend::LtpClientServiceDataToSend(LtpClientServiceDataToSend && o) : //a move constructor: X(X&&)
    m_vector(std::move(o.m_vector)),
#ifdef LTP_CLIENT_SERVICE_DATA_TO_SEND_SUPPORT_ZMQ
    m_zmqMessage(std::move(o.m_zmqMessage)),
#endif
    m_ptrData(o.m_ptrData), m_size(o.m_size) { } 

LtpClientServiceDataToSend& LtpClientServiceDataToSend::operator=(LtpClientServiceDataToSend && o) { //a move assignment: operator=(X&&)
    m_vector = std::move(o.m_vector);
#ifdef LTP_CLIENT_SERVICE_DATA_TO_SEND_SUPPORT_ZMQ
    m_zmqMessage = std::move(o.m_zmqMessage);
#endif
    m_ptrData = o.m_ptrData;
    m_size = o.m_size;
    return *this;
}
bool LtpClientServiceDataToSend::operator==(const std::vector<uint8_t> & vec) const {
    return (m_vector == vec);
}
bool LtpClientServiceDataToSend::operator!=(const std::vector<uint8_t> & vec) const {
    return (m_vector != vec);
}

uint8_t * LtpClientServiceDataToSend::data() const {
    return m_ptrData;
}
std::size_t LtpClientServiceDataToSend::size() const {
    return m_size;
}

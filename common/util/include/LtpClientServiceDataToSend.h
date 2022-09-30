/**
 * @file LtpClientServiceDataToSend.h
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
 * This LtpClientServiceDataToSend class encapsulates the bundle or user data to send and keep
 * persistent while an LTP session is alive and asynchronous UDP send operations are ongoing.
 * The class can hold a vector<uint8_t> or a ZeroMQ message.
 * Messages are intended to be moved into this class to avoid memory copies.
 * The data is then able to be destroyed once the LTP send session completes/closes.
 */

#ifndef LTP_CLIENT_SERVICE_DATA_TO_SEND_H
#define LTP_CLIENT_SERVICE_DATA_TO_SEND_H 1

#define LTP_CLIENT_SERVICE_DATA_TO_SEND_SUPPORT_ZMQ 1

#include <cstdint>
#include <vector>

#ifdef LTP_CLIENT_SERVICE_DATA_TO_SEND_SUPPORT_ZMQ
#include <zmq.hpp>
#endif
#include "hdtn_util_export.h"


class LtpClientServiceDataToSend {
public:
    HDTN_UTIL_EXPORT LtpClientServiceDataToSend(); //a default constructor: X()
    HDTN_UTIL_EXPORT LtpClientServiceDataToSend(std::vector<uint8_t> && vec);
    HDTN_UTIL_EXPORT LtpClientServiceDataToSend& operator=(std::vector<uint8_t> && vec); //a move assignment: operator=(X&&)
#ifdef LTP_CLIENT_SERVICE_DATA_TO_SEND_SUPPORT_ZMQ
    HDTN_UTIL_EXPORT LtpClientServiceDataToSend(zmq::message_t && zmqMessage);
    HDTN_UTIL_EXPORT LtpClientServiceDataToSend& operator=(zmq::message_t && zmqMessage); //a move assignment: operator=(X&&)
#endif
    HDTN_UTIL_EXPORT ~LtpClientServiceDataToSend(); //a destructor: ~X()
    HDTN_UTIL_EXPORT LtpClientServiceDataToSend(const LtpClientServiceDataToSend& o) = delete; //disallow copying
    HDTN_UTIL_EXPORT LtpClientServiceDataToSend(LtpClientServiceDataToSend&& o); //a move constructor: X(X&&)
    HDTN_UTIL_EXPORT LtpClientServiceDataToSend& operator=(const LtpClientServiceDataToSend& o) = delete; //disallow copying
    HDTN_UTIL_EXPORT LtpClientServiceDataToSend& operator=(LtpClientServiceDataToSend&& o); //a move assignment: operator=(X&&)
    HDTN_UTIL_EXPORT bool operator==(const std::vector<uint8_t> & vec) const; //operator ==
    HDTN_UTIL_EXPORT bool operator!=(const std::vector<uint8_t> & vec) const; //operator !=
    HDTN_UTIL_EXPORT uint8_t * data() const;
    HDTN_UTIL_EXPORT std::size_t size() const;
    HDTN_UTIL_EXPORT std::vector<uint8_t> & GetVecRef();
    HDTN_UTIL_EXPORT zmq::message_t & GetZmqRef();

public:
    std::vector<uint8_t> m_userData;

private:
    std::vector<uint8_t> m_vector;
#ifdef LTP_CLIENT_SERVICE_DATA_TO_SEND_SUPPORT_ZMQ
    zmq::message_t m_zmqMessage;
#endif
    uint8_t * m_ptrData;
    std::size_t m_size;
};

#endif // LTP_CLIENT_SERVICE_DATA_TO_SEND_H


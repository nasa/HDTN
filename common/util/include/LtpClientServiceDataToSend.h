/**
 * @file LtpClientServiceDataToSend.h
 * @author  Brian Tomko <brian.j.tomko@nasa.gov>
 *
 * @copyright Copyright (c) 2021 United States Government as represented by
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
#include "PaddedVectorUint8.h"
#include <boost/asio.hpp>
#include "MemoryInFiles.h"

#ifdef LTP_CLIENT_SERVICE_DATA_TO_SEND_SUPPORT_ZMQ
#include <zmq.hpp>
#endif
#include "hdtn_util_export.h"
#include <boost/core/noncopyable.hpp>

/**
 * Data States (union-like state):
 * 1. Byte buffer -> m_vector is active.
 * 2. ZMQ message -> m_zmqMessage is active.
 */
class LtpClientServiceDataToSend : private boost::noncopyable {
public:
    /**
     * Initialize an empty packet buffer
     * @post Active data state: Byte buffer
     */
    HDTN_UTIL_EXPORT LtpClientServiceDataToSend(); //a default constructor: X()
    
    /**
     * Initialize the packet buffer to the given byte buffer.
     * @param vec The byte buffer packet to set.
     * @post Active data state: Byte buffer.
     * @post The argument to vec is left in a moved-from state.
     */
    HDTN_UTIL_EXPORT LtpClientServiceDataToSend(padded_vector_uint8_t&& vec);
    
    /**
     * If previously holding a ZMQ message, clean up its resources.
     * Initialize the packet buffer to the given byte buffer.
     * @param vec The byte buffer packet to set.
     * @return *this.
     * @post Active data state: Byte buffer.
     * @post The argument to vec is left in a moved-from state.
     */
    HDTN_UTIL_EXPORT LtpClientServiceDataToSend& operator=(padded_vector_uint8_t&& vec); //a move assignment: operator=(X&&)
#ifdef LTP_CLIENT_SERVICE_DATA_TO_SEND_SUPPORT_ZMQ
    /**
     * Initialize the packet buffer to the given ZMQ message.
     * @param zmqMessage The ZMQ message to set.
     * @post Active data state: ZMQ message.
     */
    HDTN_UTIL_EXPORT LtpClientServiceDataToSend(zmq::message_t && zmqMessage);
    
    /**
     * Initialize the packet buffer to the given ZMQ message.
     * @param zmqMessage The ZMQ message to set.
     * @return *this.
     * @post Active data state: ZMQ message.
     * @post The argument to zmqMessage is left in a moved-from state.
     */
    HDTN_UTIL_EXPORT LtpClientServiceDataToSend& operator=(zmq::message_t && zmqMessage); //a move assignment: operator=(X&&)
#endif
    /// Default destructor
    HDTN_UTIL_EXPORT ~LtpClientServiceDataToSend(); //a destructor: ~X()
    
    /// Deleted copy constructor
    HDTN_UTIL_EXPORT LtpClientServiceDataToSend(const LtpClientServiceDataToSend& o) = delete; //disallow copying
    
    /// Default move constructor
    HDTN_UTIL_EXPORT LtpClientServiceDataToSend(LtpClientServiceDataToSend&& o) noexcept; //a move constructor: X(X&&)
    
    /// Deleted copy assignment operator
    HDTN_UTIL_EXPORT LtpClientServiceDataToSend& operator=(const LtpClientServiceDataToSend& o) = delete; //disallow copying
    
    /// Default move assignment operator
    HDTN_UTIL_EXPORT LtpClientServiceDataToSend& operator=(LtpClientServiceDataToSend&& o) noexcept; //a move assignment: operator=(X&&)
    
    /**
     * @param vec The byte buffer to compare.
     * @return Stored byte buffer == vec.
     */
    HDTN_UTIL_EXPORT bool operator==(const padded_vector_uint8_t& vec) const; //operator ==
    
    /**
     * @param vec The byte buffer to compare.
     * @return Stored byte buffer != vec.
     */
    HDTN_UTIL_EXPORT bool operator!=(const padded_vector_uint8_t& vec) const; //operator !=
    
    /** Get the begin pointer of the primary data buffer.
     *
     * Valid regardless of active data state.
     * @return The begin pointer of the primary data buffer.
     */
    HDTN_UTIL_EXPORT uint8_t * data() const;
    
    /** Get the size the primary data buffer.
     *
     * Valid regardless of active data state.
     * @return The size of the primary data buffer.
     */
    HDTN_UTIL_EXPORT std::size_t size() const;
    
    /** Clean up internal data buffers.
     *
     * Valid regardless of active data state.
     * @param setSizeValueToZero Whether to reset the tracked size of the primary data buffer.
     * @post The primary data buffer begin pointer is set to NULL.
     */
    HDTN_UTIL_EXPORT void clear(bool setSizeValueToZero);
    
    /** Get the stored byte buffer.
     *
     * Reference to a default-constructed object when not the active data state.
     * @return The stored byte buffer.
     */
    HDTN_UTIL_EXPORT padded_vector_uint8_t& GetVecRef();
    
    /** Get the stored ZMQ message.
     *
     * Reference to a default-constructed object when not the active data state.
     * @return The stored ZMQ message.
     */
    HDTN_UTIL_EXPORT zmq::message_t & GetZmqRef();

public:
    /// Attached user data
    std::vector<uint8_t> m_userData;

private:
    /// Stored byte buffer
    padded_vector_uint8_t m_vector;
#ifdef LTP_CLIENT_SERVICE_DATA_TO_SEND_SUPPORT_ZMQ
    /// Stored ZMQ message
    zmq::message_t m_zmqMessage;
#endif
    /// Primary data buffer begin pointer
    uint8_t * m_ptrData;
    /// Primary data buffer size
    std::size_t m_size;
};

/// UDP send operation context data
struct UdpSendPacketInfo : private boost::noncopyable {
    /// Data buffers to send, references data from underlyingDataToDeleteOnSentCallback
    std::vector<boost::asio::const_buffer> constBufferVec;
    /// Underlying data buffers shared pointer, feeds constBufferVec
    std::shared_ptr<std::vector<std::vector<uint8_t> > > underlyingDataToDeleteOnSentCallback;
    /// Underlying client service data to send shared pointer, holds a copy of the in-memory client service data to send shared pointer
    /// when reading data from memory, if reading data from disk should be nullptr
    std::shared_ptr<LtpClientServiceDataToSend> underlyingCsDataToDeleteOnSentCallback;
    /// Deferred disk read, for when reading data to send from disk, in which case memoryBlockId MUST be non-zero
    MemoryInFiles::deferred_read_t deferredRead;
    /// Session originator engine ID
    uint64_t sessionOriginatorEngineId;

    /// Default constructor
    HDTN_UTIL_EXPORT UdpSendPacketInfo() = default;
    
    /// Move constructor
    HDTN_UTIL_EXPORT UdpSendPacketInfo(UdpSendPacketInfo&& o) noexcept; //a move constructor: X(X&&)
    
    /// Move assignment operator
    HDTN_UTIL_EXPORT UdpSendPacketInfo& operator=(UdpSendPacketInfo&& o) noexcept; //a move assignment: operator=(X&&)
    
    /** Perform reset.
     *
     * Clears buffers, resets shared pointers, resets deferred read object.
     */
    HDTN_UTIL_EXPORT void Reset();
};

#endif // LTP_CLIENT_SERVICE_DATA_TO_SEND_H


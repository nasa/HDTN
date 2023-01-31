/**
 * @file CircularIndexBufferSingleProducerSingleConsumerConfigurable.h
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
 * This CircularIndexBufferSingleProducerSingleConsumerConfigurable class is used for:
 * 1.) one producer thread to obtain an array index into a circular buffer, modify the data,
 *     and commit the write (modifying only the end index).
 * 2.) one consumer thread to obtain an array index into a circular buffer, read the data,
 *     and commit the read (modifying only the begin index).
 * This class is only concerned with sharing the two array indices between the two threads.
 * Therefore this class requires the user to provide the array(s) of user defined data.
 * This class assumes that "unsigned int" read and write operations for an index are atomic.
 */

#ifndef _CIRCULAR_INDEX_BUFFER_SINGLE_PRODUCER_SINGLE_CONSUMER_CONFIGURABLE_H
#define _CIRCULAR_INDEX_BUFFER_SINGLE_PRODUCER_SINGLE_CONSUMER_CONFIGURABLE_H 1

#include <boost/integer.hpp>
#include <stdint.h>
#include "hdtn_util_export.h"

/// When equal to write index, the buffer is full.
#define CIRCULAR_INDEX_BUFFER_FULL UINT32_MAX
/// When equal to read index, the buffer is empty
#define CIRCULAR_INDEX_BUFFER_EMPTY UINT32_MAX

class HDTN_UTIL_EXPORT CircularIndexBufferSingleProducerSingleConsumerConfigurable {
private:
    CircularIndexBufferSingleProducerSingleConsumerConfigurable();
public:
    /**
     * Set the working size of the external buffer, then initialize begin and end to zero.
     * @param size The working size of the associated external buffer.
     */
    CircularIndexBufferSingleProducerSingleConsumerConfigurable(unsigned int size) noexcept;
    
    /// Default destructor
    ~CircularIndexBufferSingleProducerSingleConsumerConfigurable() noexcept;
	
    /** Reset bounds.
     *
     * Set begin and end back to zero.
     */
    void Init() noexcept;
    
    /** Query whether external buffer is full.
     *
     * Checks if the next element after end (wrap on overflow) is equal to begin.
     * @return True if the external buffer is full, or False otherwise.
     */
    bool IsFull() const noexcept;
    
    /** Query whether external buffer is empty.
     *
     * Checks if begin is equal to end.
     * @return True if the external buffer is empty, or False otherwise.
     */
    bool IsEmpty() const noexcept;
    
    /** Get write index.
     *
     * Indicates the start of a write operation.
     * @return CIRCULAR_INDEX_BUFFER_FULL if buffer is full, else the write index.
     */
    unsigned int GetIndexForWrite() const noexcept;
    
    /** Advance write index.
     *
     * Indicates the completion of the current active write operation, advances end one element forward (wrap on overflow).
     */
    void CommitWrite() noexcept;
    
    /** Get read index.
     *
     * Indicates the start of a read operation.
     * @return CIRCULAR_INDEX_BUFFER_EMPTY if buffer is empty, else the read index.
     */
    unsigned int GetIndexForRead() const noexcept;
    
    /** Advance read index.
     *
     * Indicates the completion of the current active read operation, advances begin one element forward (wrap on overflow).
     */
    void CommitRead() noexcept;
    
    /** Get the number of active elements in the external buffer.
     *
     * Calculates how many elements exist between begin and end.
     * @return The number of elements in the external buffer that are currently being indexed.
     */
    unsigned int NumInBuffer() const noexcept;

private:
    /// Begin
    volatile unsigned int m_cbStartIndex;
    /// End
    volatile unsigned int m_cbEndIndex;
    /// Working size of the external buffer
    const unsigned int M_CIRCULAR_INDEX_BUFFER_SIZE;
};


#endif //_CIRCULAR_INDEX_BUFFER_SINGLE_PRODUCER_SINGLE_CONSUMER_CONFIGURABLE_H

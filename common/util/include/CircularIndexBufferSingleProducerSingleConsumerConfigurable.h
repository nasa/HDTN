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

#define CIRCULAR_INDEX_BUFFER_FULL UINT32_MAX
#define CIRCULAR_INDEX_BUFFER_EMPTY UINT32_MAX

class HDTN_UTIL_EXPORT CircularIndexBufferSingleProducerSingleConsumerConfigurable {
private:
    CircularIndexBufferSingleProducerSingleConsumerConfigurable();
public:
    CircularIndexBufferSingleProducerSingleConsumerConfigurable(unsigned int size);
    ~CircularIndexBufferSingleProducerSingleConsumerConfigurable();
	
    void Init();
    bool IsFull();
    bool IsEmpty();
    unsigned int GetIndexForWrite();
    void CommitWrite();
    unsigned int GetIndexForRead();
    void CommitRead();
    unsigned int NumInBuffer();

private:
    volatile unsigned int m_cbStartIndex;
    volatile unsigned int m_cbEndIndex;
    const unsigned int M_CIRCULAR_INDEX_BUFFER_SIZE;
	
};


#endif //_CIRCULAR_INDEX_BUFFER_SINGLE_PRODUCER_SINGLE_CONSUMER_CONFIGURABLE_H

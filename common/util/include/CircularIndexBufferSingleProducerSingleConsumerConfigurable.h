#ifndef _CIRCULAR_INDEX_BUFFER_SINGLE_PRODUCER_SINGLE_CONSUMER_CONFIGURABLE_H
#define _CIRCULAR_INDEX_BUFFER_SINGLE_PRODUCER_SINGLE_CONSUMER_CONFIGURABLE_H

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

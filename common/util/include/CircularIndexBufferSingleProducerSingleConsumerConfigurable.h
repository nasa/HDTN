#ifndef _CIRCULAR_INDEX_BUFFER_SINGLE_PRODUCER_SINGLE_CONSUMER_CONFIGURABLE_H
#define _CIRCULAR_INDEX_BUFFER_SINGLE_PRODUCER_SINGLE_CONSUMER_CONFIGURABLE_H

#include <boost/integer.hpp>
#include <stdint.h>




class CircularIndexBufferSingleProducerSingleConsumerConfigurable {
private:
    CircularIndexBufferSingleProducerSingleConsumerConfigurable();
public:
        CircularIndexBufferSingleProducerSingleConsumerConfigurable(unsigned int size);
        ~CircularIndexBufferSingleProducerSingleConsumerConfigurable();
	
	bool IsFull();
	bool IsEmpty();
	unsigned int GetIndexForWrite();
	void CommitWrite();
	unsigned int GetIndexForRead();
	void CommitRead();

private:
	volatile unsigned int m_cbStartIndex;
	volatile unsigned int m_cbEndIndex;
        const unsigned int M_CIRCULAR_INDEX_BUFFER_SIZE;
	
};


#endif //_CIRCULAR_INDEX_BUFFER_SINGLE_PRODUCER_SINGLE_CONSUMER_CONFIGURABLE_H

#ifndef _CIRCULAR_INDEX_BUFFER_SINGLE_PRODUCER_SINGLE_CONSUMER_H
#define _CIRCULAR_INDEX_BUFFER_SINGLE_PRODUCER_SINGLE_CONSUMER_H

#include <boost/integer.hpp>
#include <stdint.h>


#define CIRCULAR_INDEX_BUFFER_SIZE 30

class CircularIndexBufferSingleProducerSingleConsumer {
public:
	CircularIndexBufferSingleProducerSingleConsumer();
	~CircularIndexBufferSingleProducerSingleConsumer();
	
	bool IsFull();
	bool IsEmpty();
	unsigned int GetIndexForWrite();
	void CommitWrite();
	unsigned int GetIndexForRead();
	void CommitRead();

private:
	volatile unsigned int m_cbStartIndex;
	volatile unsigned int m_cbEndIndex;
	
};


#endif //_CIRCULAR_INDEX_BUFFER_SINGLE_PRODUCER_SINGLE_CONSUMER_H

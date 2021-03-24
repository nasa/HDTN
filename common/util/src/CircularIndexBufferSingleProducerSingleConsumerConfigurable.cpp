#include "CircularIndexBufferSingleProducerSingleConsumerConfigurable.h"
#include <stdint.h>


CircularIndexBufferSingleProducerSingleConsumerConfigurable::CircularIndexBufferSingleProducerSingleConsumerConfigurable(unsigned int size) :
    m_cbStartIndex(0), m_cbEndIndex(0), M_CIRCULAR_INDEX_BUFFER_SIZE(size)
{
	
}

CircularIndexBufferSingleProducerSingleConsumerConfigurable::~CircularIndexBufferSingleProducerSingleConsumerConfigurable() {
	
}

void CircularIndexBufferSingleProducerSingleConsumerConfigurable::Init() {
    m_cbStartIndex = 0;
    m_cbEndIndex = 0;
}


bool CircularIndexBufferSingleProducerSingleConsumerConfigurable::IsFull() {
    //return ((m_end + 1) % m_bufferSize) == m_start;
    unsigned int endPlus1 = m_cbEndIndex + 1;
        if (endPlus1 >= M_CIRCULAR_INDEX_BUFFER_SIZE) endPlus1 = 0;
    return (m_cbStartIndex == endPlus1);
}

bool CircularIndexBufferSingleProducerSingleConsumerConfigurable::IsEmpty() {
	return (m_cbEndIndex == m_cbStartIndex);
}

unsigned int CircularIndexBufferSingleProducerSingleConsumerConfigurable::GetIndexForWrite() {
	if (IsFull())
		return UINT32_MAX;
	return m_cbEndIndex;
}

void CircularIndexBufferSingleProducerSingleConsumerConfigurable::CommitWrite() {
	////if(CircularBufferIsFull())
	////	return;
	//m_end = (m_end + 1) % m_bufferSize;
	unsigned int endPlus1 = m_cbEndIndex + 1;
        if (endPlus1 >= M_CIRCULAR_INDEX_BUFFER_SIZE) endPlus1 = 0;
	m_cbEndIndex = endPlus1;
}

unsigned int CircularIndexBufferSingleProducerSingleConsumerConfigurable::GetIndexForRead() {
	if (IsEmpty())
		return UINT32_MAX;
	return m_cbStartIndex;
}

void CircularIndexBufferSingleProducerSingleConsumerConfigurable::CommitRead() {
	////if(CircularBufferIsEmpty())
	////	return;
	//m_start = (m_start + 1) % m_bufferSize;
	unsigned int startPlus1 = m_cbStartIndex + 1;
        if (startPlus1 >= M_CIRCULAR_INDEX_BUFFER_SIZE) startPlus1 = 0;
	m_cbStartIndex = startPlus1;
	
}

unsigned int CircularIndexBufferSingleProducerSingleConsumerConfigurable::NumInBuffer() {
    unsigned int endIndex = m_cbEndIndex;
    const unsigned int startIndex = m_cbStartIndex;
    if (endIndex < startIndex) {
        endIndex += M_CIRCULAR_INDEX_BUFFER_SIZE;
    }
    return endIndex - startIndex;
}

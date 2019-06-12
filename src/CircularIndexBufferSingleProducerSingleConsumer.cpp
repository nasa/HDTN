#include "CircularIndexBufferSingleProducerSingleConsumer.h"
#include <stdint.h>


CircularIndexBufferSingleProducerSingleConsumer::CircularIndexBufferSingleProducerSingleConsumer() : m_cbStartIndex(0), m_cbEndIndex(0) {
	
}
CircularIndexBufferSingleProducerSingleConsumer::~CircularIndexBufferSingleProducerSingleConsumer() {
	
}

bool CircularIndexBufferSingleProducerSingleConsumer::IsFull() {
	//return ((m_end + 1) % m_bufferSize) == m_start;
	unsigned int endPlus1 = m_cbEndIndex + 1;
	if (endPlus1 >= CIRCULAR_INDEX_BUFFER_SIZE) endPlus1 = 0;
	return (m_cbStartIndex == endPlus1);
}

bool CircularIndexBufferSingleProducerSingleConsumer::IsEmpty() {
	return (m_cbEndIndex == m_cbStartIndex);
}

unsigned int CircularIndexBufferSingleProducerSingleConsumer::GetIndexForWrite() {
	if (IsFull())
		return UINT32_MAX;
	return m_cbEndIndex;
}

void CircularIndexBufferSingleProducerSingleConsumer::CommitWrite() {
	////if(CircularBufferIsFull())
	////	return;
	//m_end = (m_end + 1) % m_bufferSize;
	unsigned int endPlus1 = m_cbEndIndex + 1;
	if (endPlus1 >= CIRCULAR_INDEX_BUFFER_SIZE) endPlus1 = 0;
	m_cbEndIndex = endPlus1;
}

unsigned int CircularIndexBufferSingleProducerSingleConsumer::GetIndexForRead() {
	if (IsEmpty())
		return UINT32_MAX;
	return m_cbStartIndex;
}

void CircularIndexBufferSingleProducerSingleConsumer::CommitRead() {
	////if(CircularBufferIsEmpty())
	////	return;
	//m_start = (m_start + 1) % m_bufferSize;
	unsigned int startPlus1 = m_cbStartIndex + 1;
	if (startPlus1 >= CIRCULAR_INDEX_BUFFER_SIZE) startPlus1 = 0;
	m_cbStartIndex = startPlus1;
	
}
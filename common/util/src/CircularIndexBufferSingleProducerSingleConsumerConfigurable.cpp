/**
 * @file CircularIndexBufferSingleProducerSingleConsumerConfigurable.cpp
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


bool CircularIndexBufferSingleProducerSingleConsumerConfigurable::IsFull() const {
    //return ((m_end + 1) % m_bufferSize) == m_start;
    unsigned int endPlus1 = m_cbEndIndex + 1;
        if (endPlus1 >= M_CIRCULAR_INDEX_BUFFER_SIZE) endPlus1 = 0;
    return (m_cbStartIndex == endPlus1);
}

bool CircularIndexBufferSingleProducerSingleConsumerConfigurable::IsEmpty() const {
	return (m_cbEndIndex == m_cbStartIndex);
}

unsigned int CircularIndexBufferSingleProducerSingleConsumerConfigurable::GetIndexForWrite() const {
	if (IsFull())
		return CIRCULAR_INDEX_BUFFER_FULL;
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

unsigned int CircularIndexBufferSingleProducerSingleConsumerConfigurable::GetIndexForRead() const {
	if (IsEmpty())
		return CIRCULAR_INDEX_BUFFER_EMPTY;
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

unsigned int CircularIndexBufferSingleProducerSingleConsumerConfigurable::NumInBuffer() const {
    unsigned int endIndex = m_cbEndIndex;
    const unsigned int startIndex = m_cbStartIndex;
    if (endIndex < startIndex) {
        endIndex += M_CIRCULAR_INDEX_BUFFER_SIZE;
    }
    return endIndex - startIndex;
}

/**
 * @file CircularIndexBufferSingleProducerSingleConsumerConfigurable.cpp
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
 */

#include "CircularIndexBufferSingleProducerSingleConsumerConfigurable.h"
#include <stdint.h>


CircularIndexBufferSingleProducerSingleConsumerConfigurable::CircularIndexBufferSingleProducerSingleConsumerConfigurable(unsigned int size) noexcept :
    m_cbStartIndex(0), m_cbEndIndex(0), M_CIRCULAR_INDEX_BUFFER_SIZE(size)
{
    
}

CircularIndexBufferSingleProducerSingleConsumerConfigurable::~CircularIndexBufferSingleProducerSingleConsumerConfigurable() noexcept {
    
}

void CircularIndexBufferSingleProducerSingleConsumerConfigurable::Init() noexcept {
    m_cbStartIndex = 0;
    m_cbEndIndex = 0;
}


bool CircularIndexBufferSingleProducerSingleConsumerConfigurable::IsFull() const noexcept {
    //return ((m_end + 1) % m_bufferSize) == m_start;
    unsigned int endPlus1 = m_cbEndIndex + 1;
    if (endPlus1 >= M_CIRCULAR_INDEX_BUFFER_SIZE) {
        endPlus1 = 0;
    }
    return (m_cbStartIndex == endPlus1);
}

bool CircularIndexBufferSingleProducerSingleConsumerConfigurable::IsEmpty() const noexcept {
    return (m_cbEndIndex == m_cbStartIndex);
}

unsigned int CircularIndexBufferSingleProducerSingleConsumerConfigurable::GetIndexForWrite() const noexcept {
    //if (IsFull()) return CIRCULAR_INDEX_BUFFER_FULL;
    const unsigned int end = m_cbEndIndex.load(std::memory_order_relaxed); //relaxed => writer thread owns m_cbEndIndex
    unsigned int endPlus1 = end + 1;
    if (endPlus1 >= M_CIRCULAR_INDEX_BUFFER_SIZE) {
        endPlus1 = 0;
    }
    const unsigned int start = m_cbStartIndex.load(std::memory_order_acquire); //acquire => other (reader) thread owns m_cbStartIndex
    if (start == endPlus1) { //is full 
        return CIRCULAR_INDEX_BUFFER_FULL;
    }

    return end;
}

void CircularIndexBufferSingleProducerSingleConsumerConfigurable::CommitWrite() noexcept {
    //if(CircularBufferIsFull()) return; //this check not implemented here in CommitWrite(), was done in GetIndexForWrite()
    
    //m_end = (m_end + 1) % m_bufferSize;
    unsigned int endPlus1 = m_cbEndIndex.load(std::memory_order_relaxed) + 1; //relaxed => writer thread owns m_cbEndIndex, within this thread the load still "happens before" the store later
    if (endPlus1 >= M_CIRCULAR_INDEX_BUFFER_SIZE) {
        endPlus1 = 0;
    }
    m_cbEndIndex.store(endPlus1, std::memory_order_release); //release => writer thread owns m_cbEndIndex and is informing other thread of the change
}

unsigned int CircularIndexBufferSingleProducerSingleConsumerConfigurable::GetIndexForRead() const noexcept {
    //if (IsEmpty()) return CIRCULAR_INDEX_BUFFER_EMPTY;
    const unsigned int start = m_cbStartIndex.load(std::memory_order_relaxed); //relaxed => reader thread owns m_cbStartIndex
    const unsigned int end = m_cbEndIndex.load(std::memory_order_acquire); //acquire => other (writer) thread owns m_cbEndIndex
    if (end == start) { //is empty
        return CIRCULAR_INDEX_BUFFER_EMPTY;
    }

    return start;
}

void CircularIndexBufferSingleProducerSingleConsumerConfigurable::CommitRead() noexcept {
    //if(CircularBufferIsEmpty()) return; //this check not implemented here in CommitRead(), was done in GetIndexForRead()

    //m_start = (m_start + 1) % m_bufferSize;
    unsigned int startPlus1 = m_cbStartIndex.load(std::memory_order_relaxed) + 1; //relaxed => reader thread owns m_cbStartIndex, within this thread the load still "happens before" the store later
    if (startPlus1 >= M_CIRCULAR_INDEX_BUFFER_SIZE) {
        startPlus1 = 0;
    }
    m_cbStartIndex.store(startPlus1, std::memory_order_release); //release => reader thread owns m_cbStartIndex and is informing other thread of the change
}

unsigned int CircularIndexBufferSingleProducerSingleConsumerConfigurable::NumInBuffer() const noexcept {
    unsigned int endIndex = m_cbEndIndex;
    const unsigned int startIndex = m_cbStartIndex;
    if (endIndex < startIndex) {
        endIndex += M_CIRCULAR_INDEX_BUFFER_SIZE;
    }
    return endIndex - startIndex;
}

unsigned int CircularIndexBufferSingleProducerSingleConsumerConfigurable::GetCapacity() const noexcept {
    return M_CIRCULAR_INDEX_BUFFER_SIZE;
}

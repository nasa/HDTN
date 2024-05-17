/**
 * @file DtnFrameQueue.cpp
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

#include "DtnFrameQueue.h"
#include "Logger.h"

static constexpr hdtn::Logger::SubProcess subprocess = hdtn::Logger::SubProcess::none;

DtnFrameQueue::DtnFrameQueue(size_t queueSize) : m_queueSize(queueSize), m_totalBytesInQueue(0)
{
    LOG_INFO(subprocess) << "Created queue of size" << m_queueSize;
}

DtnFrameQueue::~DtnFrameQueue()
{
}

// get reference to first element in frame queue
rtp_frame& DtnFrameQueue::GetNextFrame() 
{
    return m_frameQueue.front();
}

// pops oldest frame in queue
void DtnFrameQueue::PopFrame()
{
    // std::cout << "POP" <<std::endl;
    if (m_frameQueue.size() > 0) {
        m_totalBytesInQueue -= m_frameQueue.front().payload.length; 
        m_totalBytesInQueue -= sizeof(rtp_header);
        m_frameQueue.pop();
    }
}

// adds new frame to end of queue
void DtnFrameQueue::PushFrame(buffer * img_buffer, rtp_frame * frame) 
{
    if (m_frameQueue.size() >= m_queueSize)
        PopFrame();

    m_frameQueue.push(*frame); // adds frame to back of queue

    m_frameQueue.back().payload.allocate(img_buffer->length); // allocate memory in frame that is at back of queue

    memcpy(m_frameQueue.back().payload.start , img_buffer->start, img_buffer->length); // copy once into the frame that is in back of queue

    m_totalBytesInQueue += m_frameQueue.back().payload.length;
    m_totalBytesInQueue += sizeof(rtp_header);
    // LOG_INFO(subprocess) << "total bytes in queue" << m_totalBytesInQueue;

}   

// for pushing frames that already have all the payload filled (usually an incoming frame)
void DtnFrameQueue::PushFrame(const rtp_frame& frame) 
{

    if (m_frameQueue.size() >= m_queueSize)
        PopFrame();

    m_frameQueue.push(frame);

    m_frameQueue.front().payload.allocate(frame.payload.length);

    memcpy(m_frameQueue.front().payload.start , frame.payload.start, frame.payload.length); // copy once

    m_totalBytesInQueue += m_frameQueue.front().payload.length;
    m_totalBytesInQueue += sizeof(rtp_header);
    LOG_INFO(subprocess) << "pushed frame into queue";

}   


size_t DtnFrameQueue::GetCurrentQueueSize()
{
    return m_frameQueue.size();
}

size_t DtnFrameQueue::GetCurrentQueueSizeBytes()
{
    return m_totalBytesInQueue;
}

std::queue<rtp_frame>& DtnFrameQueue::GetQueue() 
{
    return m_frameQueue;
}

void DtnFrameQueue::ClearQueue()
{
    m_frameQueue = std::queue<rtp_frame>();
}

bool DtnFrameQueue::GetNextQueueReady()
{
    if (m_frameQueue.size() != m_queueSize)
        return false;
    
    return true; // if queue has filled to proper size
}

bool DtnFrameQueue::GetNextQueueTimeout(const boost::posix_time::time_duration& timeout)
{
    boost::mutex::scoped_lock lock(m_queueMutex);
    if (!GetNextQueueReady()) {
        m_queueCv.timed_wait(lock, timeout); //lock mutex (above) before checking condition
    }

    return GetNextQueueReady();
}

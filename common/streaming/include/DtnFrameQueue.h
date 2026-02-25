/**
 * @file DtnFrameQueue.h
 *
 * @copyright Copyright (c) 2026 United States Government as represented by
 * the National Aeronautics and Space Administration.
 * No copyright is claimed in the United States under Title 17, U.S.Code.
 * All Other Rights Reserved.
 *
 * @section LICENSE
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once
#include <queue>

#include "DtnRtpFrame.h"
#include <boost/thread.hpp>
#include "streaming_lib_export.h"

class DtnFrameQueue
{
private:
    std::queue<rtp_frame> m_frameQueue;
    size_t m_queueSize; // number of rtp packets in queue
    size_t m_totalBytesInQueue; // raw bytes in queue

    boost::mutex m_queueMutex;
    boost::condition_variable m_queueCv;
public:
    DtnFrameQueue() = delete;
    STREAMING_LIB_EXPORT DtnFrameQueue(size_t queueSize);
    STREAMING_LIB_EXPORT ~DtnFrameQueue();

    STREAMING_LIB_EXPORT rtp_frame& GetNextFrame();
    STREAMING_LIB_EXPORT void PopFrame();
    STREAMING_LIB_EXPORT void PushFrame(buffer * image_buffer, rtp_frame * frame); // for outgoing frames
    STREAMING_LIB_EXPORT void PushFrame(const rtp_frame& frame);
    STREAMING_LIB_EXPORT void ClearQueue();

    STREAMING_LIB_EXPORT size_t GetCurrentQueueSize(); // number of rtp packets 
    STREAMING_LIB_EXPORT size_t GetCurrentQueueSizeBytes(); // number of raw bytes across all packets in queue
    STREAMING_LIB_EXPORT std::queue<rtp_frame>& GetQueue(); // reference to our queue, for copying out

    // for monitoring if queue is full
    STREAMING_LIB_EXPORT bool GetNextQueueReady();
    STREAMING_LIB_EXPORT bool GetNextQueueTimeout(const boost::posix_time::time_duration& timeout);
};



#pragma once
#include <queue>

#include "DtnRtpFrame.h"
#include <boost/thread.hpp>

class DtnFrameQueue
{
private:
    std::queue<rtp_frame> m_frameQueue;
    size_t m_queueSize = 0; // number of rtp packets in queue
    size_t m_totalBytesInQueue = 0; // raw bytes in queue

    boost::mutex m_queueMutex;
    boost::condition_variable m_queueCv;
public:
    DtnFrameQueue(size_t queueSize);
    ~DtnFrameQueue();

    rtp_frame& GetNextFrame();
    void PopFrame();
    void PushFrame(buffer * image_buffer, rtp_frame * frame); // for outgoing frames
    void PushFrame(const rtp_frame& frame);
    int PullFrame(const uint8_t * data, const uint64_t size);  // for incoming frames from sink
    void ClearQueue();

    size_t GetCurrentQueueSize(); // number of rtp packets 
    size_t GetCurrentQueueSizeBytes(); // number of raw bytes across all packets in queue
    std::queue<rtp_frame>& GetQueue(); // reference to our queue, for copying out

    // for monitoring if queue is full
    bool GetNextQueueReady();
    bool GetNextQueueTimeout(const boost::posix_time::time_duration& timeout);
};



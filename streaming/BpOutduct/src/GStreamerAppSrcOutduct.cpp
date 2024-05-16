#include "GStreamerAppSrcOutduct.h"

#include "Logger.h"
#include "ThreadNamer.h"

static constexpr hdtn::Logger::SubProcess subprocess = hdtn::Logger::SubProcess::none;
static GstClockTime duration    = 33333333; // .0.016666667 seconds aka 60fps
static GStreamerAppSrcOutduct * s_gStreamerAppSrcOutduct = NULL;

void GStreamerAppSrcOutduct::SetGStreamerAppSrcOutductInstance(GStreamerAppSrcOutduct * gStreamerAppSrcOutduct)
{
    s_gStreamerAppSrcOutduct = gStreamerAppSrcOutduct;
}


GStreamerAppSrcOutduct::GStreamerAppSrcOutduct(std::string shmSocketPath, std::string gstCaps) : 
    m_shmSocketPath(shmSocketPath),
    m_gstCaps(gstCaps),
    m_running(true),
    m_runDisplayThread(true),
    m_runFilesinkThread(true),
    m_bus(NULL),

    m_pipeline(NULL),
    m_displayAppsrc(NULL),
    /* cap goes here*/
    m_displayQueue(NULL),
    m_rtpjitterbuffer(NULL),
    m_rtph264depay(NULL),
    m_h264parse(NULL),
    m_h264timestamper(NULL),
    m_decodeQueue(NULL),
    m_avdec_h264(NULL),
    m_postDecodeQueue(NULL),
    m_displayShmsink(NULL),

    // To filesink 
    m_filesinkAppsrc(NULL),
    m_filesinkQueue(NULL),
    m_filesinkShmsink(NULL),

    // stat keeping 
    m_totalIncomingCbOverruns(0),
    m_totalFilesinkCbOverruns(0),
    m_totalDisplayCbOverruns(0)
{
    m_incomingRtpPacketQueue.set_capacity(DEFAULT_NUM_CIRC_BUFFERS);
    m_incomingRtpPacketQueueForDisplay.set_capacity(DEFAULT_NUM_CIRC_BUFFERS);
    m_incomingRtpPacketQueueForFilesink.set_capacity(DEFAULT_NUM_CIRC_BUFFERS);


    boost::posix_time::time_duration timeout(boost::posix_time::milliseconds(250));
    m_bundleCallbackAsyncListenerPtr       = boost::make_unique<AsyncListener<CbQueue_t>>(m_incomingRtpPacketQueue, timeout);
    m_rtpPacketToDisplayAsyncListenerPtr   = boost::make_unique<AsyncListener<CbQueue_t>>(m_incomingRtpPacketQueueForDisplay, timeout);
    m_rtpPacketToFilesinkAsyncListenerPtr  = boost::make_unique<AsyncListener<CbQueue_t>>(m_incomingRtpPacketQueueForFilesink, timeout);

    LOG_INFO(subprocess) << "Creating GStreamer appsrc pipeline. ShmSocketPath=" << m_shmSocketPath;
    
    gst_init(NULL, NULL); // Initialize gstreamer first
    
    if ((CreateElements() == 0) && (BuildPipeline() == 0)) {
        m_busMonitoringThread = boost::make_unique<boost::thread>(boost::bind(&GStreamerAppSrcOutduct::OnBusMessages, this)); 

        StartPlaying();
        
        m_packetTeeThread = boost::make_unique<boost::thread>(boost::bind(&GStreamerAppSrcOutduct::TeeDataToQueuesThread, this)); 
        m_displayThread =  boost::make_unique<boost::thread>(boost::bind(&GStreamerAppSrcOutduct::PushDataToDisplayThread, this)); 
        m_filesinkThread =  boost::make_unique<boost::thread>(boost::bind(&GStreamerAppSrcOutduct::PushDataToFilesinkThread, this)); 
    } else {
        LOG_ERROR(subprocess) << "Could not initialize GStreamerAppSrcOutduct. Aborting";
    }
}

GStreamerAppSrcOutduct::~GStreamerAppSrcOutduct()
{
    LOG_INFO(subprocess) << "Calling GStreamerAppSrcOutduct deconstructor";
    LOG_INFO(subprocess) << "GStreamerAppSrcOutduct::m_totalIncomingCbOverruns: " << m_totalIncomingCbOverruns;
    LOG_INFO(subprocess) << "GStreamerAppSrcOutduct::m_totalDisplayCbOverruns: " << m_totalDisplayCbOverruns;
    LOG_INFO(subprocess) << "GStreamerAppSrcOutduct::m_totalFilesinkCbOverruns: " << m_totalFilesinkCbOverruns;
    LOG_INFO(subprocess) << "GStreamerAppSrcOutduct::m_numDisplaySamples " << m_numDisplaySamples;
    LOG_INFO(subprocess) << "GStreamerAppSrcOutduct::m_numFilesinkSamples " << m_numFilesinkSamples;

    
    m_running = false;
    m_runDisplayThread = false;
    m_runFilesinkThread = false; 

    m_busMonitoringThread->join();
    m_packetTeeThread->join();
    m_filesinkThread->join();
    m_displayThread->join();

    gst_element_set_state(m_pipeline, GST_STATE_NULL);
}

int GStreamerAppSrcOutduct::CreateElements()
{
    /* Display */
    m_displayAppsrc = gst_element_factory_make("appsrc", NULL);
    m_displayQueue = gst_element_factory_make("queue", NULL);
    m_rtpjitterbuffer = gst_element_factory_make("rtpjitterbuffer", NULL);
    m_rtph264depay = gst_element_factory_make("rtph264depay", NULL);
    m_h264parse = gst_element_factory_make("h264parse", NULL);
    m_h264timestamper = gst_element_factory_make("h264timestamper", NULL);
    m_decodeQueue = gst_element_factory_make("queue", NULL);
    m_avdec_h264 = gst_element_factory_make("avdec_h264", NULL);
    m_postDecodeQueue = gst_element_factory_make("queue", NULL);
    m_displayShmsink = gst_element_factory_make("shmsink", NULL);
   
    /* Filesink */
    m_filesinkAppsrc = gst_element_factory_make("appsrc", NULL);
    m_filesinkQueue = gst_element_factory_make("queue", NULL);
    m_filesinkShmsink = gst_element_factory_make("shmsink", NULL);

    m_pipeline   =  gst_pipeline_new(NULL);
    

    /* Configure queues */
    g_object_set(G_OBJECT(m_displayQueue), "max-size-buffers", MAX_NUM_BUFFERS_QUEUE, "max-size-bytes", MAX_SIZE_BYTES_QUEUE, "max-size-time", MAX_SIZE_TIME_QUEUE, "min-threshold-time", (uint64_t) 0,  NULL );
    g_object_set(G_OBJECT(m_decodeQueue), "max-size-buffers", 0, "max-size-bytes", 0, "max-size-time", 0, "min-threshold-time", (uint64_t) 0, "leaky", 0,  NULL );
    g_object_set(G_OBJECT(m_filesinkQueue), "max-size-buffers", 0, "max-size-bytes", MAX_SIZE_BYTES_QUEUE, "max-size-time", MAX_SIZE_TIME_QUEUE, "min-threshold-time", (uint64_t) 0, "leaky", 0, NULL );

    /* Configure shared memory sinks */
    g_object_set(G_OBJECT(m_displayShmsink), "socket-path", m_shmSocketPath.c_str(), "wait-for-connection", false, "sync", false, "async", false,  "processing-deadline", (uint64_t) 30e9,  "shm-size", 4294967295, NULL);
    g_object_set(G_OBJECT(m_filesinkShmsink), "max-bitrate", 20000000,  "socket-path", "/tmp/hdtn_gst_shm_outduct_filesink", "wait-for-connection", true, "sync", false, "async", false, "processing-deadline", (uint64_t) 30e9, "shm-size", 4294967295, NULL);

    /* Configure rtpjitterbuffer */
    g_object_set(G_OBJECT(m_rtpjitterbuffer), "latency", RTP_LATENCY_MILLISEC, "max-dropout-time", RTP_MAX_DROPOUT_TIME_MILLISEC,
        "max-misorder-time", RTP_MAX_MISORDER_TIME_MIILISEC, "mode", RTP_MODE, "drop-on-latency", true, NULL);
    
    /* Configure decoder */
    g_object_set(G_OBJECT(m_avdec_h264), "lowres", 0, "output-corrupt", false, "discard-corrupted-frames", true,  NULL);

    /* set caps on the src element */
    GstCaps * caps = gst_caps_from_string(m_gstCaps.c_str());
    g_object_set(G_OBJECT(m_displayAppsrc), "emit-signals", false, "min-latency", 0, "is-live", true, "do-timestamp", true, "max-bytes", GST_APPSRC_MAX_BYTES_IN_BUFFER, "caps", caps, "format", GST_FORMAT_TIME, "block", false, NULL);
    g_object_set(G_OBJECT(m_filesinkAppsrc),  "emit-signals", false, "min-latency", 0, "is-live", true, "do-timestamp", true, "max-bytes", GST_APPSRC_MAX_BYTES_IN_BUFFER, "caps", caps, "format", GST_FORMAT_TIME, "block", false, NULL);
    gst_caps_unref(caps);


    /* Register our bus to be notified of bus messages */
    m_bus = gst_element_get_bus(m_pipeline);

    return 0;
}


int GStreamerAppSrcOutduct::BuildPipeline()
{
    LOG_INFO(subprocess) << "Building Pipeline";
    
    gst_bin_add_many(GST_BIN(m_pipeline), m_displayAppsrc, m_displayQueue, m_rtpjitterbuffer, m_rtph264depay, m_h264parse, m_h264timestamper, m_decodeQueue, m_avdec_h264, m_postDecodeQueue, m_displayShmsink, \
        m_filesinkAppsrc, m_filesinkQueue, m_filesinkShmsink, NULL);

    if (!gst_element_link_many(m_displayAppsrc, m_displayQueue, m_rtpjitterbuffer, m_rtph264depay, m_h264parse, m_h264timestamper, m_decodeQueue, m_avdec_h264, m_postDecodeQueue, m_displayShmsink, NULL)) {
        LOG_ERROR(subprocess) << "Display pipeline could not be linked";
        return -1;
    }
        
    if (!gst_element_link_many(m_filesinkAppsrc, m_filesinkQueue, m_filesinkShmsink, NULL)) {
        LOG_ERROR(subprocess) << "Filesink pipeline could not be linked";
        return -1;
    }

    LOG_INFO(subprocess) << "Succesfully built pipeline";
    return 0;
}

int GStreamerAppSrcOutduct::StartPlaying()
{
    /* Start playing the pipeline */
    gst_element_set_state (m_pipeline, GST_STATE_PLAYING);
    LOG_INFO(subprocess) << "Receiving bin launched";
    GST_DEBUG_BIN_TO_DOT_FILE((GstBin *) m_pipeline, GST_DEBUG_GRAPH_SHOW_ALL, "gst_outduct");
    return 0;
}


void GStreamerAppSrcOutduct::TeeDataToQueuesThread()
{
    padded_vector_uint8_t incomingRtpFrame;
    incomingRtpFrame.reserve(1600);

    while (m_running.load(std::memory_order_acquire)) {
        bool notInWaitForNewBundlesState = m_bundleCallbackAsyncListenerPtr->TryWaitForIncomingDataAvailable();
        if (notInWaitForNewBundlesState) {
            /* Make local copy to allow bundle thread to continue asap */
            m_bundleCallbackAsyncListenerPtr->Lock();
                incomingRtpFrame = std::move(m_bundleCallbackAsyncListenerPtr->m_queue.front()); // we now have a local copy, can unlock from incoming bundle queue
                m_bundleCallbackAsyncListenerPtr->m_queue.pop_front();
            m_bundleCallbackAsyncListenerPtr->Unlock();
            m_bundleCallbackAsyncListenerPtr->Notify();

            /* hard copy the data to the filesink queue */
            padded_vector_uint8_t frameToPush(incomingRtpFrame.size());
            memcpy(frameToPush.data(), incomingRtpFrame.data(), incomingRtpFrame.size());
            m_rtpPacketToFilesinkAsyncListenerPtr->Lock();
                if (m_rtpPacketToFilesinkAsyncListenerPtr->m_queue.full())
                    m_totalFilesinkCbOverruns++;
                m_rtpPacketToFilesinkAsyncListenerPtr->m_queue.push_front(std::move(frameToPush));
            m_rtpPacketToFilesinkAsyncListenerPtr->Unlock();
            m_rtpPacketToFilesinkAsyncListenerPtr->Notify();


            /* zero copy the data to the display queue */
            m_rtpPacketToDisplayAsyncListenerPtr->Lock();
                if (m_rtpPacketToDisplayAsyncListenerPtr->m_queue.full())
                    m_totalDisplayCbOverruns++;
                m_rtpPacketToDisplayAsyncListenerPtr->m_queue.push_front(std::move(incomingRtpFrame));
            m_rtpPacketToDisplayAsyncListenerPtr->Unlock();
            m_rtpPacketToDisplayAsyncListenerPtr->Notify();
        }
    }

}

void GStreamerAppSrcOutduct::PushDataToFilesinkThread()
{
    static HdtnGstHandoffUtils_t hdtnGstHandoffUtils;
    static padded_vector_uint8_t incomingRtpFrame;
    incomingRtpFrame.reserve(1600);

    while (m_runFilesinkThread.load(std::memory_order_acquire)) {
        
        bool notInWaitForNewPacketsState = m_rtpPacketToFilesinkAsyncListenerPtr->TryWaitForIncomingDataAvailable();
        if (notInWaitForNewPacketsState) {
            m_rtpPacketToFilesinkAsyncListenerPtr->Lock();
                incomingRtpFrame = std::move(m_rtpPacketToFilesinkAsyncListenerPtr->m_queue.front());
                m_rtpPacketToFilesinkAsyncListenerPtr->PopFront();
            m_rtpPacketToFilesinkAsyncListenerPtr->Unlock();
            m_rtpPacketToFilesinkAsyncListenerPtr->Notify();

            hdtnGstHandoffUtils.buffer = gst_buffer_new_and_alloc(incomingRtpFrame.size());

            /* copy in from our local queue */
            gst_buffer_map(hdtnGstHandoffUtils.buffer, &hdtnGstHandoffUtils.map, GST_MAP_WRITE);
            memcpy(hdtnGstHandoffUtils.map.data, incomingRtpFrame.data(), incomingRtpFrame.size());
         
            /* Set its timestamp and duration */
            GST_BUFFER_PTS(hdtnGstHandoffUtils.buffer) = gst_util_uint64_scale(m_numFilesinkSamples, GST_SECOND, SAMPLE_RATE);
            GST_BUFFER_DURATION(hdtnGstHandoffUtils.buffer) = duration;
            
            gst_buffer_unmap(hdtnGstHandoffUtils.buffer, &hdtnGstHandoffUtils.map);

            /* Push the buffer into the appsrc */
            hdtnGstHandoffUtils.ret = gst_app_src_push_buffer((GstAppSrc *) m_filesinkAppsrc, hdtnGstHandoffUtils.buffer); // takes ownership of buffer we DO NOT deref

            m_numFilesinkSamples += 1;
        }

        if ((m_numFilesinkSamples % 150) == 0) {
            static guint buffersInFilesinkQueue;
            g_object_get(G_OBJECT(m_filesinkQueue), "current-level-buffers", &buffersInFilesinkQueue, NULL);
            // printf("filesink::buffers_in_display_queue:%u\n", buffersInFilesinkQueue);
        }
    }

    LOG_INFO(subprocess) << "Exiting PushDataToFilesinkThread processing thread";
}

void GStreamerAppSrcOutduct::PushDataToDisplayThread()
{
    static HdtnGstHandoffUtils_t hdtnGstHandoffUtils;
    static padded_vector_uint8_t incomingRtpFrame;
    incomingRtpFrame.reserve(1600);

    while (m_runDisplayThread.load(std::memory_order_acquire)) {
        
        bool notInWaitForNewPacketsState = m_rtpPacketToDisplayAsyncListenerPtr->TryWaitForIncomingDataAvailable();
        if (notInWaitForNewPacketsState) {
            
            m_rtpPacketToDisplayAsyncListenerPtr->Lock();
                incomingRtpFrame = std::move(m_rtpPacketToDisplayAsyncListenerPtr->m_queue.front());
                m_rtpPacketToDisplayAsyncListenerPtr->PopFront();
            m_rtpPacketToDisplayAsyncListenerPtr->Unlock();
            m_rtpPacketToDisplayAsyncListenerPtr->Notify();

            /* Create a new empty buffer */
            hdtnGstHandoffUtils.buffer = gst_buffer_new_and_alloc(incomingRtpFrame.size());

            /* copy in from our local queue */
            gst_buffer_map(hdtnGstHandoffUtils.buffer, &hdtnGstHandoffUtils.map, GST_MAP_WRITE);
            memcpy(hdtnGstHandoffUtils.map.data, incomingRtpFrame.data(), incomingRtpFrame.size());
         
            /* Set its timestamp and duration */
            GST_BUFFER_PTS(hdtnGstHandoffUtils.buffer) = gst_util_uint64_scale(m_numDisplaySamples, GST_SECOND, SAMPLE_RATE);
            GST_BUFFER_DURATION(hdtnGstHandoffUtils.buffer) = duration;

            gst_buffer_unmap(hdtnGstHandoffUtils.buffer, &hdtnGstHandoffUtils.map);

            /* Push the buffer into the appsrc */
            hdtnGstHandoffUtils.ret = gst_app_src_push_buffer((GstAppSrc *) m_displayAppsrc, hdtnGstHandoffUtils.buffer); // takes ownership of buffer we DO NOT deref

            m_numDisplaySamples += 1;
        }   
    }

    LOG_INFO(subprocess) << "Exiting PushDataToDisplayThread processing thread";

    return; // get here only when shutting down
}

int GStreamerAppSrcOutduct::PushRtpPacketToGStreamerOutduct(padded_vector_uint8_t &rtpPacketToTake)
{
    m_bundleCallbackAsyncListenerPtr->Lock();
        if (m_bundleCallbackAsyncListenerPtr->m_queue.full())
            m_totalIncomingCbOverruns++;
        m_bundleCallbackAsyncListenerPtr->m_queue.push_back(std::move(rtpPacketToTake));  // copy out bundle to circular buffer for sending
    m_bundleCallbackAsyncListenerPtr->Unlock();
    m_bundleCallbackAsyncListenerPtr->Notify();
    return 0;
}


void GStreamerAppSrcOutduct::OnBusMessages()
{
    while (m_running.load(std::memory_order_acquire)) 
    {
        GstMessage * msg = gst_bus_timed_pop(m_bus, GST_MSECOND*100);
        if (!msg) 
            continue;
        switch (GST_MESSAGE_TYPE (msg)) 
        {
            case GST_MESSAGE_ERROR: 
            {
                GError *err;
                gchar *debug;

                gst_message_parse_error (msg, &err, &debug);
                LOG_INFO(subprocess) << "Error:" << err->message;
                g_error_free (err);
                g_free (debug);
                break;
            }
            case GST_MESSAGE_EOS:
            {
                /* end-of-stream */
                LOG_INFO(subprocess) << "Got GST_MESSAGE_EOS";
                m_running = false;
                // gst_element_set_state (m_pipeline, GST_STATE_NULL);
                break;
            }
            case GST_MESSAGE_BUFFERING: 
            {
                break;
            }
            case GST_MESSAGE_TAG:
            {
                // gst_message_parse_tag(msg, &list);                
                LOG_INFO(subprocess) << "Got tag message from element " << GST_OBJECT_NAME(msg->src);
                break;
            }
            case GST_MESSAGE_ASYNC_DONE:
            {
                LOG_INFO(subprocess) << "Got GST_MESSAGE_ASYNC_DONE";
                break;
            }
            case GST_MESSAGE_STATE_CHANGED:
            {
                LOG_INFO(subprocess) << "Got GST_MESSAGE_STATE_CHANGED";
                break;
            }
            case GST_MESSAGE_CLOCK_LOST:
            {
                break;
            }
            default:
            {
                /* Unhandled message */
                break;
            }
        }
    }
}



#include "GStreamerAppSinkInduct.h"


#include "Logger.h"
#include "ThreadNamer.h"

static constexpr hdtn::Logger::SubProcess subprocess = hdtn::Logger::SubProcess::none;

/**
 * I do not know if there is a better way to do this. The problem is complicated:
 * 1. We can not use member functions for the GStreamer callbacks since they are pure C and do not 
 *  support the 'this' keyword.
 * 2. Therefore we can not set a callback function the usual way in the constructor.
 * 3. The callback function can not be a member of the class since the Gstreamder callbacks 
 * are not member functions
*/
static WholeBundleReadyCallback_t s_wholeBundleReadyCallback;
void GStreamerAppSinkInduct::SetCallbackFunction(const WholeBundleReadyCallback_t& wholeBundleReadyCallback)
{
    s_wholeBundleReadyCallback = wholeBundleReadyCallback;
}

static void OnPadAdded(GstElement *element, GstPad *pad, GStreamerAppSinkInduct *GStreamerAppSinkInduct)
{
    GstCaps *newPadCaps = NULL;
    GstStructure *newPadStruct = NULL;
    const gchar *newPadType = NULL;


    LOG_INFO(subprocess) << "Received new pad " << GST_PAD_NAME (pad) << " from " << GST_ELEMENT_NAME (element);
    LOG_INFO(subprocess) << "Attempting to link pads";
    GstPad *sinkpad = gst_element_get_static_pad((GstElement *) GStreamerAppSinkInduct, "sink");;
    if (gst_pad_is_linked (sinkpad)) {
        LOG_INFO(subprocess) << "We are already linked. Ignoring.";
        gst_object_unref(sinkpad);
        return;
    }

    /* Check the new pad's type */
    newPadCaps = gst_pad_get_current_caps (pad);
    newPadStruct = gst_caps_get_structure (newPadCaps, 0);
    newPadType = gst_structure_get_name (newPadStruct);
    LOG_INFO(subprocess) << newPadType;
    if (!g_str_has_prefix (newPadType, "video/x-h")) {
        LOG_INFO(subprocess) << "It has type " << newPadType << "which is not h2XX video. Ignoring." ;
        gst_object_unref(sinkpad);
        return;
    }

    /* We can now link this pad with the its corresponding sink pad */
    LOG_INFO(subprocess) << "Dynamic pad created, linking qtdemuxer/h264parser";
    int ret = gst_pad_link (pad, sinkpad);
    if (GST_PAD_LINK_FAILED (ret)) {
        LOG_INFO(subprocess) << "Type is " << newPadType << " but link failed";
    } else {
        LOG_INFO(subprocess) << "Link succeeded (type " << newPadType << ")";
    }
    gst_object_unref(sinkpad);
}

static void OnNewSampleFromSink(GstElement *element, GStreamerAppSinkInduct *gStreamerAppSinkInduct)
{
    (void)gStreamerAppSinkInduct;
    GstSample *sample;
    GstBuffer *buffer;
    GstMapInfo map;

    /* get the sample from appsink */
    sample = gst_app_sink_pull_sample(GST_APP_SINK (element));
    buffer = gst_sample_get_buffer(sample);
    if(!gst_buffer_map(buffer, &map, GST_MAP_READ)) {
      LOG_WARNING(subprocess) <<"could not map buffer";
      return;
    }

    /* Copy into buffer, from here we can std::move rather than copy */
    padded_vector_uint8_t bufferToForward(map.size);
    memcpy(bufferToForward.data(), map.data, map.size);
    
    /* the order here matters, unref before exporting bundle */
    gst_sample_unref(sample);  

    /* push buffer to HDTN */
    s_wholeBundleReadyCallback(bufferToForward);
}

GStreamerAppSinkInduct::GStreamerAppSinkInduct(std::string fileToStream) : 
    m_fileToStream(fileToStream),
    m_running(true),
    m_bus(NULL),
    m_gstMsg(NULL),
    m_pipeline(NULL),
    m_filesrc(NULL),
    m_qtdemux(NULL),
    m_h264parse(NULL),
    m_h264timestamper(NULL),
    m_rtph264pay(NULL),
    m_appsink(NULL),
    m_progressreport(NULL)
{
    gst_init(NULL, NULL);     // Initialize gstreamer first

    CreateElements();
    BuildPipeline();

    m_busMonitoringThread = boost::make_unique<boost::thread>(
        boost::bind(&GStreamerAppSinkInduct::OnBusMessages, this)); 

    StartPlaying();
}

GStreamerAppSinkInduct::~GStreamerAppSinkInduct()
{
    LOG_INFO(subprocess) << "Calling GStreamerAppSinkInduct deconstructor";   
    m_running = false;
    m_busMonitoringThread->join();
    gst_element_set_state(m_pipeline, GST_STATE_NULL);
}


int GStreamerAppSinkInduct::CreateElements()
{
    m_filesrc        =  gst_element_factory_make("filesrc", NULL);
    m_qtdemux        =  gst_element_factory_make("qtdemux", NULL);
    m_h264parse      =  gst_element_factory_make("h264parse", NULL);
    m_rtph264pay     =  gst_element_factory_make("rtph264pay", NULL);
    m_appsink        =  gst_element_factory_make("appsink", NULL);
    m_progressreport =  gst_element_factory_make("progressreport", NULL);
    m_pipeline       =  gst_pipeline_new(NULL);

    if (!m_filesrc || !m_qtdemux || !m_h264parse || !m_rtph264pay  || !m_appsink || !m_progressreport || !m_pipeline) 
    {
        LOG_ERROR(subprocess) << "Could not construct all gstreamer objects, aborting";
        return -1;
    }

    g_object_set(G_OBJECT(m_filesrc), "location", m_fileToStream.c_str(), NULL);
    g_object_set(G_OBJECT(m_progressreport), "update-freq", 1, NULL);
    
    /* config-interval is absolutely critical, stream can not be decoded on otherside without it. -1 = with every IDR frame */
    /* https://gstreamer.freedesktop.org/documentation/rtp/rtph264pay.html?gi-language=c#GstRtpH264AggregateMode */ // "aggregate-mode", 0,
    g_object_set(G_OBJECT(m_rtph264pay), "mtu", 1400, "config-interval", -1,  NULL);
    
    return 0;
}

int GStreamerAppSinkInduct::BuildPipeline()
{
    LOG_INFO(subprocess) << "Building Pipeline to stream " << m_fileToStream;
    
    gst_bin_add_many(GST_BIN(m_pipeline), m_filesrc,  m_qtdemux, m_h264parse,  m_rtph264pay, m_progressreport, m_appsink, NULL);
    
    if (!gst_element_link(m_filesrc, m_qtdemux)) {
        LOG_ERROR(subprocess) << "Source and qtmux could not be linked";
        return -1;
    }

    // byte stream vs avc (h264 packetized) ---> https://stackoverflow.com/questions/6342224/what-is-the-difference-between-byte-stream-and-packetized-stream-in-gstreamer-rt
    // avc is packetized to the maximum transmission unit
    // GstCaps * caps = gst_caps_from_string("video/x-h264, stream-format=(string)avc, alignment=(string)au"); // au = output buffer contains the NALs for a whole frame
    // gst_caps_unref(caps);
    // GstCaps * caps = gst_caps_from_string("video/x-h264, stream-format=(string)byte-stream, alignment=(string)nal"); // nal = output buffer contains complete NALs, but those do not need to represent a whole frame.
    // GstCaps * caps = gst_caps_from_string("video/x-h264, stream-format=(string)byte-stream, alignment=(string)au");

    if (!gst_element_link_many(m_h264parse, m_rtph264pay, m_progressreport, m_appsink, NULL)) {
        LOG_ERROR(subprocess) << "Pipeline could not be linked";
        return -1;
    }

    /**
     * Elements can not be linked until their pads are created. Pads on the qtdemux are not created until the video 
     * source provides "enough information" to the plugin that it can determine the type of media. We hook up a callback
     * function here to link the two halves of the pipeline together when the pad is added
    */
    g_signal_connect(m_qtdemux, "pad-added", G_CALLBACK(OnPadAdded), m_h264parse);
    
    /* we use appsink in push mode, it sends us a signal when data is available
    * and we pull out the data in the signal callback. Also, enable sync so the data is livestreamed 
    and not all sent in a single instant */
    g_object_set(G_OBJECT(m_appsink), "emit-signals", TRUE, "sync", TRUE, NULL);
    g_signal_connect(m_appsink, "new-sample", G_CALLBACK(OnNewSampleFromSink), NULL);
    
    /**
     * Register callback function to be notified of bus messages
    */
    m_bus = gst_element_get_bus(m_pipeline);
   
    LOG_INFO(subprocess) << "Succesfully built pipeline";
    GST_DEBUG_BIN_TO_DOT_FILE((GstBin *) m_pipeline, GST_DEBUG_GRAPH_SHOW_ALL, "gst_induct");

    return 0;
}

int GStreamerAppSinkInduct::StartPlaying()
{
    GstStateChangeReturn gstStateChangeReturn = gst_element_set_state(m_pipeline, GST_STATE_PLAYING);
    if (gstStateChangeReturn == GST_STATE_CHANGE_FAILURE) {
        LOG_ERROR(subprocess) << "Unable to set the pipeline to the playing state";
        return -1;
    }
    
    LOG_INFO(subprocess) << "Going to set state to play";
    return 0;
}



void GStreamerAppSinkInduct::OnBusMessages()
{
    while (m_running.load(std::memory_order_acquire)) 
    {
        GstMessage * msg = gst_bus_timed_pop(m_bus, GST_MSECOND * 100);
        if (!msg) 
            continue;

        switch (GST_MESSAGE_TYPE (msg)) 
        {
            case GST_MESSAGE_ERROR: 
            {
                GError *err;
                gchar *debug;

                gst_message_parse_error (msg, &err, &debug);
                LOG_INFO(subprocess) << "Error:" << err->message << " Debugging info: " << (debug ? debug : "none");
                g_error_free (err);
                g_free (debug);
                break;
            }
            case GST_MESSAGE_EOS:
            {
                /* end-of-stream */
                LOG_INFO(subprocess) << "Got GST_MESSAGE_EOS";
                gst_element_set_state (m_pipeline, GST_STATE_NULL);
                break;
            }
            case GST_MESSAGE_BUFFERING:
            {
                break;
            }
            case GST_MESSAGE_TAG:
            {
                LOG_INFO(subprocess) << "Got tag message";
                break;
            }
            case GST_MESSAGE_ASYNC_DONE:
            {
                LOG_INFO(subprocess) << "Got GST_MESSAGE_ASYNC_DONE";
                break;
            }
            case GST_MESSAGE_STATE_CHANGED:
            {
                GstState old_state, new_state;
                gst_message_parse_state_changed (msg, &old_state, &new_state, NULL);

                LOG_INFO(subprocess) << "Got GST_MESSAGE_STATE_CHANGED";
                LOG_INFO(subprocess) << "Element " << GST_OBJECT_NAME (msg->src) << " changed state from " << 
                    gst_element_state_get_name (old_state) << " to " << gst_element_state_get_name (new_state);
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
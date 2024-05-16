#include "GStreamerShmInduct.h"
#include "Logger.h"
#include "ThreadNamer.h"

static constexpr hdtn::Logger::SubProcess subprocess = hdtn::Logger::SubProcess::none;
static void OnNewSampleFromSink(GstElement *element, GStreamerShmInduct *gStreamerShmInduct);

/**
 * I do not know if there is a better way to do this. The problem is complicated:
 * 1. We can not use member functions for the GStreamer callbacks since they are pure C and do not 
 *  support the 'this' keyword.
 * 2. Therefore we can noon can not be a member of the class since the Gstreamder callbacks 
 * are not member functt set a callback function the usual way in the constructor.
 * 3. The callback functiions
*/
static WholeBundleReadyCallback_t s_wholeBundleReadyCallback;
void GStreamerShmInduct::SetShmInductCallbackFunction(const WholeBundleReadyCallback_t& wholeBundleReadyCallback)
{
    s_wholeBundleReadyCallback = wholeBundleReadyCallback;
}


GStreamerShmInduct::GStreamerShmInduct(std::string shmSocketPath) :
    m_shmSocketPath(shmSocketPath),
    m_running(true),
    m_bus(NULL),
    m_gstMsg(NULL),
    m_pipeline(NULL),
    m_shmsrc(NULL),
    m_queue(NULL),
    m_appsink(NULL)
{
    gst_init(NULL, NULL);     // Initialize gstreamer first

    CreateElements();
    BuildPipeline();

    m_busMonitoringThread = boost::make_unique<boost::thread>(
        boost::bind(&GStreamerShmInduct::OnBusMessages, this)); 

    StartPlaying();
}

GStreamerShmInduct::~GStreamerShmInduct()
{
    LOG_INFO(subprocess) << "Calling GStreamerShmInduct deconstructor";
    m_running = false;
    m_busMonitoringThread->join();
    gst_element_set_state(m_pipeline, GST_STATE_NULL);
}

int GStreamerShmInduct::CreateElements() 
{
    m_pipeline = gst_pipeline_new(NULL);
    m_shmsrc = gst_element_factory_make("shmsrc", NULL);
    m_queue = gst_element_factory_make("queue", NULL);
    m_appsink = gst_element_factory_make("appsink", NULL);

    if (!m_pipeline || !m_shmsrc || !m_queue || !m_appsink) {
        LOG_ERROR(subprocess) << "Could not create elements";
        return -1;
    }

    g_object_set(G_OBJECT(m_shmsrc), "socket-path", m_shmSocketPath.c_str(), "is-live", true, "do-timestamp", true, NULL);
    g_object_set(G_OBJECT(m_queue), "max-size-buffers", 0, "max-size-bytes", 0, "max-size-time", 0, "leaky", 0, NULL );
    g_object_set(G_OBJECT(m_appsink), "emit-signals", true, "sync", true, NULL);
    g_signal_connect(m_appsink, "new-sample", G_CALLBACK(OnNewSampleFromSink), NULL);
    return 0;
}

int GStreamerShmInduct::BuildPipeline() 
{
    LOG_INFO(subprocess) << "Building Pipeline to stream from socket " << m_shmSocketPath;
    
    gst_bin_add_many(GST_BIN(m_pipeline), m_shmsrc, m_queue, m_appsink, NULL);
    
    if (!gst_element_link_many(m_shmsrc, m_queue, m_appsink, NULL)) 
    {
        LOG_ERROR(subprocess) << "Could not link pipeline.";
        return -1;
    } 
    
    m_bus = gst_element_get_bus(m_pipeline);
   
    LOG_INFO(subprocess) << "Succesfully built pipeline";
    GST_DEBUG_BIN_TO_DOT_FILE((GstBin *) m_pipeline, GST_DEBUG_GRAPH_SHOW_ALL, "gst_induct");

    return 0;
}

int GStreamerShmInduct::StartPlaying() 
{
    LOG_INFO(subprocess) << "Going to set state to play";
    GstStateChangeReturn gstStateChangeReturn = gst_element_set_state(m_pipeline, GST_STATE_PLAYING);
    if (gstStateChangeReturn == GST_STATE_CHANGE_FAILURE) {
        LOG_ERROR(subprocess) << "Unable to set the pipeline to the playing state";
        return -1;
    }
        
    return 0;
}


void OnNewSampleFromSink(GstElement *element, GStreamerShmInduct *gStreamerShmInduct)
{
    (void)gStreamerShmInduct;
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
    
    /* the order here matters, unref before exporting bundle, also unrefs buffer */
    gst_sample_unref(sample);  

    /* push buffer to HDTN */
    s_wholeBundleReadyCallback(bufferToForward);
}

void GStreamerShmInduct::OnBusMessages()
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
                LOG_INFO(subprocess) << "Error:" << err->message;
                g_error_free (err);
                g_free (debug);
                break;
            }
            case GST_MESSAGE_EOS:
                /* end-of-stream */
                LOG_INFO(subprocess) << "Got GST_MESSAGE_EOS";
                gst_element_set_state (m_pipeline, GST_STATE_NULL);
                break;
            case GST_MESSAGE_BUFFERING: 
                break;
            case GST_MESSAGE_TAG:
                LOG_INFO(subprocess) << "Got tag message";
                break;
            case GST_MESSAGE_ASYNC_DONE:
                LOG_INFO(subprocess) << "Got GST_MESSAGE_ASYNC_DONE";
                break;
            case GST_MESSAGE_STATE_CHANGED:
                LOG_INFO(subprocess) << "Got GST_MESSAGE_STATE_CHANGED";
                break;
            case GST_MESSAGE_CLOCK_LOST:
                break;
            default:
                /* Unhandled message */
                break;
        }
    }
}
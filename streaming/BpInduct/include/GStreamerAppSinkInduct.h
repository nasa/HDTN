#include <gst/gst.h>
#include <gst/app/gstappsrc.h>
#include <gst/app/gstappsink.h>

#include <boost/asio.hpp>
#include <boost/process.hpp>
#include <boost/smart_ptr/make_unique.hpp>
#include <boost/thread/thread.hpp>

#include "DtnUtil.h"
#include "DtnRtpFrame.h"
#include "PaddedVectorUint8.h"
#include "streaming_lib_export.h"

typedef boost::function<void(padded_vector_uint8_t & wholeBundleVec)> WholeBundleReadyCallback_t;



class GStreamerAppSinkInduct
{
public:

    STREAMING_LIB_EXPORT GStreamerAppSinkInduct(std::string fileToStream);
    STREAMING_LIB_EXPORT ~GStreamerAppSinkInduct();
    STREAMING_LIB_EXPORT static void SetCallbackFunction(const WholeBundleReadyCallback_t& wholeBundleReadyCallback);
private:
    std::string m_fileToStream;

    std::unique_ptr<boost::thread> m_busMonitoringThread;

    STREAMING_LIB_NO_EXPORT void OnBusMessages();
    // setup functions
    STREAMING_LIB_NO_EXPORT int CreateElements();
    STREAMING_LIB_NO_EXPORT int BuildPipeline();
    STREAMING_LIB_NO_EXPORT int StartPlaying();

    std::atomic<bool> m_running;

    // members
    GstBus *m_bus;
    GstMessage *m_gstMsg;
    
    GstElement *m_pipeline;
    GstElement *m_filesrc;
    GstElement *m_qtdemux;
    GstElement *m_h264parse;
    GstElement *m_h264timestamper;
    GstElement *m_rtph264pay;
    GstElement *m_appsink;
    GstElement *m_progressreport;
};
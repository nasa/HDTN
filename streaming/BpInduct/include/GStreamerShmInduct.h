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



class GStreamerShmInduct
{
public:

    STREAMING_LIB_EXPORT GStreamerShmInduct(std::string shmSocketPath);
    STREAMING_LIB_EXPORT ~GStreamerShmInduct();
    STREAMING_LIB_EXPORT static void SetShmInductCallbackFunction(const WholeBundleReadyCallback_t& wholeBundleReadyCallback);

private:
    STREAMING_LIB_NO_EXPORT int CreateElements();
    STREAMING_LIB_NO_EXPORT int BuildPipeline();
    STREAMING_LIB_NO_EXPORT int StartPlaying();
    
    std::string m_shmSocketPath;
    std::atomic<bool> m_running;
    
    std::unique_ptr<boost::thread> m_busMonitoringThread;
    STREAMING_LIB_NO_EXPORT void OnBusMessages();

    // members
    GstBus *m_bus;
    GstMessage *m_gstMsg;
    
    GstElement *m_pipeline;
    GstElement *m_shmsrc;
    GstElement *m_queue;
    GstElement *m_appsink;
};


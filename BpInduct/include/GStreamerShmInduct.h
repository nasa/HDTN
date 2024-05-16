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

typedef boost::function<void(padded_vector_uint8_t & wholeBundleVec)> WholeBundleReadyCallback_t;

void SetShmInductCallbackFunction(const WholeBundleReadyCallback_t& wholeBundleReadyCallback);

class GStreamerShmInduct
{
public:

    GStreamerShmInduct(std::string shmSocketPath);
    ~GStreamerShmInduct();

private:
    int CreateElements();
    int BuildPipeline();
    int StartPlaying();
    
    std::string m_shmSocketPath;
    std::atomic<bool> m_running;
    
    std::unique_ptr<boost::thread> m_busMonitoringThread;
    void OnBusMessages();

    // members
    GstBus *m_bus;
    GstMessage *m_gstMsg;
    
    GstElement *m_pipeline;
    GstElement *m_shmsrc;
    GstElement *m_queue;
    GstElement *m_appsink;
};


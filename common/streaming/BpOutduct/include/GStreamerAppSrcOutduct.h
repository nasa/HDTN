/**
 * @file GStreamerAppSrcOutduct.h
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
#include "AsyncListener.h"
#include "streaming_lib_export.h"

#define SAMPLE_RATE 90000
#define DEFAULT_NUM_CIRC_BUFFERS 1000000

#define GST_HDTN_OUTDUCT_SOCKET_PATH "/tmp/hdtn_gst_shm_outduct"
#define GST_APPSRC_MAX_BYTES_IN_BUFFER 20000000
#define MAX_NUM_BUFFERS_QUEUE (UINT16_MAX) // once around an rtp sequence overflow
#define MAX_SIZE_BYTES_QUEUE (0) // 0 = disable
#define MAX_SIZE_TIME_QUEUE (0) // 0 = disable
#define MIN_THRESHHOLD_TIME_QUEUE_NS (500000) //(3e9) // Min. amount of data in the queue to allow reading (in ns, 0=disable)

#define RTP_LATENCY_MILLISEC (500) 
#define RTP_MAX_DROPOUT_TIME_MILLISEC (200) // The maximum time (milliseconds) of missing packets tolerated.
#define RTP_MAX_MISORDER_TIME_MIILISEC (60000) //The maximum time (milliseconds) of misordered packets tolerated.
#define RTP_MODE (1) // gst default
 

typedef boost::function<void(padded_vector_uint8_t & wholeBundleVec)> WholeBundleReadyCallback_t;
typedef  boost::circular_buffer<padded_vector_uint8_t> CbQueue_t;


class GStreamerAppSrcOutduct
{
public:
    
    STREAMING_LIB_EXPORT GStreamerAppSrcOutduct(std::string shmSocketPath, std::string gstCaps);
    STREAMING_LIB_EXPORT ~GStreamerAppSrcOutduct();

    STREAMING_LIB_EXPORT int PushRtpPacketToGStreamerOutduct(padded_vector_uint8_t& rtpPacketToTake);
    
    STREAMING_LIB_EXPORT bool TryWaitForIncomingDataAvailable(const boost::posix_time::time_duration& timeout);

    STREAMING_LIB_EXPORT static void SetGStreamerAppSrcOutductInstance(GStreamerAppSrcOutduct* gStreamerAppSrcOutduct);
    
    CbQueue_t m_incomingRtpPacketQueue;
    CbQueue_t m_incomingRtpPacketQueueForDisplay; 
    CbQueue_t m_incomingRtpPacketQueueForFilesink;

    uint64_t m_numFilesinkSamples = 0;
    uint64_t m_numDisplaySamples = 0;
   
private:
    
    std::unique_ptr<AsyncListener<CbQueue_t>> m_bundleCallbackAsyncListenerPtr;
    std::unique_ptr<AsyncListener<CbQueue_t>> m_rtpPacketToDisplayAsyncListenerPtr;
    std::unique_ptr<AsyncListener<CbQueue_t>> m_rtpPacketToFilesinkAsyncListenerPtr;

    // thread members
    std::unique_ptr<boost::thread> m_packetTeeThread;
    std::unique_ptr<boost::thread> m_displayThread;
    std::unique_ptr<boost::thread> m_filesinkThread;
    std::unique_ptr<boost::thread> m_busMonitoringThread;

    std::string m_shmSocketPath;
    std::string m_gstCaps;
    std::atomic<bool> m_running;
    std::atomic<bool> m_runDisplayThread;
    std::atomic<bool> m_runFilesinkThread;
    // gst members
    GstBus *m_bus;

    /* setup functions */
    STREAMING_LIB_NO_EXPORT int CreateElements();
    STREAMING_LIB_NO_EXPORT int BuildPipeline();
    STREAMING_LIB_NO_EXPORT int StartPlaying();
    STREAMING_LIB_NO_EXPORT int CheckInitializationSuccess();

    /* Operating functions */
    STREAMING_LIB_NO_EXPORT void OnBusMessages();
    STREAMING_LIB_NO_EXPORT void TeeDataToQueuesThread();
    STREAMING_LIB_NO_EXPORT void PushDataToFilesinkThread();
    STREAMING_LIB_NO_EXPORT void PushDataToDisplayThread();


    /* pipeline members */

    // To display
    GstElement *m_pipeline;
    GstElement *m_displayAppsrc;
    /* cap goes here*/
    GstElement *m_displayQueue;
    GstElement *m_rtpjitterbuffer;
    GstElement *m_rtph264depay;
    GstElement *m_h264parse;
    GstElement *m_h264timestamper;
    GstElement *m_decodeQueue;
    GstElement *m_avdec_h264;
    GstElement *m_postDecodeQueue;
    GstElement *m_displayShmsink; 

    // To filesink 
    GstElement *m_filesinkAppsrc;
    GstElement *m_filesinkQueue;
    GstElement *m_filesinkShmsink;

    // stat keeping 
    uint64_t m_totalIncomingCbOverruns;
    uint64_t m_totalFilesinkCbOverruns;
    uint64_t m_totalDisplayCbOverruns;
};



struct HdtnGstHandoffUtils_t {
    GstBuffer *buffer;
    GstMapInfo map;
    GstFlowReturn ret;
};




// Use sync=true if:
// There is a human watching the output, e.g. movie playback
// Use sync=false if:
// You are using a live source
// The pipeline is being post-processed, e.g. neural net

//     alignment=au means that each output buffer contains the NALs for a whole
// picture. alignment=nal just means that each output buffer contains
// complete NALs, but those do not need to represent a whole frame.
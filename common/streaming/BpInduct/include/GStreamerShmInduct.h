/**
 * @file GStreamerShmInduct.h
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


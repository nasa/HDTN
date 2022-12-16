/**
 * @file TelemetryRunner.h
 *
 * @copyright Copyright Ⓒ 2021 United States Government as represented by
 * the National Aeronautics and Space Administration.
 * No copyright is claimed in the United States under Title 17, U.S.Code.
 * All Other Rights Reserved.
 *
 * @section LICENSE
 * Released under the NASA Open Source Agreement (NOSA)
 * See LICENSE.md in the source root directory for more information.
 *
 * @section DESCRIPTION
 *
 * This TelemetryRunner class launches a thread that handles polling HDTN for telemetry data
 */

#ifndef TELEMETRY_RUNNER_H
#define TELEMETRY_RUNNER_H 1

#include <functional>

#include <boost/scoped_ptr.hpp>
#include <boost/thread.hpp>
#include "zmq.hpp"

#include "telem_lib_export.h"
#include "Telemetry.h"
#include "Metrics.h"
#include "TelemetryLogger.h"
#include "TelemetryRunnerProgramOptions.h"
#include "WebsocketServer.h"

class TelemetryRunner
{
    public:
        TELEM_LIB_EXPORT ~TelemetryRunner();
        TELEM_LIB_EXPORT bool Run(int argc, const char* const argv[], volatile bool & running);
        TELEM_LIB_EXPORT bool Init(zmq::context_t *inprocContextPtr, TelemetryRunnerProgramOptions& options);
        TELEM_LIB_EXPORT bool ShouldExit();
        TELEM_LIB_EXPORT void Stop();

    private:
        TELEM_LIB_NO_EXPORT void ThreadFunc(zmq::context_t * inprocContextPtr);
        TELEM_LIB_NO_EXPORT void MonitorExitKeypressThreadFunc();
        TELEM_LIB_NO_EXPORT void OnNewMetrics(Metrics::metrics_t metrics);
        TELEM_LIB_NO_EXPORT void ProcessIngressTelem(IngressTelemetry_t &telem, Metrics::metrics_t &metrics);
        TELEM_LIB_NO_EXPORT void ProcessEgressTelem(EgressTelemetry_t &telem, Metrics::metrics_t &metrics);
        TELEM_LIB_NO_EXPORT void ProcessStorageTelem(StorageTelemetry_t &telem, Metrics::metrics_t &metrics);

        volatile bool m_running;
        volatile bool m_runningFromSigHandler;
        std::unique_ptr<boost::thread> m_threadPtr;
        std::unique_ptr<WebsocketServer> m_websocketServerPtr;
        std::unique_ptr<TelemetryLogger> m_telemetryLoggerPtr;
};

#endif //TELEMETRY_RUNNER_H
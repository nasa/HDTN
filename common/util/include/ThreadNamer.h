/**
 * @file ThreadNamer.h
 * @author  Brian Tomko <brian.j.tomko@nasa.gov>
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
 *
 * @section DESCRIPTION
 *
 * 
 * This ThreadNamer static class is used to set the names of running threads created
 * with boost::thread or std::thread, or to set the name of the current/calling thread.
 */

#ifndef _THREAD_NAMER_H
#define _THREAD_NAMER_H 1

#include <boost/thread.hpp>
#include <boost/asio.hpp>
#include <thread>
#include <string>
#include "hdtn_util_export.h"

//#define THREAD_NAMER_ENABLE_DEPRECATED_FUNCTIONS 1

class ThreadNamer {
public:
    
    ThreadNamer() = delete;
    
#ifdef THREAD_NAMER_ENABLE_DEPRECATED_FUNCTIONS
    /** Set the thread name for boost::thread (PROPERLY WORKS ON WINDOWS ONLY).
     *
     * @param thread The already created boost::thread to set the name of.
     * @param threadName The name to assign to the thread.
     * 
     * @post The name of parameter thread is set.
     */
    HDTN_UTIL_EXPORT static void SetThreadName(boost::thread& thread, const std::string& threadName);

    /** Set the thread name for std::thread (PROPERLY WORKS ON WINDOWS ONLY).
     *
     * @param thread The already created std::thread to set the name of.
     * @param threadName The name to assign to the thread.
     *
     * @post The name of parameter thread is set.
     */
    HDTN_UTIL_EXPORT static void SetThreadName(std::thread& thread, const std::string& threadName);
#endif

    /** Set the thread name for the current/calling thread.
     *
     * @param threadName The name to assign to the current/calling thread.
     *
     * @post The name of the current/calling thread is set to threadName.
     */
    HDTN_UTIL_EXPORT static void SetThisThreadName(const std::string& threadName);

    /** Set the thread name for the running io_service thread.
     *
     * @param ioService The io_service which post a SetThisThreadName function to.
     * @param threadName The name to assign to the thread that is running the io_service.
     *
     * @post The name of the thread that is running the io_service is set.
     */
    HDTN_UTIL_EXPORT static void SetIoServiceThreadName(boost::asio::io_service& ioService, const std::string& threadName);
    
    
};



#endif //_THREAD_NAMER_H

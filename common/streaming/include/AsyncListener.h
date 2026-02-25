/**
 * @file AsyncListener.h
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

// THIS IS NOT THREAD SAFE 
#include <boost/asio.hpp>
#include <boost/process.hpp>
#include <boost/smart_ptr/make_unique.hpp>
#include <boost/thread/thread.hpp>


template <typename T> 
class AsyncListener
{

public:
    AsyncListener(T &queue, boost::posix_time::time_duration timeout);
    ~AsyncListener();

    bool TryWaitForIncomingDataAvailable();

    inline void Lock();
    inline void Unlock();
    inline void Notify();
    
    void PopFront();

    T &m_queue;

private:
    boost::mutex m_mux;
    boost::condition_variable m_cv;
    
    boost::posix_time::time_duration m_timeout;
   
};


template<class T>
AsyncListener<T>::AsyncListener(T &queue, boost::posix_time::time_duration timeout) : 
    m_queue(queue), m_timeout(timeout)
{
}

template<class T>
AsyncListener<T>::~AsyncListener()
{
}


/**
 * If return true, we have data
 * If return false, we do not have data
*/
template<class T>
bool AsyncListener<T>::TryWaitForIncomingDataAvailable() {
    boost::mutex::scoped_lock lock(m_mux);
    if (!m_queue.empty()) {
        return true;
    }
    m_cv.timed_wait(lock, m_timeout);
    return !m_queue.empty(); //true => data available, false => data not avaiable
}


template <typename T>
void AsyncListener<T>::PopFront()
{
    m_queue.pop_front();
}



template <typename T>
inline void AsyncListener<T>::Lock()
{
    m_mux.lock();
}

template <typename T>
inline void AsyncListener<T>::Unlock()
{
    m_mux.unlock();
}

template <typename T>
inline void AsyncListener<T>::Notify()
{
    m_cv.notify_one();
}

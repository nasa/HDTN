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
    bool GetNextIncomingItemTimeout();

    inline void Lock();
    inline void Unlock();
    inline void Notify();
    
    void PushFront();
    void PopFront();
    void MoveFront();

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
    if (m_queue.size() == 0) { // if empty, we wait
        return GetNextIncomingItemTimeout();
    }
    return true; 
}

template<class T>
bool AsyncListener<T>::GetNextIncomingItemTimeout()
{
    {
        boost::mutex::scoped_lock lock(m_mux);
        if ((m_queue.size() == 0)) {
            m_cv.timed_wait(lock, m_timeout); //lock mutex (above) before checking condition
            return false;
        }
    }
    
    return true;
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

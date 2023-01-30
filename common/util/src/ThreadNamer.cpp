/**
 * @file ThreadNamer.cpp
 * @author  Brian Tomko <brian.j.tomko@nasa.gov>
 *
 * @copyright Copyright © 2021 United States Government as represented by
 * the National Aeronautics and Space Administration.
 * No copyright is claimed in the United States under Title 17, U.S.Code.
 * All Other Rights Reserved.
 *
 * @section LICENSE
 * Released under the NASA Open Source Agreement (NOSA)
 * See LICENSE.md in the source root directory for more information.
 */

#include "ThreadNamer.h"
#include <cstdint>
#include "Logger.h"

static constexpr hdtn::Logger::SubProcess subprocess = hdtn::Logger::SubProcess::none;

//https://stackoverflow.com/questions/10121560/stdthread-naming-your-thread
#ifdef _WIN32
#include <cstdlib>
#include <windows.h>
const DWORD MS_VC_EXCEPTION = 0x406D1388;

#pragma pack(push,8)
typedef struct tagTHREADNAME_INFO
{
    DWORD dwType; // Must be 0x1000.
    LPCSTR szName; // Pointer to name (in user addr space).
    DWORD dwThreadID; // Thread ID (-1=caller thread).
    DWORD dwFlags; // Reserved for future use, must be zero.
} THREADNAME_INFO;
#pragma pack(pop)


static void ImplSetThreadNameByRaiseException(uint32_t dwThreadID, const std::string& threadName) {
    THREADNAME_INFO info;
    info.dwType = 0x1000;
    info.szName = threadName.c_str();
    info.dwThreadID = dwThreadID;
    info.dwFlags = 0;

    __try {
        RaiseException(MS_VC_EXCEPTION, 0, sizeof(info) / sizeof(ULONG_PTR), (ULONG_PTR*)&info);
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {

    }
}
static void ImplSetThreadNameBySetDescription(HANDLE nativeThreadHandle, const std::string& threadName) {
    std::vector<wchar_t> wideThreadNameVec(threadName.size() + 5);
    if (std::mbstowcs(wideThreadNameVec.data(), threadName.data(), wideThreadNameVec.size()) == static_cast<std::size_t> (-1)) {
        LOG_ERROR(subprocess) << "Cannot convert the following threadName to wide char: " << threadName;
        return; //error
    }

    //https://learn.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-setthreaddescription?redirectedfrom=MSDN
    // https://learn.microsoft.com/en-us/visualstudio/debugger/how-to-set-a-thread-name-in-native-code?view=vs-2022
    //   according to this:
    //     The use of SetThreadDescription is supported starting in Windows 10, version 1607 or Windows Server 2016.
    //     It is worth noting that both approaches can be used together, if desired, since the mechanisms by which they work are independent of each other.
    std::wstring s;
    HRESULT hr = SetThreadDescription(nativeThreadHandle, wideThreadNameVec.data());
    if (FAILED(hr)) {
        LOG_ERROR(subprocess) << "Cannot set thread description of " << threadName;
        // Call failed.
    }
}
static void ImplSetCurrentThreadName(const std::string& threadName) {
    ImplSetThreadNameByRaiseException(GetCurrentThreadId(), threadName);
    ImplSetThreadNameBySetDescription(GetCurrentThread(), threadName);
}

static void ImplSetThreadName(boost::thread& thread, const std::string& threadName) {
    DWORD threadId = ::GetThreadId(thread.native_handle());
    ImplSetThreadNameByRaiseException(threadId, threadName.c_str());
    ImplSetThreadNameBySetDescription(thread.native_handle(), threadName);
}

static void ImplSetThreadName(std::thread& thread, const std::string& threadName) {
    DWORD threadId = ::GetThreadId(thread.native_handle());
    ImplSetThreadNameByRaiseException(threadId, threadName.c_str());
    ImplSetThreadNameBySetDescription(thread.native_handle(), threadName);
}

#else
#include <sys/prctl.h>
static void ImplSetCurrentThreadName(const std::string& threadName) {
    prctl(PR_SET_NAME, threadName.c_str(), 0, 0, 0);
}

static void ImplSetThreadName(boost::thread& thread, const std::string& threadName) {
    pthread_setname_np(thread.native_handle(), threadName.c_str());
}

static void ImplSetThreadName(std::thread& thread, const std::string& threadName) {
    pthread_setname_np(thread.native_handle(), threadName.c_str());
}
#endif

void ThreadNamer::SetIoServiceThreadName(boost::asio::io_service& ioService, const std::string& threadName) {
    boost::asio::post(ioService, boost::bind(&ThreadNamer::SetThisThreadName, threadName));
}
void ThreadNamer::SetThreadName(boost::thread& thread, const std::string& threadName) {
    ImplSetThreadName(thread, threadName);
}
void ThreadNamer::SetThreadName(std::thread& thread, const std::string& threadName) {
    ImplSetThreadName(thread, threadName);
}
void ThreadNamer::SetThisThreadName(const std::string& threadName) {
    ImplSetCurrentThreadName(threadName);
}

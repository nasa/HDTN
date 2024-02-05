/**
 * @file LtpIpcEngine.cpp
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

#include "LtpIpcEngine.h"
#include "Logger.h"
#include <boost/make_unique.hpp>
#include <boost/lexical_cast.hpp>
#include "Sdnv.h"
#include "ThreadNamer.h"

static constexpr hdtn::Logger::SubProcess subprocess = hdtn::Logger::SubProcess::none;

static const uint8_t ENGINE_INDEX = 1; //this is a don't care for inducts, only needed for outducts (to mimic udp behavior)

LtpIpcEngine::LtpIpcEngine(
    const std::string& myTxSharedMemoryName,
    const uint64_t maxUdpRxPacketSizeBytes,
    const LtpEngineConfig& ltpRxOrTxCfg) :
    LtpEngine(ltpRxOrTxCfg, ENGINE_INDEX, true),
    m_myTxSharedMemoryName(myTxSharedMemoryName),
    M_REMOTE_ENGINE_ID(ltpRxOrTxCfg.remoteEngineId),
    m_myTxIpcControlPtr(NULL),
    m_myTxIpcPacketCbArray(NULL),
    m_myTxIpcDataStart(NULL),
    m_remoteTxIpcControlPtr(NULL),
    m_remoteTxIpcPacketCbArray(NULL),
    m_remoteTxIpcDataStart(NULL),
    M_NUM_CIRCULAR_BUFFER_VECTORS(ltpRxOrTxCfg.numUdpRxCircularBufferVectors),
    m_running(false),
    m_isInduct(ltpRxOrTxCfg.isInduct),
    m_resetInProgress(false),
    m_countAsyncSendCalls(0),
    m_countAsyncSendCallbackCalls(0),
    m_countBatchSendCalls(0),
    m_countBatchSendCallbackCalls(0),
    m_countBatchUdpPacketsSent(0),
    m_countCircularBufferOverruns(0),
    m_countUdpPacketsReceived(0)
{
    LOG_INFO(subprocess) << "LtpIpcEngine using " << M_NUM_CIRCULAR_BUFFER_VECTORS << " circular buffer elements";

    //owner/creator shall try to remove on constructor and destructor
    if (boost::interprocess::shared_memory_object::remove(m_myTxSharedMemoryName.c_str())) {
        LOG_INFO(subprocess) << "removed my shared memory " << m_myTxSharedMemoryName;
    }

    try {
        //Create a shared memory object.
        m_myTxSharedMemoryObjectPtr = boost::make_unique<boost::interprocess::shared_memory_object>(
            boost::interprocess::create_only, //only create
            m_myTxSharedMemoryName.c_str(), //name
            boost::interprocess::read_write);  //read-write mode

        //Set size
        m_myTxSharedMemoryObjectPtr->truncate(sizeof(IpcControl) 
            + (M_NUM_CIRCULAR_BUFFER_VECTORS * sizeof(IpcPacket))
            + (M_NUM_CIRCULAR_BUFFER_VECTORS * maxUdpRxPacketSizeBytes));

        //Map the whole shared memory in this process
        m_myTxShmMappedRegionPtr = boost::make_unique<boost::interprocess::mapped_region>(
            *m_myTxSharedMemoryObjectPtr, //What to map
            boost::interprocess::read_write); //Map it as read-write
    }
    catch (const boost::interprocess::interprocess_exception& e) {
        LOG_INFO(subprocess) << "LtpIpcEngine: Cannot create shared memory: " << e.what();
        m_myTxSharedMemoryObjectPtr.reset();
        m_myTxShmMappedRegionPtr.reset();
        return;
    }

    //Get the address of the mapped region
    uint8_t* addr = (uint8_t*)m_myTxShmMappedRegionPtr->get_address();

    //Construct the shared structure in memory
    m_myTxIpcControlPtr = new (addr) IpcControl(M_NUM_CIRCULAR_BUFFER_VECTORS, maxUdpRxPacketSizeBytes); //placement new
    addr += sizeof(IpcControl);
    m_myTxIpcPacketCbArray = (IpcPacket*)addr;
    addr += M_NUM_CIRCULAR_BUFFER_VECTORS * sizeof(IpcPacket);
    m_myTxIpcDataStart = addr;

    //initialize my tx buffer
    for (unsigned int i = 0; i < M_NUM_CIRCULAR_BUFFER_VECTORS; ++i) {
        IpcPacket& p = m_myTxIpcPacketCbArray[i];
        p.bytesTransferred = 0;
        //p.dataIndex = i;
    }

    //logic from LtpUdpEngineManager::AddLtpUdpEngine
    if ((ltpRxOrTxCfg.delaySendingOfReportSegmentsTimeMsOrZeroToDisable != 0) && (!m_isInduct)) {
        LOG_ERROR(subprocess) << "LtpUdpEngineManager::AddLtpUdpEngine: delaySendingOfReportSegmentsTimeMsOrZeroToDisable must be set to 0 for an outduct";
        return;
    }
    if ((ltpRxOrTxCfg.delaySendingOfDataSegmentsTimeMsOrZeroToDisable != 0) && (m_isInduct)) {
        LOG_ERROR(subprocess) << "LtpUdpEngineManager::AddLtpUdpEngine: delaySendingOfDataSegmentsTimeMsOrZeroToDisable must be set to 0 for an induct";
        return;
    }
    if (ltpRxOrTxCfg.maxUdpPacketsToSendPerSystemCall == 0) {
        LOG_ERROR(subprocess) << "LtpUdpEngineManager::AddLtpUdpEngine: maxUdpPacketsToSendPerSystemCall must be non-zero.";
        return;
    }
#ifdef UIO_MAXIOV
    //sendmmsg() is Linux-specific. NOTES The value specified in vlen is capped to UIO_MAXIOV (1024).
    if (ltpRxOrTxCfg.maxUdpPacketsToSendPerSystemCall > UIO_MAXIOV) {
        LOG_ERROR(subprocess) << "LtpUdpEngineManager::AddLtpUdpEngine: maxUdpPacketsToSendPerSystemCall ("
            << ltpRxOrTxCfg.maxUdpPacketsToSendPerSystemCall << ") must be <= UIO_MAXIOV (" << UIO_MAXIOV << ").";
        return;
    }
#endif //UIO_MAXIOV
    if (ltpRxOrTxCfg.senderPingSecondsOrZeroToDisable && m_isInduct) {
        LOG_ERROR(subprocess) << "LtpUdpEngineManager::AddLtpUdpEngine: senderPingSecondsOrZeroToDisable cannot be used with an induct (must be set to 0).";
        return;
    }
    if (ltpRxOrTxCfg.activeSessionDataOnDiskNewFileDurationMsOrZeroToDisable != 0) { //if enabled
        if (ltpRxOrTxCfg.activeSessionDataOnDiskNewFileDurationMsOrZeroToDisable < 1000) {
            LOG_ERROR(subprocess) << "LtpUdpEngineManager::AddLtpUdpEngine: activeSessionDataOnDiskNewFileDurationMsOrZeroToDisable, if non-zero, must be 1000ms minimum.";
            return;
        }
        else if (ltpRxOrTxCfg.activeSessionDataOnDiskDirectory.empty()) {
            LOG_ERROR(subprocess) << "LtpUdpEngineManager::AddLtpUdpEngine: activeSessionDataOnDiskDirectory must be defined when activeSessionDataOnDisk feature is enabled.";
            return;
        }
        else if (ltpRxOrTxCfg.maxSimultaneousSessions < 8) { //because of "static constexpr uint64_t diskPipelineLimit = 6;" in LtpEngine.cpp, want a number greater than 6
            LOG_ERROR(subprocess) << "LtpUdpEngineManager::AddLtpUdpEngine: when activeSessionDataOnDisk feature is enabled, maxSimultaneousSessions must be at least 8.";
            return;
        }
    }
}

LtpIpcEngine::~LtpIpcEngine() {
    Stop();

    m_myTxShmMappedRegionPtr.reset();
    m_myTxSharedMemoryObjectPtr.reset();
    //owner/creator shall try to remove on constructor and destructor
    if (boost::interprocess::shared_memory_object::remove(m_myTxSharedMemoryName.c_str())) {
        LOG_INFO(subprocess) << "removed my shared memory " << m_myTxSharedMemoryName;
    }

    m_remoteTxShmMappedRegionPtr.reset();
    m_remoteTxSharedMemoryObjectPtr.reset();
    //user shall try to remove on destructor only
    if ((!m_remoteTxSharedMemoryName.empty()) && boost::interprocess::shared_memory_object::remove(m_remoteTxSharedMemoryName.c_str())) {
        LOG_INFO(subprocess) << "removed remote shared memory " << m_remoteTxSharedMemoryName;
    }
    

    LOG_INFO(subprocess) << "~LtpUdpEngine: m_countAsyncSendCalls " << m_countAsyncSendCalls 
        << " m_countBatchSendCalls " << m_countBatchSendCalls
        << " m_countBatchUdpPacketsSent " << m_countBatchUdpPacketsSent
        << " m_countCircularBufferOverruns " << m_countCircularBufferOverruns
        << " m_countUdpPacketsReceived " << m_countUdpPacketsReceived;
}

void LtpIpcEngine::Stop() {
    m_running = false;
    for (unsigned int i = 0; i < 10; ++i) {
        if (m_remoteTxIpcControlPtr) {
            m_remoteTxIpcControlPtr->m_waitUntilNotEmpty_postHasData_semaphore.post(); //stop thread if in wait() state
        }
        m_myTxIpcControlPtr->m_waitUntilNotFull_postHasFreeSpace_semaphore.post(); //stop LtpIpcEngine::DoSendPacket if in wait() state
    }
    
    if (m_readRemoteTxShmThreadPtr) {
        try {
            m_readRemoteTxShmThreadPtr->join();
            m_readRemoteTxShmThreadPtr.reset(); //delete it
        }
        catch (const boost::thread_resource_error&) {
            LOG_ERROR(subprocess) << "error stopping LtpIpcEngine readRemoteTxShmThread";
        }
    }
}

bool LtpIpcEngine::Connect(const std::string& remoteTxSharedMemoryName) {

    m_remoteTxSharedMemoryName = remoteTxSharedMemoryName;

    //start worker thread
    m_running = true;
    m_readRemoteTxShmThreadPtr = boost::make_unique<boost::thread>(
        boost::bind(&LtpIpcEngine::ReadRemoteTxShmThreadFunc, this)); //create and start the worker thread
    return true;
}

void LtpIpcEngine::Reset_ThreadSafe_Blocking() {
    boost::mutex::scoped_lock cvLock(m_resetMutex);
    m_resetInProgress = true;
    boost::asio::post(m_ioServiceLtpEngine, boost::bind(&LtpIpcEngine::Reset, this));
    while (m_resetInProgress) { //lock mutex (above) before checking condition
        m_resetConditionVariable.wait(cvLock);
    }
}

void LtpIpcEngine::Reset() {
    LtpEngine::Reset();
    m_countAsyncSendCalls = 0;
    m_countAsyncSendCallbackCalls = 0;
    m_countBatchSendCalls = 0;
    m_countBatchSendCallbackCalls = 0;
    m_countBatchUdpPacketsSent = 0;
    m_countCircularBufferOverruns = 0;
    m_countUdpPacketsReceived = 0;
    m_resetMutex.lock();
    m_resetInProgress = false;
    m_resetMutex.unlock();
    m_resetConditionVariable.notify_one();
}

void LtpIpcEngine::ReadRemoteTxShmThreadFunc() {
    ThreadNamer::SetThisThreadName("LtpIpcEngReadShm");
    while (m_running.load(std::memory_order_acquire)) { //try to connect
        try {
            //Create a shared memory object.
            m_remoteTxSharedMemoryObjectPtr = boost::make_unique<boost::interprocess::shared_memory_object>(
                boost::interprocess::open_only,
                m_remoteTxSharedMemoryName.c_str(), //name
                boost::interprocess::read_write);  //read-write mode
        }
        catch (const boost::interprocess::interprocess_exception& e) {
            LOG_INFO(subprocess) << "LtpIpcEngine::Connect: Cannot open_only yet: " << e.what();
            m_remoteTxSharedMemoryObjectPtr.reset();
            boost::this_thread::sleep(boost::posix_time::milliseconds(1000));
            continue;
        }

        boost::this_thread::sleep(boost::posix_time::milliseconds(1000));

        try {
            //Map the whole shared memory in this process
            m_remoteTxShmMappedRegionPtr = boost::make_unique<boost::interprocess::mapped_region>(
                *m_remoteTxSharedMemoryObjectPtr, //What to map
                boost::interprocess::read_write); //Map it as read-write
        }
        catch (const boost::interprocess::interprocess_exception& e) {
            LOG_INFO(subprocess) << "LtpIpcEngine::Connect: Cannot create mapped_region: " << e.what();
            m_remoteTxSharedMemoryObjectPtr.reset();
            boost::this_thread::sleep(boost::posix_time::milliseconds(1000));
            continue;
        }

        //Get the address of the mapped region
        uint8_t* addr = (uint8_t*)m_remoteTxShmMappedRegionPtr->get_address();

        //Construct the shared structure in memory
        m_remoteTxIpcControlPtr = (IpcControl*)addr; //read (no placement new)
        addr += sizeof(IpcControl);
        //const unsigned int numCbElementsPlus1 = m_remoteTxIpcControlPtr->m_circularIndexBuffer.GetCapacity() + 1; //+1 for extra swappable element
        m_remoteTxIpcPacketCbArray = (IpcPacket*)addr;
        addr += M_NUM_CIRCULAR_BUFFER_VECTORS * sizeof(IpcPacket);
        m_remoteTxIpcDataStart = addr;
        break;
    }
    LOG_INFO(subprocess) << "Successfully connected to remoteTxSharedMemoryName: " << m_remoteTxSharedMemoryName;

    while (m_running.load(std::memory_order_acquire)) { //keep thread alive if running
#if 0
        const bool waitSuccess = m_remoteTxIpcControlPtr->m_waitUntilNotEmpty_postHasData_semaphore.try_wait();
        if (waitSuccess) {
            LOG_DEBUG(subprocess) << "T success";
        }
        else {
            m_remoteTxIpcControlPtr->m_waitUntilNotEmpty_postHasData_semaphore.wait();
            LOG_DEBUG(subprocess) << "T wasblocked";
        }
#else
        m_remoteTxIpcControlPtr->m_waitUntilNotEmpty_postHasData_semaphore.wait();
#endif
        //check that the destructor did not post by checking the m_running flag
        if (m_running.load(std::memory_order_acquire)) { //success not empty, read from remote tx
            const unsigned int readIndex = m_remoteTxIpcControlPtr->m_circularIndexBuffer.GetIndexForRead(); //store the volatile
            if (readIndex == CIRCULAR_INDEX_BUFFER_EMPTY) {
                LOG_FATAL(subprocess) << "LtpIpcEngine::ReadRemoteTxShmThreadFunc(): CIRCULAR BUFFER SHOULD NEVER HAVE AN EMPTY CONDITION!";
                continue;
            }
            m_remoteTxIpcControlPtr->m_circularIndexBuffer.CommitRead(); //protected by m_waitUntilNotFull_postHasFreeSpace_semaphore (.post() not called until data processed)
            
            //per: https://en.cppreference.com/w/cpp/atomic/memory_order
            // memory_order_relaxed is okay because this is an RMW,
            // and RMWs (with any ordering) following a release form a release sequence
            m_countUdpPacketsReceived.fetch_add(1, std::memory_order_relaxed);

            IpcPacket& p = m_remoteTxIpcPacketCbArray[readIndex];
            //if (readIndex != p.dataIndex) {
            //    LOG_FATAL(subprocess) << "LtpIpcEngine::ReadRemoteTxShmThreadFunc(): readIndex != p.dataIndex!";
            //    continue;
            //}
            //uint8_t* dataRead = &m_remoteTxIpcDataStart[p.dataIndex * m_remoteTxIpcControlPtr->m_bytesPerElement];
            uint8_t* dataRead = &m_remoteTxIpcDataStart[readIndex * m_remoteTxIpcControlPtr->m_bytesPerElement];
            if (!VerifyIpcPacketReceive(dataRead, p.bytesTransferred)) {
                LOG_ERROR(subprocess) << "LtpIpcEngine::ReadRemoteTxShmThreadFunc(): VerifyIpcPacketReceive failed.. dropping packet!";
                m_remoteTxIpcControlPtr->m_waitUntilNotFull_postHasFreeSpace_semaphore.post();
                continue;
            }
            PacketIn_ThreadSafe(dataRead, p.bytesTransferred); //Post to the LtpEngine IoService so its thread will process
        }
    }

    LOG_INFO(subprocess) << "LtpIpcEngine::ReadRemoteTxShmThreadFunc thread exiting";
}

void LtpIpcEngine::PacketInFullyProcessedCallback(bool success) {
    (void)success;
    //Called by LTP Engine thread
    m_remoteTxIpcControlPtr->m_waitUntilNotFull_postHasFreeSpace_semaphore.post();
}

bool LtpIpcEngine::VerifyIpcPacketReceive(uint8_t* data, std::size_t bytesTransferred) {
    if (bytesTransferred <= 2) {
        LOG_ERROR(subprocess) << "LtpIpcEngine::VerifyIpcPacketReceive(): bytesTransferred <= 2 .. ignoring packet";
        return false;
    }

    const uint8_t segmentTypeFlags = data[0]; // & 0x0f; //upper 4 bits must be 0 for version 0
    bool isSenderToReceiver;
    if (!Ltp::GetMessageDirectionFromSegmentFlags(segmentTypeFlags, isSenderToReceiver)) {
        LOG_FATAL(subprocess) << "LtpIpcEngine::VerifyIpcPacketReceive(): received invalid ltp packet with segment type flag " << (int)segmentTypeFlags;
        return false;
    }
#if defined(USE_SDNV_FAST) && defined(SDNV_SUPPORT_AVX2_FUNCTIONS)
    uint64_t decodedValues[2];
    const unsigned int numSdnvsToDecode = 2u - isSenderToReceiver;
    uint8_t totalBytesDecoded;
    unsigned int numValsDecodedThisIteration = SdnvDecodeMultiple256BitU64Fast(&data[1], &totalBytesDecoded, decodedValues, numSdnvsToDecode);
    if (numValsDecodedThisIteration != numSdnvsToDecode) { //all required sdnvs were not decoded, possibly due to a decode error
        LOG_ERROR(subprocess) << "LtpIpcEngine::VerifyIpcPacketReceive(): cannot read 1 or more of sessionOriginatorEngineId or sessionNumber.. ignoring packet";
        return false;
    }
    const uint64_t& sessionOriginatorEngineId = decodedValues[0];
    const uint64_t& sessionNumber = decodedValues[1];
#else
    uint8_t sdnvSize;
    const uint64_t sessionOriginatorEngineId = SdnvDecodeU64(&data[1], &sdnvSize, (100 - 1)); //no worries about hardware accelerated sdnv read out of bounds due to minimum 100 byte size
    if (sdnvSize == 0) {
        LOG_ERROR(subprocess) << "LtpIpcEngine::VerifyIpcPacketReceive(): cannot read sessionOriginatorEngineId.. ignoring packet";
        return false;
    }
    uint64_t sessionNumber;
    if (!isSenderToReceiver) {
        sessionNumber = SdnvDecodeU64(&data[1 + sdnvSize], &sdnvSize, ((100 - 10) - 1)); //no worries about hardware accelerated sdnv read out of bounds due to minimum 100 byte size
        if (sdnvSize == 0) {
            LOG_ERROR(subprocess) << "LtpIpcEngine::VerifyIpcPacketReceive(): cannot read sessionNumber.. ignoring packet";
            return false;
        }
    }
#endif

    if (isSenderToReceiver) { //received an isSenderToReceiver message type => isInduct (this ltp engine received a message type that only travels from an outduct (sender) to an induct (receiver))
        //sessionOriginatorEngineId is the remote engine id in the case of an induct
        if(sessionOriginatorEngineId != M_REMOTE_ENGINE_ID) {
            LOG_ERROR(subprocess) << "LtpIpcEngine::VerifyIpcPacketReceive: an induct received packet with unknown remote engine Id "
                << sessionOriginatorEngineId << ".. ignoring packet";
            return false;
        }
            
    }
    else { //received an isReceiverToSender message type => isOutduct (this ltp engine received a message type that only travels from an induct (receiver) to an outduct (sender))
        //sessionOriginatorEngineId is my engine id in the case of an outduct.. need to get the session number to find the proper LtpUdpEngine
        const uint8_t engineIndex = LtpRandomNumberGenerator::GetEngineIndexFromRandomSessionNumber(sessionNumber);
        if (engineIndex != ENGINE_INDEX) {
            LOG_ERROR(subprocess) << "LtpIpcEngine::VerifyIpcPacketReceive: an outduct received packet of type " << (int)segmentTypeFlags << " with unknown session number "
                << sessionNumber << ".. ignoring packet";
            return false;
        }
    }
    return true;
}



void LtpIpcEngine::DoSendPacket(const std::vector<boost::asio::const_buffer>& constBufferVec) {
#if 0
    const bool waitSuccess = m_myTxIpcControlPtr->m_waitUntilNotFull_postHasFreeSpace_semaphore.try_wait();
    if (waitSuccess) {
        LOG_DEBUG(subprocess) << "DSP success";
    }
    else {
        m_myTxIpcControlPtr->m_waitUntilNotFull_postHasFreeSpace_semaphore.wait();
        LOG_DEBUG(subprocess) << "DSP wasblocked";
    }
#else
    m_myTxIpcControlPtr->m_waitUntilNotFull_postHasFreeSpace_semaphore.wait();
#endif
    //check that the destructor did not post by checking the m_running flag
    if (m_running.load(std::memory_order_acquire)) { //success not full, write to my tx
        const unsigned int writeIndex = m_myTxIpcControlPtr->m_circularIndexBuffer.GetIndexForWrite(); //store the volatile
        if (writeIndex == CIRCULAR_INDEX_BUFFER_FULL) {
            LOG_FATAL(subprocess) << "LtpIpcEngine::SendPacket(): CIRCULAR BUFFER SHOULD NEVER HAVE A FULL CONDITION!";
            return;
        }

        IpcPacket& p = m_myTxIpcPacketCbArray[writeIndex];
        //if (writeIndex != p.dataIndex) {
        //    LOG_FATAL(subprocess) << "LtpIpcEngine::SendPacket(): writeIndex != p.dataIndex!";
        //    return;
        //}
        //uint8_t* dataWrite = &m_myTxIpcDataStart[p.dataIndex * m_myTxIpcControlPtr->m_bytesPerElement];
        uint8_t* dataWrite = &m_myTxIpcDataStart[writeIndex * m_myTxIpcControlPtr->m_bytesPerElement];
        p.bytesTransferred = 0;
        for (std::size_t i = 0; i < constBufferVec.size(); ++i) {
            const boost::asio::const_buffer& b = constBufferVec[i];
            p.bytesTransferred += static_cast<unsigned int>(b.size());
            if (p.bytesTransferred > m_myTxIpcControlPtr->m_bytesPerElement) {
                LOG_FATAL(subprocess) << "LtpIpcEngine::SendPacket(): bytesWritten exceeds bytesPerElement!";
                return;
            }
            memcpy(dataWrite, b.data(), b.size());
            dataWrite += b.size();
        }
        m_myTxIpcControlPtr->m_circularIndexBuffer.CommitWrite();
        m_myTxIpcControlPtr->m_waitUntilNotEmpty_postHasData_semaphore.post();
    }
}

void LtpIpcEngine::SendPacket(
    const std::vector<boost::asio::const_buffer> & constBufferVec,
    std::shared_ptr<std::vector<std::vector<uint8_t> > >&& underlyingDataToDeleteOnSentCallback,
    std::shared_ptr<LtpClientServiceDataToSend>&& underlyingCsDataToDeleteOnSentCallback)
{
    //called by LtpEngine Thread
    m_countAsyncSendCalls.fetch_add(1, std::memory_order_relaxed);
    
    DoSendPacket(constBufferVec);

    //mimic std::move of shared_ptr's from LtpUdpEngine
    underlyingDataToDeleteOnSentCallback.reset();
    underlyingCsDataToDeleteOnSentCallback.reset();

    //HandleUdpSend(std::shared_ptr<std::vector<std::vector<uint8_t> > >& underlyingDataToDeleteOnSentCallback,
    m_countAsyncSendCallbackCalls.fetch_add(1, std::memory_order_relaxed);

    //notify the ltp engine regardless whether or not there is an error
    //NEW BEHAVIOR (always notify LtpEngine which keeps its own internal count of pending Udp Send system calls queued)
    OnSendPacketsSystemCallCompleted_ThreadSafe(); //per system call operation, not per udp packet(s)
}

void LtpIpcEngine::SendPackets(std::shared_ptr<std::vector<UdpSendPacketInfo> >&& udpSendPacketInfoVecSharedPtr, const std::size_t numPacketsToSend)
{
    //called by LtpEngine Thread
    m_countBatchSendCalls.fetch_add(1, std::memory_order_relaxed);

    std::vector<UdpSendPacketInfo>& udpSendPacketInfoVec = *udpSendPacketInfoVecSharedPtr;
    //for loop should not compare to udpSendPacketInfoVec.size() because that vector may be resized for preallocation,
    // and don't want to everudpSendPacketInfoVec.resize() down because that would call destructor on preallocated UdpSendPacketInfo
    for (std::size_t i = 0; i < numPacketsToSend; ++i) {
        DoSendPacket(udpSendPacketInfoVec[i].constBufferVec);
    }

    //OnSentPacketsCallback
    m_countBatchSendCallbackCalls.fetch_add(1, std::memory_order_relaxed);
    m_countBatchUdpPacketsSent.fetch_add(numPacketsToSend, std::memory_order_relaxed);

    //mimic std::move of shared_ptr from LtpUdpEngine
    udpSendPacketInfoVecSharedPtr.reset();
    
    //notify the ltp engine regardless whether or not there is an error
    //NEW BEHAVIOR (always notify LtpEngine which keeps its own internal count of pending Udp Send system calls queued)
    OnSendPacketsSystemCallCompleted_ThreadSafe(); //per system call operation, not per udp packet(s)
}






bool LtpIpcEngine::ReadyToSend() const noexcept {
    return m_running.load(std::memory_order_acquire) && m_remoteTxIpcDataStart;
}

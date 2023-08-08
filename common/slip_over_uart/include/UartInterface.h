/**
 * @file UartInterface.h
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
 *
 * @section DESCRIPTION
 *
 * This UartInterface class encapsulates the appropriate bidirectional SLIP over UART functionality
 * to send and/or receive bundles (or any other user defined data) over a SLIP over UART link.
 */

#ifndef UART_INTERFACE_H
#define UART_INTERFACE_H 1

#include <iostream>
#include <string>
#include <vector>
#include <memory>
#include <boost/asio.hpp>
#include <boost/thread.hpp>
#include "PaddedVectorUint8.h"
#include "slip.h"
#include "CircularIndexBufferSingleProducerSingleConsumerConfigurable.h"
#include "TelemetryDefinitions.h"
#include "BundleCallbackFunctionDefines.h"
#include <atomic>
#include "slip_over_uart_lib_export.h"

struct SerialSendElement {
    std::vector<uint8_t> m_userData;
    padded_vector_uint8_t m_slipEncodedBundle;
    padded_vector_uint8_t m_underlyingDataVecBundle;
    zmq::message_t m_underlyingDataZmqBundle;
};

class UartInterface {
public:
    typedef boost::function<void(padded_vector_uint8_t& wholeBundleVec)> WholeBundleReadyCallback_t;

    SLIP_OVER_UART_LIB_EXPORT UartInterface(const std::string & comPortName, const unsigned int baudRate,
        const unsigned int numRxCircularBufferVectors,
        const std::size_t maxRxBundleSizeBytes,
        const unsigned int maxTxBundlesInFlight,
        const WholeBundleReadyCallback_t& wholeBundleReadyCallback);
    SLIP_OVER_UART_LIB_EXPORT ~UartInterface();
    SLIP_OVER_UART_LIB_EXPORT void Stop();
    SLIP_OVER_UART_LIB_EXPORT bool IsRunningNormally();
    SLIP_OVER_UART_LIB_EXPORT boost::asio::io_service& GetIoServiceRef();
    
    SLIP_OVER_UART_LIB_EXPORT bool Forward(const uint8_t* bundleData, const std::size_t size, std::vector<uint8_t>&& userData);
    SLIP_OVER_UART_LIB_EXPORT bool Forward(zmq::message_t& dataZmq, std::vector<uint8_t>&& userData);
    SLIP_OVER_UART_LIB_EXPORT bool Forward(padded_vector_uint8_t& dataVec, std::vector<uint8_t>&& userData);

    SLIP_OVER_UART_LIB_EXPORT bool ReadyToForward();
    SLIP_OVER_UART_LIB_EXPORT void SetOnFailedBundleVecSendCallback(const OnFailedBundleVecSendCallback_t& callback);
    SLIP_OVER_UART_LIB_EXPORT void SetOnFailedBundleZmqSendCallback(const OnFailedBundleZmqSendCallback_t& callback);
    SLIP_OVER_UART_LIB_EXPORT void SetOnSuccessfulBundleSendCallback(const OnSuccessfulBundleSendCallback_t& callback);
    SLIP_OVER_UART_LIB_EXPORT void SetOnOutductLinkStatusChangedCallback(const OnOutductLinkStatusChangedCallback_t& callback);
    SLIP_OVER_UART_LIB_EXPORT void SetUserAssignedUuid(uint64_t userAssignedUuid);
    SLIP_OVER_UART_LIB_EXPORT void SyncTelemetry();

    SLIP_OVER_UART_LIB_EXPORT std::size_t GetTotalDataSegmentsAcked();
    SLIP_OVER_UART_LIB_EXPORT std::size_t GetTotalDataSegmentsSent();
    SLIP_OVER_UART_LIB_EXPORT std::size_t GetTotalDataSegmentsUnacked();
    SLIP_OVER_UART_LIB_EXPORT std::size_t GetTotalBundleBytesAcked();
    SLIP_OVER_UART_LIB_EXPORT std::size_t GetTotalBundleBytesSent();
    SLIP_OVER_UART_LIB_EXPORT std::size_t GetTotalBundleBytesUnacked();
private:
    
    SLIP_OVER_UART_LIB_NO_EXPORT void TryStartSerialReceive();
    SLIP_OVER_UART_LIB_NO_EXPORT void PopCbThreadFunc();
    SLIP_OVER_UART_LIB_NO_EXPORT void ResetRxStates();
    SLIP_OVER_UART_LIB_NO_EXPORT void TrySendBundleIfAvailable_NotThreadSafe();
    SLIP_OVER_UART_LIB_NO_EXPORT void TrySendBundleIfAvailable_ThreadSafe();
    SLIP_OVER_UART_LIB_NO_EXPORT void SerialReceiveSomeHandler(const boost::system::error_code & error, std::size_t bytesTransferred);
    SLIP_OVER_UART_LIB_NO_EXPORT void HandleSerialSend(const boost::system::error_code& error, std::size_t bytes_transferred, const unsigned int consumeIndex);
    SLIP_OVER_UART_LIB_NO_EXPORT void DoFailedBundleCallback(SerialSendElement& el);
    SLIP_OVER_UART_LIB_NO_EXPORT void EmptySendQueueOnFailure();

protected:
    
    
private:
    volatile bool m_useLocalConditionVariableAckReceived;
    volatile bool m_running;
    bool m_runningNormally;
    bool m_rxBundleOverran;
    bool m_stateSerialReadActive;
    bool m_writeInProgress;
    bool m_sendErrorOccurred;
    SlipDecodeState_t m_slipDecodeState;
    const unsigned int M_NUM_RX_CIRCULAR_BUFFER_VECTORS;
    const std::string m_comPortName;
    const std::size_t m_maxRxBundleSizeBytes;
    boost::asio::io_service m_ioService;
    boost::asio::serial_port m_serialPort; // the serial port this instance is connected to 
    std::vector<uint8_t> m_readSomeBuffer;
    padded_vector_uint8_t* m_currentRxBundlePtr;
    CircularIndexBufferSingleProducerSingleConsumerConfigurable m_circularIndexBuffer;
    std::vector<padded_vector_uint8_t > m_bundleRxBuffersCbVec;
    boost::condition_variable m_conditionVariableCb;
    boost::mutex m_mutexCb;
    std::unique_ptr<boost::thread> m_threadCbReaderPtr;
    std::unique_ptr<boost::thread> m_ioServiceThreadPtr;
    boost::condition_variable m_localConditionVariableAckReceived;

    const unsigned int MAX_TX_BUNDLES_IN_FLIGHT;
    CircularIndexBufferSingleProducerSingleConsumerConfigurable m_txBundlesCb;
    std::vector<SerialSendElement> m_txBundlesCbVec;

    const WholeBundleReadyCallback_t m_wholeBundleReadyCallback;

    OnFailedBundleVecSendCallback_t m_onFailedBundleVecSendCallback;
    OnFailedBundleZmqSendCallback_t m_onFailedBundleZmqSendCallback;
    OnSuccessfulBundleSendCallback_t m_onSuccessfulBundleSendCallback;
    OnOutductLinkStatusChangedCallback_t m_onOutductLinkStatusChangedCallback;
    uint64_t m_userAssignedUuid;
    
    //translate to telem
    std::atomic<uint64_t> m_totalBundlesReceived;
    std::atomic<uint64_t> m_totalBundleBytesReceived;
    std::atomic<uint64_t> m_totalBundlesSent;
    std::atomic<uint64_t> m_totalBundleBytesSent;
    std::atomic<uint64_t> m_totalBundlesAcked;
    std::atomic<uint64_t> m_totalBundleBytesAcked;
    std::atomic<uint64_t> m_totalBundlesFailedToSend;
    std::atomic<uint64_t> m_totalSlipBytesSent;
    std::atomic<uint64_t> m_totalSlipBytesReceived;
    std::atomic<uint64_t> m_totalReceivedChunks;
    std::atomic<uint64_t> m_largestReceivedBytesPerChunk;
public:
    SlipOverUartInductConnectionTelemetry_t m_inductTelemetry;
    SlipOverUartOutductTelemetry_t m_outductTelemetry; //volatile => used by 2 threads
};


#endif

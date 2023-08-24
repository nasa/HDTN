/**
 * @file LtpEngineConfig.h
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
 * This LtpEngineConfig struct is a plain old data (POD) C++ struct that is
 * used to configure an LTP over UDP Engine and is passed to
 * functions like LtpUdpEngineManager::AddLtpUdpEngine()
 */

#ifndef _LTP_ENGINE_CONFIG_H
#define _LTP_ENGINE_CONFIG_H 1

#include <cstdint>
#include <string>
#include <boost/date_time.hpp>
#include <boost/filesystem.hpp>

struct LtpEngineConfig {

    /**
     * This LTP engine's engine ID.
     */
    uint64_t thisEngineId = 0;

    /**
     * The LTP remote engine ID.
     */
    uint64_t remoteEngineId = 0;

    /**
     * The client service ID number identifies the upper-level service to
     * which the segment is to be delivered by the receiver.  It is
     * functionally analogous to a TCP port number.  If multiple
     * instances of the client service are present at the destination,
     * multiplexing must be done by the client service itself on the
     * basis of information encoded within the transmitted block.
     * In the HDTN implementation, this value is not checked by the receiver,
     * and the receiver accepts all ids.
     * For a sender, this value is used in the Transmission Request.
     */
    uint64_t clientServiceId;

    /**
     * True if this Engine will be an LTP receiver.  False if it will be an LTP transmitter.
     */
    bool isInduct = false;

    /**
     * The max size of the data portion (excluding LTP headers and UDP headers and IP headers)
     * of an LTP sender's Red data segment being sent.
     * Set this low enough to avoid exceeding ethernet MTU to avoid IP fragmentation.
     */
    uint64_t mtuClientServiceData = 1360;

    /**
     * The max size of the data portion (excluding LTP headers and UDP headers and IP headers)
     * of an LTP receiver's report segment being sent.
     * Set this low enough to avoid exceeding ethernet MTU to avoid IP fragmentation.
     */
    uint64_t mtuReportSegment = 1360;

    /**
     * The one way light time.  Round trip time (retransmission time) is
     * computed by (2 * (oneWayLightTime + oneWayMarginTime)).
     */
    boost::posix_time::time_duration oneWayLightTime = boost::posix_time::milliseconds(1000);

    /**
     * The one way margin (packet processing) time.  Round trip time (retransmission time) is
     * computed by (2 * (oneWayLightTime + oneWayMarginTime)).
     */
    boost::posix_time::time_duration oneWayMarginTime = boost::posix_time::milliseconds(200);;

    /**
     * The remote IP address or hostname of the sender or receiver.
     */
    std::string remoteHostname = "localhost";

    /**
     * The remote UDP port of the sender or receiver.
     */
    uint16_t remotePort = 1113;

    /**
     * The port to bind this engine's UDP socket to.
     */
    uint16_t myBoundUdpPort = 1113;

    /**
     * When LTP is run in "ltp_over_encap_local_stream", this is the
     * socket or pipe name, and remoteHostname, remotePort, and myBoundUdpPort are ignored.
     * On Windows, this is acomplished using a full-duplex named pipe in the form of
     * "\\\\.\\pipe\\mynamedpipe" (including c string escape characters) or
     * \\.\pipe\mynamedpipe (without c string escape characters).
     * On Linux, this is accomplished using a local "AF_UNIX" duplex socket,
     * usually in the form of "/tmp/my_ltp_local_socket"
     */
    std::string encapLocalSocketOrPipePath = "/tmp/ltp_local_socket";

    /**
     * The max number of unprocessed LTP received UDP packets to buffer.
     * If this buffer fills up, received UDP packets will be dropped.
     */
    unsigned int numUdpRxCircularBufferVectors = 1000;

    /**
     * The number of Red data contiguous bytes to initialized on a receiver.
     * Make this large enough to accomidate the max Red data size so that
     * the Ltp receiver doesn't have to reallocate/copy/delete data while it is receiving Red data.
     * Make this small enough so that the system doesn't have to allocate too much extra memory per receiving session.
     * (e.g. if set to 1000000 (1MB), then bundle size (sum of all bundle blocks) received should be less than this value
     */
    uint64_t estimatedBytesToReceivePerSession = 1000000;

    /**
     * A protection to prevent an LTP Red data segment with a huge memory offset from crashing the system.
     * Set this to the worst case largest Red data size for an LTP session.
     */
    uint64_t maxRedRxBytesPerSession = 10000000;

    /**
     * Enables accelerated retransmission for an LTP sender by making every Nth UDP packet a checkpoint (0 disables).
     */
    uint32_t checkpointEveryNthDataPacketSender = 0;

    /**
     * The max number of retries/resends of a single LTP packet with a serial number before the session is terminated.
     */
    uint32_t maxRetriesPerSerialNumber = 5;

    /**
     * True will constrain LTP's headers containining SDNV random numbers to be CCSDS/ION compliant 32-bit values.
     * False will allow LTP to generate 10-byte SDNV (64-bit values) random numbers (compatible with HDTN and DTNME).
     */
    bool force32BitRandomNumbers = false;

    /**
     * Rate limiting UDP send rate in bits per second.
     * A zero value will send UDP packets as fast as the operating system will allow.
     */
    uint64_t maxSendRateBitsPerSecOrZeroToDisable = 0;

    /**
     * The number of expected simultaneous LTP sessions for this engine (important to Ltp receivers),
     * used to initialize hash maps' bucket size for SessionNumberToSessionSender and SessionIdToSessionReceiver.
     */
    uint64_t maxSimultaneousSessions = 5000;

    /**
     * The number of recent Ltp receiver history of session numbers to remember.
     * If an Ltp receiver's session has been closed and it receives a session number that's within the history, the receiver will refuse the session to prevent
     * a potentially old session from being reopened, which has been known to happen with IP fragmentation enabled.
     */
    uint64_t rxDataSegmentSessionNumberRecreationPreventerHistorySizeOrZeroToDisable = 0;

    /**
     * The max number of udp packets to send per system call.
     * If 1 is used, then the receiving udp socket is used to send udp packets from the specified bound port that it is on
     * and one boost::asio:::async_send_to is called per one udp packet to send.
     * If more than 1 is used, a dedicated sender udp socket is created and bound to a random ephemeral port,
     * the socket is then permanently "UDP connected" to the remoteHostname:remotePort,
     * and packets will be sent using this socket's sendmmsg on POSIX or LPFN_TRANSMITPACKETS on Windows.
     */
    uint64_t maxUdpPacketsToSendPerSystemCall = 1;

    /**
     * The number of seconds between ltp session sender pings during times of zero data segment activity.
     * An LTP ping is defined as a sender sending a cancel segment of a known non-existent session number to a receiver,
     * in which the receiver shall respond with a cancel ack in order to determine if the link is active.
     * A link down callback will be called if a cancel ack is not received after (RTT * maxRetriesPerSerialNumber).
     * This parameter should be set to zero for a receiver as there is currently no use case for a receiver to detect link-up.
     */
    uint64_t senderPingSecondsOrZeroToDisable = 0;

    /**
     * The number of milliseconds the ltp engine
     * should wait for gaps to be filled.
     * When red part data is segmented and delivered to the receiving engine out-of-order,
     * the checkpoint(s) and EORP can be received before the earlier-in-block data segments.
     * If a synchronous report is sent immediately upon receiving the checkpoint there will be
     * data segments in-flight and about to be delivered that will be seen as reception gaps in the report.
     * Instead of sending the synchronous report immediately upon receiving a checkpoint segment
     * the receiving engine should wait this period of time before sending the report segment.
     * The delay time will reset upon any data segments which fill gaps.
     * This parameter should be set to zero for a sender.
     */
    uint64_t delaySendingOfReportSegmentsTimeMsOrZeroToDisable = 20;

    /**
     * The number of milliseconds the ltp engine
     * should wait after receiving a report segment before resending data segments.
     * This parameter should be set to zero for a receiver.
     */
    uint64_t delaySendingOfDataSegmentsTimeMsOrZeroToDisable = 20;

    /**
     * If non-zero, makes LTP keep session data on disk instead of in memory,
     * which is useful for high rate data with extremely long delays.
     * This value is the number of milliseconds the ltp engine
     * should create a new file for storing new LTP session data for
     * this period of time.  Once all sessions contained in a file are closed,
     * the file is automatically deleted.  Files are stored in
     * "activeSessionDataOnDiskDirectory/randomly_generated_directory/ltp_%09d.bin"
     * If zero, makes LTP keep session data in memory (default behavior).
     * If enabled for senders, data will be written to disk first on a transmission request and
     * then after the disk write is complete, the session will be created and data segments will
     * read their data from disk as needed.
     * If enabled for receivers, red data segments will be written to disk, and once
     * all data is present, the whole red data will be read into memory and
     * will call the red part reception callback before the memory is
     * destroyed (i.e. destroyed if the memory wasn't std::move'd within the red part reception callback).
     */
    uint64_t activeSessionDataOnDiskNewFileDurationMsOrZeroToDisable = 0;

    /**
     * If and only if activeSessionDataOnDiskNewFileDurationMsOrZeroToDisable is non-zero,
     * then this is the base directory for which LTP keeps session data on disk instead of in memory,
     * which is useful for high rate data with extremely long delays.
     * This path should point to a directory that is mounted on a solid state drive.
     */
    boost::filesystem::path activeSessionDataOnDiskDirectory = "./";

    /**
     * The window of time for averaging the rate over. This limits the allowed
     * burst rate. 
     */
    uint64_t rateLimitPrecisionMicroSec = 0;
};

    


#endif //_LTP_ENGINE_CONFIG_H

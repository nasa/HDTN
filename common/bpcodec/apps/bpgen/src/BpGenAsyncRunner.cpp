#include "BpGenAsyncRunner.h"
#include <iostream>
#include "SignalHandler.h"
#include <boost/filesystem.hpp>
#include <boost/program_options.hpp>
#include <boost/lexical_cast.hpp>


void BpGenAsyncRunner::MonitorExitKeypressThreadFunction() {
    std::cout << "Keyboard Interrupt.. exiting\n";
    m_runningFromSigHandler = false; //do this first
}



static void DurationEndedThreadFunction(const boost::system::error_code& e, volatile bool * running) {
    if (e != boost::asio::error::operation_aborted) {
        // Timer was not cancelled, take necessary action.
        std::cout << "BpGen reached duration.. exiting\n";
    }
    else {
        std::cout << "Unknown error occurred in DurationEndedThreadFunction " << e.message() << std::endl;
    }
    *running = false;
}

BpGenAsyncRunner::BpGenAsyncRunner() {}
BpGenAsyncRunner::~BpGenAsyncRunner() {}


bool BpGenAsyncRunner::Run(int argc, const char* const argv[], volatile bool & running, bool useSignalHandler) {
    //scope to ensure clean exit before return 0
    {
        running = true;
        m_runningFromSigHandler = true;
        SignalHandler sigHandler(boost::bind(&BpGenAsyncRunner::MonitorExitKeypressThreadFunction, this));
        std::string destinationAddress;
        std::string thisLocalEidString;
        uint16_t port;
        bool useTcpcl = false;
        bool useStcp = false;
        bool useLtp = false;
        uint32_t bundleSizeBytes;
        uint32_t bundleRate;
        uint32_t tcpclFragmentSize;
        uint32_t durationSeconds;
        uint64_t flowId;
        uint64_t stcpRateBitsPerSec;
        //ltp
        uint64_t thisLtpEngineId;
        uint64_t remoteLtpEngineId;
        uint64_t ltpDataSegmentMtu;
        uint64_t oneWayLightTimeMs;
        uint64_t oneWayMarginTimeMs;
        uint64_t clientServiceId;
        unsigned int numLtpUdpRxPacketsCircularBufferSize;
        unsigned int maxLtpRxUdpPacketSizeBytes;

        boost::program_options::options_description desc("Allowed options");
        try {
                desc.add_options()
                        ("help", "Produce help message.")
                        ("dest", boost::program_options::value<std::string>()->default_value("localhost"), "Send bundles to this TCP or UDP destination address.")
                        ("port", boost::program_options::value<boost::uint16_t>()->default_value(4556), "Send bundles to this TCP or UDP destination port.")
                        ("bundle-size", boost::program_options::value<boost::uint32_t>()->default_value(100), "Bundle size bytes.")
                        ("bundle-rate", boost::program_options::value<boost::uint32_t>()->default_value(1500), "Bundle rate. (0=>as fast as possible)")
                        ("duration", boost::program_options::value<boost::uint32_t>()->default_value(0), "Seconds to send bundles for (0=>infinity).")
                        ("use-stcp", "Use STCP instead of UDP.")
                        ("use-tcpcl", "Use TCP Convergence Layer Version 3 instead of UDP.")
                        ("use-ltp", "Use LTP Convergence Layer Version 0 instead of UDP.")
                        ("tcpcl-fragment-size", boost::program_options::value<boost::uint32_t>()->default_value(0), "Max fragment size bytes of Tcpcl bundles (0->disabled).")
                        ("tcpcl-eid", boost::program_options::value<std::string>()->default_value("BpGen"), "Local EID string for this program.")
                        ("flow-id", boost::program_options::value<uint64_t>()->default_value(2), "Destination flow id.")
                        ("stcp-rate-bits-per-sec", boost::program_options::value<uint64_t>()->default_value(500000), "Desired link rate for STCP (default 0.5Mbit/sec.")

                        ("this-ltp-engine-id", boost::program_options::value<uint64_t>()->default_value(2), "My LTP engine ID.")
                        ("remote-ltp-engine-id", boost::program_options::value<uint64_t>()->default_value(2), "Remote LTP engine ID.")
                        ("ltp-data-segment-mtu", boost::program_options::value<uint64_t>()->default_value(1), "Max payload size (bytes) of sender's LTP data segment")
                        ("num-ltp-rx-udp-packets-buffer-size", boost::program_options::value<unsigned int>()->default_value(100), "UDP max packets to receive (circular buffer size)")
                        ("max-ltp-rx-udp-packet-size-bytes", boost::program_options::value<unsigned int>()->default_value(UINT16_MAX), "Maximum size (bytes) of a UDP packet to receive (65KB safest option)")
                        ("one-way-light-time-ms", boost::program_options::value<uint64_t>()->default_value(1), "One way light time in milliseconds")
                        ("one-way-margin-time-ms", boost::program_options::value<uint64_t>()->default_value(1), "One way light time in milliseconds")
                        ("client-service-id", boost::program_options::value<uint64_t>()->default_value(2), "LTP Client Service ID.")
                        ;

                boost::program_options::variables_map vm;
                boost::program_options::store(boost::program_options::parse_command_line(argc, argv, desc, boost::program_options::command_line_style::unix_style | boost::program_options::command_line_style::case_insensitive), vm);
                boost::program_options::notify(vm);

                if (vm.count("help")) {
                        std::cout << desc << "\n";
                        return false;
                }

                if (vm.count("use-tcpcl")) {
                        useTcpcl = true;
                }
                if (vm.count("use-stcp")) {
                    useStcp = true;
                }
                if (vm.count("use-ltp")) {
                    useLtp = true;
                }
                if ((useTcpcl + useStcp + useLtp) > 1) {
                    std::cerr << "ERROR: cannot use more than 1 of tcpcl, stcp, or ltp" << std::endl;
                    return false;
                }
                destinationAddress = vm["dest"].as<std::string>();
                port = vm["port"].as<boost::uint16_t>();
                bundleSizeBytes = vm["bundle-size"].as<boost::uint32_t>();
                bundleRate = vm["bundle-rate"].as<boost::uint32_t>();
                tcpclFragmentSize = vm["tcpcl-fragment-size"].as<boost::uint32_t>();
                durationSeconds = vm["duration"].as<boost::uint32_t>();
                thisLocalEidString = vm["tcpcl-eid"].as<std::string>();
                flowId = vm["flow-id"].as<uint64_t>();
                stcpRateBitsPerSec = vm["stcp-rate-bits-per-sec"].as<uint64_t>();
                //ltp
                thisLtpEngineId = vm["this-ltp-engine-id"].as<uint64_t>();
                remoteLtpEngineId = vm["remote-ltp-engine-id"].as<uint64_t>();
                ltpDataSegmentMtu = vm["ltp-data-segment-mtu"].as<uint64_t>();
                oneWayLightTimeMs = vm["one-way-light-time-ms"].as<uint64_t>();
                oneWayMarginTimeMs = vm["one-way-margin-time-ms"].as<uint64_t>();
                clientServiceId = vm["client-service-id"].as<uint64_t>();
                numLtpUdpRxPacketsCircularBufferSize = vm["num-ltp-rx-udp-packets-buffer-size"].as<unsigned int>();
                maxLtpRxUdpPacketSizeBytes = vm["max-ltp-rx-udp-packet-size-bytes"].as<unsigned int>();
        }
        catch (boost::bad_any_cast & e) {
                std::cout << "invalid data error: " << e.what() << "\n\n";
                std::cout << desc << "\n";
                return false;
        }
        catch (std::exception& e) {
                std::cerr << "error: " << e.what() << "\n";
                return false;
        }
        catch (...) {
                std::cerr << "Exception of unknown type!\n";
                return false;
        }


        std::cout << "starting BpGenAsync.." << std::endl;

        BpGenAsync bpGen;
        bpGen.Start(destinationAddress, boost::lexical_cast<std::string>(port), useTcpcl, useStcp, useLtp, bundleSizeBytes, bundleRate, tcpclFragmentSize, thisLocalEidString,
            thisLtpEngineId, remoteLtpEngineId, ltpDataSegmentMtu, oneWayLightTimeMs, oneWayMarginTimeMs, clientServiceId,
            numLtpUdpRxPacketsCircularBufferSize, maxLtpRxUdpPacketSizeBytes,
            flowId, stcpRateBitsPerSec);

        boost::asio::io_service ioService;
        boost::asio::deadline_timer deadlineTimer(ioService, boost::posix_time::seconds(durationSeconds));
        if (durationSeconds) {
            deadlineTimer.async_wait(boost::bind(&DurationEndedThreadFunction, boost::asio::placeholders::error, &running));
        }

        if (useSignalHandler) {
            sigHandler.Start(false);
        }
        std::cout << "BpGenAsync up and running" << std::endl;
        while (running && m_runningFromSigHandler) {
            boost::this_thread::sleep(boost::posix_time::millisec(250));
            if (durationSeconds) {
                ioService.poll_one();
            }
            if (useSignalHandler) {
                sigHandler.PollOnce();
            }
        }

       //std::cout << "Msg Count, Bundle Count, Bundle data bytes\n";

        //std::cout << egress.m_messageCount << "," << egress.m_bundleCount << "," << egress.m_bundleData << "\n";


        std::cout<< "BpGenAsyncRunner::Run: exiting cleanly..\n";
        bpGen.Stop();
        m_bundleCount = bpGen.m_bundleCount;
        m_FinalStats = bpGen.m_FinalStats;
    }
    std::cout<< "BpGenAsyncRunner::Run: exited cleanly\n";
    return true;

}

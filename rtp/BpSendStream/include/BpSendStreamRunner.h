#pragma once

#include "BpSendStream.h"
#include <stdint.h>


class BpSendStreamRunner {
public:
    BpSendStreamRunner();
    ~BpSendStreamRunner();

    std::string ReadSdpFile(const boost::filesystem::path sdpFilePath);
    std::string TranslateSdpToBp(std::string sdp, std::string uriCbheNumber, std::string bpEID);
    
    bool Run(int argc, const char* const argv[], volatile bool & running, bool useSignalHandler);
    uint64_t m_bundleCount;
    uint64_t m_totalBundlesAcked;

    OutductFinalStats m_outductFinalStats;


private:
    void MonitorExitKeypressThreadFunction();

    volatile bool m_runningFromSigHandler;
};



// std::string BpSendStreamRunner::TranslateSdpToBp(std::string sdp, std::string uriCbheNumber, std::string bpEID)
// {
//     std::string newSdp;

//     // we do not care about the "o" and "v" fields

//     // replace the "c" field with DTN BP
//     newSdp.append("c=DTN BP IPN:");
//     newSdp.append(uriCbheNumber);
//     newSdp.append("\n");

//     // replace the "m" field with DTN endpoint
//     newSdp.append("m=");
//     if (sdp.find("m=video") != std::string::npos)
//         newSdp.append("video ");
//     if (sdp.find("m=audio") != std::string::npos)
//         newSdp.append("audio ");
//     newSdp.append(bpEID);
//     newSdp.append(" ");

//     // append the rest of the original SDP message
//     size_t rtpLocation = sdp.find("RTP/AVP"); // sdp protocol for RTP
//     if (rtpLocation == std::string::npos) 
//     {
//         LOG_ERROR(subprocess) << "Invalid SDP file";
//         return "ERROR";
//     }

//     std::string sdpSubString = sdp.substr(rtpLocation, UINT64_MAX);
//     newSdp.append(sdpSubString);
    
//     LOG_INFO(subprocess) << "Translated SDP:\n" << newSdp;
//     return newSdp;
// }

// std::string BpSendStreamRunner::ReadSdpFile(const boost::filesystem::path sdpFilePath)
// {
//     std::string sdpfile;

//     while (!boost::filesystem::exists(sdpFilePath))
//     {

//     }
//     // wait for file to be written to
//     boost::this_thread::sleep_for(boost::chrono::milliseconds(10));
//     boost::filesystem::load_string_file(sdpFilePath, sdpfile);
//     return sdpfile;
// }
/**
 * @file HdtnCliRunner.cpp
 * @author Ethan Schweinsberg <ethan.e.schweinsberg@nasa.gov>
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

#include <boost/program_options.hpp>
#include <boost/make_unique.hpp>
#include <boost/filesystem/fstream.hpp>
#include <zmq.hpp>

#include "HdtnCliRunner.h"
#include "Logger.h"
#include "TelemetryDefinitions.h"
#include "HdtnConfig.h"

static constexpr hdtn::Logger::SubProcess subprocess = hdtn::Logger::SubProcess::cli;
static const int ZMQ_RESPONSE_TIMEOUT_MS = 2000;
static const int ZMQ_SOCKET_TIMEOUT_MS = 1000;
static const std::string DEFAULT_HOSTNAME = "127.0.0.1";
static const std::string DEFAULT_PORT = "10305";

HdtnCliRunner::HdtnCliRunner() : m_pollitems() {}

HdtnCliRunner::~HdtnCliRunner() {}

bool HdtnCliRunner::Run(int argc, const char* const argv[]) {
    // First, parse *only* the hostname and port from the command line. These are needed to
    // connect to HDTN and retrieve the HDTN config file, which is then used to configure the rest
    // of the command line options.
    std::string hostname;
    std::string port;
    if (!ParseHostnameAndPort(argc, argv, hostname, port)) {
        return false;
    }

    // Connect to HDTN and get the HDTN config
    if (!ConnectToHdtn(hostname, port)) {
        return false;
    }
    HdtnConfig_ptr config = GetHdtnConfig();
    if (!config) {
        return false;
    }

    // Parse the rest of the command line options and execute them
    boost::program_options::variables_map vm;
    if (!ParseCliOptions(argc, argv, config, vm)) {
        return false;
    }
    if (!ExecuteCliOptions(vm)) {
        return false;
    }

    return true;
}

bool HdtnCliRunner::ParseHostnameAndPort(int argc, const char* const argv[], std::string& hostname, std::string& port) {
    // First, parse the hostname and port which is needed to get the HDTN config file
    boost::program_options::options_description desc("Options");
    try {
        desc.add_options()
            ("hostname", boost::program_options::value<std::string>()->default_value(DEFAULT_HOSTNAME), "HDTN hostname")
            ("port", boost::program_options::value<std::string>()->default_value(DEFAULT_PORT), "HDTN port");

        boost::program_options::variables_map vm;
    
        boost::program_options::parsed_options options = 
            boost::program_options::command_line_parser(argc, argv).options(desc).allow_unregistered().run();
        boost::program_options::store(options, vm); // can throw
        boost::program_options::notify(vm);
        hostname = vm["hostname"].as<std::string>();
        port = vm["port"].as<std::string>();
    }
    catch(const boost::program_options::error& e) {
        LOG_ERROR(subprocess) << "invalid program options: " << e.what();
        return false;
    }
    catch (const boost::bad_any_cast & e) {
            LOG_ERROR(subprocess) << "invalid data error: " << e.what();
            LOG_ERROR(subprocess) << desc;
            return false;
    }
    catch (const boost::bad_lexical_cast & e) {
            LOG_ERROR(subprocess) << "invalid data error: " << e.what();
            LOG_ERROR(subprocess) << desc;
            return false;
    }
    catch (const std::exception& e) {
            LOG_ERROR(subprocess) << "error: " << e.what();
            return false;
    }
    catch (...) {
            LOG_ERROR(subprocess) << "Exception of unknown type!";
            return false;
    }
    return true;
}

std::string HdtnCliRunner::SendRequestToHdtn(const std::string& msg) {
    try {
        zmq::message_t request(msg);
        m_socket->send(request, zmq::send_flags::none);

        // Wait for the reply
        int response = zmq::poll(m_pollitems, 1, ZMQ_RESPONSE_TIMEOUT_MS);
        if (response == ZMQ_POLLIN && m_pollitems[0].revents & ZMQ_POLLIN) {
            zmq::message_t reply;
            zmq::recv_result_t result = m_socket->recv(reply, zmq::recv_flags::dontwait);
            if (!result) {
                LOG_ERROR(subprocess) << "Error receiving reply from HDTN";
                return "";
            }
            return reply.to_string();
        }
        else {
            LOG_ERROR(subprocess) << "Timeout waiting for reply from HDTN";
            return "";
        }
    }
    catch (const zmq::error_t& e) {
        LOG_ERROR(subprocess) << "Error receiving reply from HDTN: " << e.what();
        return "";
    }
}

HdtnConfig_ptr HdtnCliRunner::GetHdtnConfig() {
    // Send the request
    GetHdtnConfigApiCommand_t cmd;
    std::string msg;
    try {
        msg = cmd.ToJson();
    }
    catch (const boost::property_tree::json_parser::json_parser_error & e) {
        LOG_ERROR(subprocess) << "invalid json error: " << e.what();
        return HdtnConfig_ptr(); //null
    }

    std::string reply = SendRequestToHdtn(msg);
    if (reply == "") {
        return HdtnConfig_ptr(); //null
    }

    // Parse the reply
    try {
        return HdtnConfig::CreateFromJson(reply);
    }
    catch (const boost::property_tree::json_parser::json_parser_error & e) {
        LOG_ERROR(subprocess) << "invalid json error: " << e.what();
        return HdtnConfig_ptr(); //null
    }
}

bool HdtnCliRunner::ConnectToHdtn(std::string& hostname, std::string& port) {
    try {
        m_socket = boost::make_unique<zmq::socket_t>(zmq::socket_t(m_context, zmq::socket_type::req));
        m_socket->set(zmq::sockopt::connect_timeout, ZMQ_SOCKET_TIMEOUT_MS);
        m_socket->set(zmq::sockopt::linger, 0);
        m_pollitems[0] = { *m_socket, 0, ZMQ_POLLIN, 0 };
        m_socket->connect("tcp://" + hostname + ":" + port);
    }
    catch (zmq::error_t&) {
        LOG_ERROR(subprocess) << "Could not connect to HDTN; ensure it is running and the hostname and port are set correctly";
        return false;
    }
    return true;
}

bool HdtnCliRunner::ParseCliOptions(
    int argc,
    const char* const argv[],
    HdtnConfig_ptr config,
    boost::program_options::variables_map& vm
) {
    boost::program_options::options_description desc("Options");
    try {
        desc.add_options()
            ("help", "Produce help message")
            ("hostname", boost::program_options::value<std::string>()->default_value(DEFAULT_HOSTNAME), "HDTN hostname")
            ("port", boost::program_options::value<std::string>()->default_value(DEFAULT_PORT), "HDTN port")
            ("contact-plan-file", boost::program_options::value<std::string>(), "Local contact plan file")
            ("contact-plan-json", boost::program_options::value<std::string>(), "Contact plan json string");

        // Depending on the convergence layer, add additional options
        for (std::size_t i = 0; i < config->m_outductsConfig.m_outductElementConfigVector.size(); ++i) {
            if (config->m_outductsConfig.m_outductElementConfigVector[i].convergenceLayer == "ltp_over_udp" ||
                config->m_outductsConfig.m_outductElementConfigVector[i].convergenceLayer == "udp") {
                desc.add_options()
                    (("outduct[" + std::to_string(i) + "].rateBps").c_str(), boost::program_options::value<uint64_t>(), "Outduct rate limit (bits per second)");
            }
        }

        boost::program_options::store(boost::program_options::parse_command_line(argc, argv, desc), vm); // can throw
        boost::program_options::notify(vm);
    }
    catch(const boost::program_options::error& e) {
        LOG_ERROR(subprocess) << "invalid program options: " << e.what();
        return false;
    }
    catch (const boost::bad_any_cast & e) {
        LOG_ERROR(subprocess) << "invalid data error: " << e.what();
        LOG_ERROR(subprocess) << desc;
        return false;
    }
    catch (const boost::bad_lexical_cast & e) {
        LOG_ERROR(subprocess) << "invalid data error: " << e.what();
        LOG_ERROR(subprocess) << desc;
        return false;
    }
    catch (const std::exception& e) {
        LOG_ERROR(subprocess) << "error: " << e.what();
        return false;
    }
    catch (...) {
        LOG_ERROR(subprocess) << "Exception of unknown type!";
        return false;
    }

    if (vm.count("help")) {
        std::cout << desc << std::endl;
    }

    return true;
}

bool HdtnCliRunner::ExecuteCliOptions(boost::program_options::variables_map& vm) {
    // Build a list of commands to send to HDTN
    std::vector<std::string> cmds;
    try {
        for (const boost::program_options::variables_map::value_type& option : vm) {
            if (option.first == "contact-plan-file") {
                // Read contact plan file from disk
                boost::filesystem::path filePath(vm["contact-plan-file"].as<std::string>());
                boost::filesystem::ifstream ifs(filePath);
                std::string content((std::istreambuf_iterator<char>(ifs)),
                                    (std::istreambuf_iterator<char>()));
                ifs.close();

                UploadContactPlanApiCommand_t cmd;
                cmd.m_contactPlanJson = content;
                cmds.push_back(cmd.ToJson());
            }

            if (option.first == "contact-plan-json") {
                UploadContactPlanApiCommand_t cmd;
                cmd.m_contactPlanJson = vm["contact-plan-json"].as<std::string>();
                cmds.push_back(cmd.ToJson());
            }

            if (option.first.find("outduct") == 0 && option.first.find("rateBps") != std::string::npos) {
                // Parse the outduct number and covert it to an integer
                std::size_t start = option.first.find('[') + 1;
                std::size_t end = option.first.find(']');
                std::string indexStr = option.first.substr(start, end - start);
                int index = std::stoi(indexStr);

                SetMaxSendRateApiCommand_t cmd;
                cmd.m_outduct = index;
                cmd.m_rateBitsPerSec = vm[option.first].as<uint64_t>();
                cmds.push_back(cmd.ToJson());
            }
        }
    }
    catch (const boost::bad_any_cast & e) {
        LOG_ERROR(subprocess) << "invalid data error: " << e.what();
        return false;
    }
    catch (const boost::property_tree::json_parser::json_parser_error & e) {
        LOG_ERROR(subprocess) << "invalid json error: " << e.what();
        return false;
    }

    // Send the commands
    bool sent = false;
    for (const std::string& cmd : cmds) {
        std::string reply = SendRequestToHdtn(cmd);
        if (reply == "") {
            return false;
        }
        else {
            sent = true;
        }
    }
    if (sent) {
        LOG_INFO(subprocess) << "Command(s) successfully sent to HDTN";
    }
    return true;
}

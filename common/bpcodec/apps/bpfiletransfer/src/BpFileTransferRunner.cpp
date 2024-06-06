/**
 * @file BpFileTransferRunner.cpp
 * @author  Brian Tomko <brian.j.tomko@nasa.gov>
 *
 * @copyright Copyright (c) 2021 United States Government as represented by
 * the National Aeronautics and Space Administration.
 * No copyright is claimed in the United States under Title 17, U.S.Code.
 * All Other Rights Reserved.
 *
 * @section LICENSE
 * Released under the NASA Open Source Agreement (NOSA)
 * See LICENSE.md in the source root directory for more information.
 */

#include "BpFileTransferRunner.h"
#include "Logger.h"
#include "SignalHandler.h"
#include <boost/filesystem/path.hpp>
#include <boost/program_options.hpp>
#include <boost/lexical_cast.hpp>
#include "Uri.h"
#include "Environment.h"

static constexpr hdtn::Logger::SubProcess subprocess = hdtn::Logger::SubProcess::none;

void BpFileTransferRunner::MonitorExitKeypressThreadFunction() {
    LOG_INFO(subprocess) << "Keyboard Interrupt.. exiting";
    m_runningFromSigHandler = false; //do this first
}

BpFileTransferRunner::BpFileTransferRunner(): m_runningFromSigHandler(false) {}

BpFileTransferRunner::~BpFileTransferRunner() {}


bool BpFileTransferRunner::Run(int argc, const char* const argv[], std::atomic<bool>& running, bool useSignalHandler) {
    //scope to ensure clean exit before return 0
    {
        running = true;
        m_runningFromSigHandler = true;
        SignalHandler sigHandler(boost::bind(&BpFileTransferRunner::MonitorExitKeypressThreadFunction, this));
        uint64_t maxBundleSizeBytes;
        uint64_t maxRxFileSizeBytes;
        boost::filesystem::path fileOrFolderPathToSend;
        boost::filesystem::path saveDirectory;
        cbhe_eid_t myEid;
        cbhe_eid_t finalDestEid;
        uint64_t myCustodianServiceId;
        OutductsConfig_ptr outductsConfigPtr;
        InductsConfig_ptr inductsConfigPtr;
        bool uploadExistingFiles;
        bool uploadNewFiles;
        bool custodyTransferUseAcs;
        bool forceDisableCustody;
        bool useBpVersion7;
        unsigned int bundleSendTimeoutSeconds;
        unsigned int recurseDirectoriesDepth;
        uint64_t bundleLifetimeMilliseconds;
        uint64_t bundlePriority;
        boost::filesystem::path bpSecConfigFilePath;
        uint64_t claRate;
        WebsocketServer::ProgramOptions websocketServerProgramOptions;

        boost::program_options::options_description desc("Allowed options");
        try {
            desc.add_options()
                ("help", "Produce help message.")
                ("max-tx-bundle-size-bytes", boost::program_options::value<uint64_t>()->default_value(4000000), "Max size bundle for file fragments (default 4MB).")
                ("file-or-folder-path-tx", boost::program_options::value<boost::filesystem::path>()->default_value(""), "File or folder paths to send. Folders are recursive.")
                ("my-uri-eid", boost::program_options::value<std::string>()->default_value("ipn:1.1"), "BpGen Source Node Id.")
                ("dest-uri-eid", boost::program_options::value<std::string>()->default_value("ipn:2.1"), "BpGen sends to this final destination Eid.")
                ("my-custodian-service-id", boost::program_options::value<uint64_t>()->default_value(0), "Custodian service ID is always 0.")
                ("outducts-config-file", boost::program_options::value<boost::filesystem::path>()->default_value(""), "Outducts Configuration File.")
                ("custody-transfer-inducts-config-file", boost::program_options::value<boost::filesystem::path>()->default_value(""), "Inducts Configuration File for custody transfer (use custody if present).")
                ("bpsec-config-file", boost::program_options::value<boost::filesystem::path>()->default_value(""), "BpSec Configuration File.")
                ("skip-upload-existing-files", "Do not upload existing files in the directory if and only if file-or-folder-path-tx is a directory.")
                ("upload-new-files", "Upload new files copied or moved into the directory if and only if file-or-folder-path-tx is a directory.")
                ("recurse-directories-depth", boost::program_options::value<unsigned int>()->default_value(3), "Upload all files within max specified depth of subdirectories if file-or-folder-path-tx is a directory (0->no recursion).")
                ("custody-transfer-use-acs", "Custody transfer should use Aggregate Custody Signals instead of RFC5050.")
                ("force-disable-custody", "Custody transfer turned off regardless of link bidirectionality.")
                ("use-bp-version-7", "Send bundles using bundle protocol version 7.")
                ("bundle-send-timeout-seconds", boost::program_options::value<unsigned int>()->default_value(3), "Max time to send a bundle and get acknowledgement.")
                ("bundle-lifetime-milliseconds", boost::program_options::value<uint64_t>()->default_value(1000000), "Bundle lifetime in milliseconds.")
                ("bundle-priority", boost::program_options::value<uint64_t>()->default_value(2), "Bundle priority. 0 = Bulk 1 = Normal 2 = Expedited")
                ("cla-rate", boost::program_options::value<uint64_t>()->default_value(0), "Convergence layer adapter send rate. 0 = unlimited")
                //
                ("save-directory", boost::program_options::value<boost::filesystem::path>()->default_value(""), "Directory to save file(s) to.  Empty=>DoNotSaveToDisk")
                ("max-rx-file-size-bytes", boost::program_options::value<uint64_t>()->default_value(12000000000), "Max bundle size bytes to receive (default=12GB).")
                ;
            boost::filesystem::path defaultWwwRoot = Environment::GetPathHdtnSourceRoot() / "common" / "bpcodec" / "apps" / "bpfiletransfer" / "www";
            WebsocketServer::ProgramOptions::AppendToDesc(desc, &defaultWwwRoot);

            boost::program_options::variables_map vm;
            boost::program_options::store(boost::program_options::parse_command_line(argc, argv, desc, boost::program_options::command_line_style::unix_style | boost::program_options::command_line_style::case_insensitive), vm);
            boost::program_options::notify(vm);

            if (vm.count("help")) {
                    LOG_INFO(subprocess) << desc;
                    return false;
            }
            forceDisableCustody = (vm.count("force-disable-custody") != 0);
            useBpVersion7 = (vm.count("use-bp-version-7") != 0);
            uploadExistingFiles = (vm.count("skip-upload-existing-files") == 0);
            uploadNewFiles = (vm.count("upload-new-files") != 0);

            const std::string myUriEid = vm["my-uri-eid"].as<std::string>();
            if (!Uri::ParseIpnUriString(myUriEid, myEid.nodeId, myEid.serviceId)) {
                LOG_ERROR(subprocess) << "error: bad bpsink uri string: " << myUriEid;
                return false;
            }

            const std::string myFinalDestUriEid = vm["dest-uri-eid"].as<std::string>();
            if (!Uri::ParseIpnUriString(myFinalDestUriEid, finalDestEid.nodeId, finalDestEid.serviceId)) {
                LOG_ERROR(subprocess) << "error: bad bpsink uri string: " << myFinalDestUriEid;
                return false;
            }

            bpSecConfigFilePath = vm["bpsec-config-file"].as<boost::filesystem::path>();

            const boost::filesystem::path outductsConfigFileName = vm["outducts-config-file"].as<boost::filesystem::path>();

            if (!outductsConfigFileName.empty()) {
                outductsConfigPtr = OutductsConfig::CreateFromJsonFilePath(outductsConfigFileName);
                if (!outductsConfigPtr) {
                    LOG_ERROR(subprocess) << "error loading outducts config file: " << outductsConfigFileName;
                    return false;
                }
                std::size_t numBpGenOutducts = outductsConfigPtr->m_outductElementConfigVector.size();
                if (numBpGenOutducts != 1) {
                    LOG_ERROR(subprocess) << "number of bpsendfile outducts is not 1: got " << numBpGenOutducts;
                }
            }
            else {
                LOG_WARNING(subprocess) << "notice: bpsendfile has no outduct... bundle data will have to flow out through a bidirectional tcpcl induct";
            }

            //create induct for custody signals
            const boost::filesystem::path inductsConfigFileName = vm["custody-transfer-inducts-config-file"].as<boost::filesystem::path>();
            if (!inductsConfigFileName.empty()) {
                inductsConfigPtr = InductsConfig::CreateFromJsonFilePath(inductsConfigFileName);
                if (!inductsConfigPtr) {
                    LOG_ERROR(subprocess) << "error loading induct config file: " << inductsConfigFileName;
                    return false;
                }
                std::size_t numBpGenInducts = inductsConfigPtr->m_inductElementConfigVector.size();
                if (numBpGenInducts != 1) {
                    LOG_ERROR(subprocess) << "number of bp gen inducts for custody signals is not 1: got " << numBpGenInducts;
                }
            }
            custodyTransferUseAcs = (vm.count("custody-transfer-use-acs"));

            fileOrFolderPathToSend = vm["file-or-folder-path-tx"].as<boost::filesystem::path>();
            saveDirectory = vm["save-directory"].as<boost::filesystem::path>();
            maxRxFileSizeBytes = vm["max-rx-file-size-bytes"].as<uint64_t>();
            maxBundleSizeBytes = vm["max-tx-bundle-size-bytes"].as<uint64_t>();
            myCustodianServiceId = vm["my-custodian-service-id"].as<uint64_t>();
            bundleSendTimeoutSeconds = vm["bundle-send-timeout-seconds"].as<unsigned int>();
            recurseDirectoriesDepth = vm["recurse-directories-depth"].as<unsigned int>();
            if (!websocketServerProgramOptions.ParseFromVariableMap(vm)) {
                LOG_ERROR(subprocess) << "cannot parse websocketServerProgramOptions";
                return false;
            }

            bundlePriority = vm["bundle-priority"].as<uint64_t>();
            if (bundlePriority > 2) {
                std::cerr << "Priority must be 0, 1, or 2." << std::endl;
                return false;
            }

            bundleLifetimeMilliseconds = vm["bundle-lifetime-milliseconds"].as<uint64_t>();

            claRate = vm["cla-rate"].as<uint64_t>();

        }
        catch (boost::bad_any_cast & e) {
            LOG_ERROR(subprocess) << "invalid data error: " << e.what();
            LOG_ERROR(subprocess) << desc;
            return false;
        }
        catch (std::exception& e) {
            LOG_ERROR(subprocess) << e.what();
            return false;
        }
        catch (...) {
            LOG_ERROR(subprocess) << "Exception of unknown type!";
            return false;
        }


        LOG_INFO(subprocess) << "starting..";

        BpFileTransfer bpFileTransfer(fileOrFolderPathToSend, saveDirectory, maxBundleSizeBytes, uploadExistingFiles, uploadNewFiles, recurseDirectoriesDepth, maxRxFileSizeBytes);
        //if ((bpFileTransfer.GetNumberOfFilesToSend() == 0) && (!uploadNewFiles)) {
        //    LOG_ERROR(subprocess) << "Terminating because there are no files to send and monitoring of new files is disabled!";
        //    return false;
        //}
        bpFileTransfer.Start(
            outductsConfigPtr,
            inductsConfigPtr,
            bpSecConfigFilePath,
            custodyTransferUseAcs,
            myEid,
            0,
            finalDestEid,
            myCustodianServiceId,
            bundleSendTimeoutSeconds,
            bundleLifetimeMilliseconds,
            bundlePriority,
            false,
            forceDisableCustody,
            useBpVersion7,
            claRate,
            websocketServerProgramOptions
        );

        LOG_INFO(subprocess) << "running";
        
        

        if (useSignalHandler) {
            sigHandler.Start(false);
        }
        LOG_INFO(subprocess) << "Up and running";
        while (running && m_runningFromSigHandler) {
            try {
                boost::this_thread::sleep(boost::posix_time::milliseconds(250));
            }
            catch (const boost::thread_resource_error&) {}
            catch (const boost::thread_interrupted&) {}
            catch (const boost::condition_error&) {}
            catch (const boost::lock_error&) {}
            if (useSignalHandler) {
                sigHandler.PollOnce();
            }
        }

        LOG_INFO(subprocess)<< "Exiting cleanly..";
        bpFileTransfer.Stop();
        //m_bundleCount = bpFileTransfer.m_bundleCount;
        //m_outductFinalStats = bpFileTransfer.m_outductFinalStats;
    }
    LOG_INFO(subprocess)<< "Exited cleanly";
    return true;

}

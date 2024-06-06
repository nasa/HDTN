/**
 * @file BpFileTransfer.h
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
 *
 * @section DESCRIPTION
 *
 * The BpFileTransfer class is a child class of BpSourcePattern.  It is an app used for
 * bundling up existing files (or optionally monitoring for new files) within a directory
 * into bundles up to a specified maximum size.
 * The app is also used as the receiver for receiving file-fragment bundles from the BpFileTransfer app
 * and writing those files to disk within a directory and preserving
 * the sender's relative path names.
 * Bundles are sent either at a defined rate or as fast as possible.
 * The app copies a tiny meta-data payload to the beginning of the bundle
 * payload block used for preserving relative path names.
 * The remaining data in the bundle payload block is the file data (or fragment of file data).
 * This app is intended to be used between 2 BpFileTransfer apps.
 * It is acceptable if the bundles arrive to the destination out-of-order.
 */

#ifndef _BP_FILE_TRANSFER_H
#define _BP_FILE_TRANSFER_H 1


#include <cstdint>
#include <memory>
#include <boost/core/noncopyable.hpp>
#include "app_patterns/BpSourcePattern.h"
#include "WebsocketServer.h"


class BpFileTransfer : private boost::noncopyable {
public:
    BpFileTransfer(const boost::filesystem::path& fileOrFolderPathToSend,
        const boost::filesystem::path& saveDirectory,
        uint64_t maxBundleSizeBytes,
        bool uploadExistingFiles,
        bool uploadNewFiles,
        unsigned int recurseDirectoriesDepth,
        const uint64_t maxRxFileSizeBytes);
    ~BpFileTransfer();

    void Stop();
    void Start(OutductsConfig_ptr& outductsConfigPtr, InductsConfig_ptr& inductsConfigPtr,
        const boost::filesystem::path& bpSecConfigFilePath,
        bool custodyTransferUseAcs,
        const cbhe_eid_t& myEid, double bundleRate, const cbhe_eid_t& finalDestEid, const uint64_t myCustodianServiceId,
        const unsigned int bundleSendTimeoutSeconds, const uint64_t bundleLifetimeMilliseconds, const uint64_t bundlePriority,
        const bool requireRxBundleBeforeNextTx, const bool forceDisableCustody, const bool useBpVersion7,
        const uint64_t claRate, const WebsocketServer::ProgramOptions& websocketServerProgramOptions);

private:
    // Internal implementation class
    class Impl;
    // Pointer to the internal implementation
    std::unique_ptr<Impl> m_pimpl;


};


#endif //_BP_FILE_TRANSFER_H

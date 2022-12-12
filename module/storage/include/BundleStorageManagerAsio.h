/**
 * @file BundleStorageManagerAsio.h
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
 * This BundleStorageManagerAsio class inherits from the BundleStorageManagerBase class and implements
 * writing and reading bundles to and from solid state disk drive(s) using 1 thread regardless of number of drives
 * and uses cross-platform asynchronous I/O operations.
 */

#ifndef _BUNDLE_STORAGE_MANAGER_ASIO_H
#define _BUNDLE_STORAGE_MANAGER_ASIO_H 1

#include "BundleStorageManagerBase.h"
#include <boost/asio.hpp>



class CLASS_VISIBILITY_STORAGE_LIB BundleStorageManagerAsio : public BundleStorageManagerBase {
public:
    STORAGE_LIB_EXPORT BundleStorageManagerAsio();
    STORAGE_LIB_EXPORT BundleStorageManagerAsio(const boost::filesystem::path& jsonConfigFilePath);
    STORAGE_LIB_EXPORT BundleStorageManagerAsio(const StorageConfig_ptr & storageConfigPtr);
    STORAGE_LIB_EXPORT virtual ~BundleStorageManagerAsio();
    STORAGE_LIB_EXPORT virtual void Start();


private:
    STORAGE_LIB_NO_EXPORT void TryDiskOperation_Consume_NotThreadSafe(const unsigned int diskId);
    STORAGE_LIB_NO_EXPORT void HandleDiskOperationCompleted(const boost::system::error_code& error, std::size_t bytes_transferred,
        const unsigned int diskId, const unsigned int consumeIndex, const bool wasReadOperation);

    STORAGE_LIB_NO_EXPORT virtual void CommitWriteAndNotifyDiskOfWorkToDo_ThreadSafe(const unsigned int diskId);

private:

    boost::asio::io_service m_ioService;
    std::unique_ptr<boost::asio::io_service::work> m_workPtr;
    std::unique_ptr<boost::thread> m_ioServiceThreadPtr;
    
#ifdef _WIN32
    std::vector<std::unique_ptr<boost::asio::windows::random_access_handle> > m_asioHandlePtrsVec;
#else
    std::vector<std::unique_ptr<boost::asio::posix::stream_descriptor> > m_asioHandlePtrsVec;
#endif

    std::vector<bool> m_diskOperationInProgressVec;
};


#endif //_BUNDLE_STORAGE_MANAGER_ASIO_H

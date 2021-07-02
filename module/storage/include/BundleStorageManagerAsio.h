#ifndef _BUNDLE_STORAGE_MANAGER_ASIO_H
#define _BUNDLE_STORAGE_MANAGER_ASIO_H

#include "BundleStorageManagerBase.h"
#include <boost/asio.hpp>



class BundleStorageManagerAsio : public BundleStorageManagerBase {
public:
    BundleStorageManagerAsio();
    BundleStorageManagerAsio(const std::string & jsonConfigFileName);
    BundleStorageManagerAsio(const StorageConfig_ptr & storageConfigPtr);
    virtual ~BundleStorageManagerAsio();
    virtual void Start(bool autoDeleteFilesOnExit = true);


private:
    void TryDiskOperation_Consume_NotThreadSafe(const unsigned int threadIndex);
    void HandleDiskOperationCompleted(const boost::system::error_code& error, std::size_t bytes_transferred,
        const unsigned int threadIndex, const unsigned int consumeIndex, const bool wasReadOperation);

    virtual void NotifyDiskOfWorkToDo_ThreadSafe(const unsigned int diskId);

private:

    boost::asio::io_service m_ioService;
    std::unique_ptr<boost::asio::io_service::work> m_workPtr;
    std::unique_ptr<boost::thread> m_ioServiceThreadPtr;
    std::vector<boost::filesystem::path> m_filePathsVec;
    std::vector<std::unique_ptr<boost::asio::windows::random_access_handle> > m_asioHandlePtrsVec;
    std::vector<bool> m_diskOperationInProgressVec;

    volatile bool m_autoDeleteFilesOnExit;

};


#endif //_BUNDLE_STORAGE_MANAGER_ASIO_H

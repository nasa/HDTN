# Changelog

All notable changes to HDTN will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Fixed

### Added

### Changed

### Removed

## [1.3.1] - 2024-05-24

### Added

* Added runscripts for H.265 multimedia streaming over HDTN

### Fixed

* Fixed a compilation issue when using a newer version of the Boost library

## [1.3.0] - 2024-05-22

### Fixed

* Fixed a re-route issue related to the API command for updating a link up/down and link state

### Added

* Added support for multimedia streaming over HDTN
* Added Beta Web User Interface which currently includes a tool for generating HDTN configuration files via a web form

## [1.2.1] - 2024-04-26

### Fixed

* Fixed a race condition in egress that caused dropped bundles.  Handle the error where no outduct exists for a bundle in egress.  Egress now returns that bundle to its sender.  This condition is careful not to mark the link down in storage or ingress.
* Fixed missing function calls in the router to recompute routes when using the link up and link down API calls.

### Changed

* Write fatal level errors to a fatal level log file when CMake cache variable `LOG_TO_ERROR_FILE=ON`

## [1.2.0] - 2024-04-05

### Fixed

* Fix BpSourcePattern can now receive non-custody-signal bundles for outduct convergence layers SlipOverUart and BpOverEncap.
* Fix BundleViews v6 and v7 bug when using a mixture of both payload admin records and payload non-admin records.
* Building HDTN as shared libraries now works on all platforms.
* HDTN now cleanly exits when an ltp outduct or induct has a bad hostname.
* Stcp, TcpclV3, and TcpclV4 inducts would cause a "use after free" event when deleting a closed connection.
* `UdpBatchSender` now works on Apple using a `syscall` to `sendmsg_x` (the `sendmmsg` equivalent).
* Windows now builds with warning level 4 (previously level 3) and treats all compiler warnings as errors; fixed all level 4 warnings.
* Linux now builds with all warnings (`-Wall`, `-Wextra`, `-Wpedantic`); fixed all warnings.  Only the CI/CD pipeline enables "Treat warnings as Errors" (`-Werror` option).
* Fix LTP RedPartReceptionCallback where the isEndOfBlock parameter now reports if the EOB came from the red data (correct behavior) instead of whether the last segment received from any part of the red data was the EOB (previous incorrect behavior).
* Fix LTP GreenPartSegmentArrivalCallback where the offsetStartOfBlock parameter incorrectly had the length of the block added to it.
* Fix BPSec decoding mishandles larger parameter and result sizes (caused by the wrong type cast).

### Added

* New supported native platforms (g++ or Clang).
    * macOS Apple Silicon M2 on Ventura (using ARM CPU NEON instructions)
    * macOS Intel x64 on Ventura
    * FreeBSD Intel x64
    * OpenBSD Intel x64
    * Linux ARM64 (aarch64) (using ARM CPU NEON instructions)
* HDTN now prints the Git commit hash (from which it was built) to the logs at initialization (in addition to the HDTN version).
* Added the option to build the web interface with CivetWeb (statically linked without SSL support) instead of Boost Beast (default) in order to reduce HDTN binary file size.  See the README for instructions.
* Added experimental "bp_over_encap_local_stream" and "ltp_over_encap_local_stream" convergence layers, allowing HDTN to generate CCSDS encap packets over a cross-platform local stream.  On Windows, this is acomplished using a full-duplex named pipe.  On Linux/POSIX, this is accomplished using a local `AF_UNIX` duplex socket.
* Added EncapRepeater.cpp demo application to serve as an example for writing code to intercept/extract CCSDS Encap packets from HDTN.
+ Add `enforceBundlePriority` config option; setting this enforces strict priority ordering
  of forwarded bundles. When true, HDTN will foward bundles by priority. This will have 
  a performance inpact, as it requires passing all bundles through storage.

### Changed

* HDTN config now has mandatory boolean field `enforceBundlePriority` (default to false).
* Egress now disables LTP ping during times when the contact plan DOES NOT allow transmission.  Likewise, Egress will reenable LTP ping (back to its config file value) during times when the contact plan allows transmission.
* `UdpBatchSender` no longer has its own thread and `io_service` due to now being completely asynchronous on all platforms; user provides an `io_service` to its constructor.  The `LtpEngine` `io_service` is what runs the `UdpBatchSender` when using LTP over UDP when `ltpMaxUdpPacketsToSendPerSystemCall` config variable is greater than 1.
* Windows now builds with warning level 4 (previously level 3) and treats all compiler warnings and linker warnings as errors.
* Linux now builds with all warnings (`-Wall`, `-Wextra`, `-Wpedantic`).

### Removed

## [1.1.0] - 2023-12-14

### Fixed

* All `volatile` variables, especially `volatile bool`, have been replaced with `std::atomic` with proper memory-ordering semantics.
* All convergence layer telemetry variables/counters use `std::atomic` with proper memory-ordering semantics.
* Routing library now uses reloaded contact plans from the telemetry API.
* Fixed bug where router link state information would become out of sync with
  actual link state.

### Added

* Added "slip_over_uart" convergence layer, allowing bidirectional communication and framing of bundles over a COM/Serial port.
* Added `--cla-rate` command-line argument to bpgen. This argument, defaulting to zero
  for unlimited, can be used to set the rate for LTP and UDP connections.
* Added config option `neighborDepletedStorageDelaySeconds` allowing for optional rerouting
  around neighboring nodes with depleted storage. Zero disables; otherwise, the value is
  interpreted as the amount of time to wait before forwarding to the neighbor after a depleted
  storage message is received. Requires that custody be enabled.
* Added config option `fragmentBundlesLargerThanBytes` allowing for optionally fragmenting
  BPv6 bundles. Setting this option to zero disables BPv6 fragmentation. Otherwise, HDTN will
  attempt to fragment received BPv6 bundles with payloads larger than this value. Bundles
  that cannot be fragmented will be forwareded as usual.
* Added optional config field `rateLimitPrecisionMicroSec`. This new field defines the window of time the UDP rate limit will be guaranteed over, for UDP and LTP convergence layers. If not provided, this field will default to a value of 100000.
* Added support for BPSec
* Added MacOS build instructions


### Changed

* All bundle data types use `padded_vector_uint8_t` instead of `std::vector<uint8_t>` to remain uniform across both inducts, outducts, and BundleView.  This results in API changes for:
    - `LtpClientServiceDataToSend`
    - the outduct `Forward` calls
    - the internal buffers of `BundleViewV6` and `BundleViewV7`
* Added support for `BundleViewV6` and `BundleViewV7` to recycle their canonical block header objects whenever the bundle view object is reused in the creation or loading of bundles.
* Combined router and scheduler into one module.
+ Updated routing logic. Minor bug fixes and improved handling of interrupted/failed contacts.
  Upon a "failed" contact, HDTN will attempt to calculate a new route avoiding the failed node.
+ Changed `--bundle-rate` argument to floating point type to allow for rates slower than one 
  bundle-per-second.

### Removed

* Removed `finalDestinationEidUris`, `udpRateBps`, and `ltpMaxSendRateBitsPerSecOrZeroToDisable`
  fields from the HDTN configuration. These values now come from the contact plan or command-line
  arguments.

## [1.0.0] - 2023-05-02

### Fixed

* Fix bug where LtpSessionReceiver::NextDataToSend used a reference (reportSegmentIt) after queue.pop().
* Separate the LtpSessionSender data into high-priority-time-critical and low-priority-first-pass queues.  First pass data is lowest priority compared to time-critical data (e.g. report acks), so this priority separation prevents long first passes from starving and expiring the time-critical data.
* Static analysis fix for non-throw versions of `boost::property_tree::ptree::get_child` which return a reference to a destroyed second parameter.

### Added

* Config file additions:
    - add: `bufferRxToStorageOnLinkUpSaturation` (boolean default=`false`) to the hdtn global configs
    - add: `maxSumOfBundleBytesInPipeline` (`uint64_t`) to the outducts
    - add: `delaySendingOfDataSegmentsTimeMsOrZeroToDisable` (default=`20`) to LTP outducts for efficiently handling out-of-order report segments by deferring data retransmission.
    - add: `delaySendingOfReportSegmentsTimeMsOrZeroToDisable` (default=`20`) to LTP inducts for efficiently handling out-of-order data segments by deferring report retransmission.
    - add: Experimental feature `keepActiveSessionDataOnDisk` (boolean default=`false`) to both LTP inducts and outducts for supporting the running of LTP sessions (both for receivers and senders) from a solid-state disk drive in lieu of keeping session data-segments in memory.  If this feature is enabled, it also uses the added configuration values `activeSessionDataOnDiskNewFileDurationMs` and `activeSessionDataOnDiskDirectory` to determine where on the drive to temporarily store sessions.  Note: currently if a LTP link goes down, bundles don't yet get transferred to storage and get dropped, as this is still experimental.
* Add check for unused variables when loading hdtn config json files (uses regex). Prevent hdtn from starting until unused variable is removed.
* An error is thrown on startup `if (maxBundleSizeBytes * 2) > maxSumOfBundleBytesInPipeline`
* Add `AllOutductCapabilitiesTelemetry_t` to Telemetry.h and send it from egress to ingress, storage, and scheduler
* Add override specifier to virtual methods.
* Add a ForwardListQueue template class to util as a lightweight singly-linked list queue with low memory usage, intended for queues that are not heavily used and for use in objects that are frequently created and deleted but require a queue member variable.


### Changed

* The size of an LTP session object (both for senders and receivers) has been greatly reduced by sharing a reference to common data as opposed to copying.
* All config files now use boost::filesystem::path instead of std::string.
* Config file changes:
    - rename: `bundlePipelineLimit` to `maxNumberOfBundlesInPipeline` in the outducts
    - In the example config files, the `finalDestinationEidUris` were simplified by using ipn wildcard service numbers instead of fully-qualified (although fully-qualified ipn still supported)
* Combine `IreleaseStartHdr` and `IreleaseStopHdr` into single `IreleaseChangeHdr`.
* Ingress, Egress, and Storage now use pimpl (pointer to implementation) pattern.
* Change egress to scheduler socket from pubsub to pushpull.
* Change scheduler release messages socket from `pub` to `xpub` and wait on ingress and storage to subscribe.
* Ingress and storage now know the capability of the egress outducts.  For full description, see comment under `receive.cpp` titled "Config file changes".  Storage and Ingress now split cut-through capacity of an outduct in half, and storage now shares in the cut-through capacity.  For a particular outduct, the max data it can hold in its sending pipeline shall not exceed, whatever comes first, either:
    1. More bundles than `maxNumberOfBundlesInPipeline`
    2. More total bytes of bundles than `maxSumOfBundleBytesInPipeline`
    
### Removed

* Config file removals:
    - removed these values from hdtn config (because it was moved to hdtn_distributed_defaults.json for when NOT using hdtn-one-process): `zmqIngressAddress`, `zmqEgressAddress`, `zmqStorageAddress`, `zmqSchedulerAddress`, `zmqRouterAddress`, `zmqBoundIngressToConnectingEgressPortPath`, `zmqConnectingEgressToBoundIngressPortPath`, `zmqConnectingEgressBundlesOnlyToBoundIngressPortPath`, `zmqConnectingEgressToBoundSchedulerPortPath`, `zmqBoundIngressToConnectingStoragePortPath`, `zmqConnectingStorageToBoundIngressPortPath`, `zmqConnectingStorageToBoundEgressPortPath`, `zmqBoundEgressToConnectingStoragePortPath`, `zmqBoundRouterPubSubPortPath`
    - remove: `zmqMaxMessagesPerPath`, `zmqMaxMessageSizeBytes`, `zmqRegistrationServerAddress`, and `zmqRegistrationServerPortPath` from hdtn global configs
* Removed `release-message-sender` and `multiple-release-message-sender` executables
* Removed `finalDestination` field from contacts in contact plans

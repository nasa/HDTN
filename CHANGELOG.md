# Changelog

All notable changes to HDTN will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Fixed

* Static analysis fix for non-throw versions of `boost::property_tree::ptree::get_child` which return a reference to a destroyed second parameter.

### Added

* Config file additions:
    - add: `bufferRxToStorageOnLinkUpSaturation` (boolean default=`false`) to the hdtn global configs
    - add: `maxSumOfBundleBytesInPipeline` (`uint64_t`) to the outducts
* Add check for unused variables when loading hdtn config json files. Prevent hdtn from starting until unused variable is removed.
* An error is thrown on startup `if (maxBundleSizeBytes * 2) > maxSumOfBundleBytesInPipeline`
* Add `AllOutductCapabilitiesTelemetry_t` to Telemetry.h and send it from egress to ingress, storage, and scheduler


### Changed

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
    - remove: `zmqMaxMessagesPerPath`, `zmqMaxMessageSizeBytes`, `zmqRegistrationServerAddress`, and `zmqRegistrationServerPortPath` from hdtn global configs
* Removed `release-message-sender` and `multiple-release-message-sender` executables

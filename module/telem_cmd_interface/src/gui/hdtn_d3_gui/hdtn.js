/**
 * @file hdtn.js
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
 * This file is used to test the d3 gui offline (apart from websocket)
 * to verify correct animations and display.
 * It sets up a fully-populated hdtn scenario with all induct and outduct types populated.
 * The bottom of this file contains animation (new telemetry) events.
 */

if(window.location.protocol == 'file:') {

var INDUCT_ACTIVE_CONNECTIONS = {
    "timestampMilliseconds": 10000,
    "bundleCountEgress": 101,
    "bundleCountStorage": 102,
    "bundleByteCountEgress": 103,
    "bundleByteCountStorage": 104,
    "allInducts": [
        {
            "convergenceLayer": "LTP",
            "inductConnections": [
                {
                    "connectionName": "localhost:1113 Eng:103",
                    "inputName": "*:1113",
                    "totalBundlesReceived": 4775,
                    "totalBundleBytesReceived": 477628669,
                    "numReportSegmentTimerExpiredCallbacks": 1000,
                    "numReportSegmentsUnableToBeIssued": 1001,
                    "numReportSegmentsTooLargeAndNeedingSplit": 1002,
                    "numReportSegmentsCreatedViaSplit": 1003,
                    "numGapsFilledByOutOfOrderDataSegments": 1004,
                    "numDelayedFullyClaimedPrimaryReportSegmentsSent": 1005,
                    "numDelayedFullyClaimedSecondaryReportSegmentsSent": 1006,
                    "numDelayedPartiallyClaimedPrimaryReportSegmentsSent": 1007,
                    "numDelayedPartiallyClaimedSecondaryReportSegmentsSent": 1008,
                    "totalCancelSegmentsStarted": 1009,
                    "totalCancelSegmentSendRetries": 1010,
                    "totalCancelSegmentsFailedToSend": 1011,
                    "totalCancelSegmentsAcknowledged": 1012,
                    "numRxSessionsCancelledBySender": 1013,
                    "numStagnantRxSessionsDeleted": 1014,
                    "countUdpPacketsSent": 1015,
                    "countRxUdpCircularBufferOverruns": 1016,
                    "countTxUdpPacketsLimitedByRate": 1017
                }
            ]
        },
        {
            "convergenceLayer": "UDP",
            "inductConnections": [
                {
                    "connectionName": "127.0.0.1:62000",
                    "inputName": "*:1114",
                    "totalBundlesReceived": 15522,
                    "totalBundleBytesReceived": 1552618710
                }
            ]
        },
        {
            "convergenceLayer": "TCPCLv3",
            "inductConnections": [
                {
                    "connectionName": "127.0.0.1:62090 ipn:1.0",
                    "inputName": "*:65000",
                    "totalBundlesReceived": 15522,
                    "totalBundleBytesReceived": 1552618710
                },
                {
                    "connectionName": "127.0.0.1:62091 ipn:3.0",
                    "inputName": "*:65000",
                    "totalBundlesReceived": 4775,
                    "totalBundleBytesReceived": 477628669
                }
            ]
        },
        {
            "convergenceLayer": "TCPCLv4",
            "inductConnections": [
                {
                    "connectionName": "127.0.0.1:62092 TLS ipn:4.0",
                    "inputName": "*:65001",
                    "totalBundlesReceived": 15522,
                    "totalBundleBytesReceived": 1552618710
                },
                {
                    "connectionName": "127.0.0.1:62093 ipn:5.0",
                    "inputName": "*:65001",
                    "totalBundlesReceived": 4775,
                    "totalBundleBytesReceived": 477628669
                }
                ,
                {
                    "connectionName": "127.0.0.1:63000 ipn:6.0",
                    "inputName": "*:65001",
                    "totalBundlesReceived": 4775,
                    "totalBundleBytesReceived": 477628669
                }
            ]
        },
        {
            "convergenceLayer": "STCP",
            "inductConnections": [
                {
                    "connectionName": "null", //"127.0.0.1:62094",
                    "inputName": "*:65002",
                    "totalBundlesReceived": 15522,
                    "totalBundleBytesReceived": 1552618710
                }
            ]
        }
    ]
};
var INITIAL_HDTN_CONFIG = {
    "hdtnVersionString": "0.0.0", //not part of config but inserted by HDTN
    "hdtnConfigName": "my hdtn config",
    "userInterfaceOn": true,
    "mySchemeName": "unused_scheme_name",
    "myNodeId": 10,
    "myBpEchoServiceId": 2047,
    "myCustodialSsp": "unused_custodial_ssp",
    "myCustodialServiceId": 0,
    "isAcsAware": true,
    "acsMaxFillsPerAcsPacket": 100,
    "acsSendPeriodMilliseconds": 1000,
    "retransmitBundleAfterNoCustodySignalMilliseconds": 10000,
    "maxBundleSizeBytes": 10000000,
    "maxIngressBundleWaitOnEgressMilliseconds": 2000,
    "bufferRxToStorageOnLinkUpSaturation": false,
    "maxLtpReceiveUdpPacketSizeBytes": 65536,
    "zmqIngressAddress": "localhost",
    "zmqEgressAddress": "localhost",
    "zmqStorageAddress": "localhost",
    "zmqSchedulerAddress": "localhost",
    "zmqRouterAddress": "localhost",
    "zmqBoundIngressToConnectingEgressPortPath": 10100,
    "zmqConnectingEgressToBoundIngressPortPath": 10160,
    "zmqConnectingEgressToBoundSchedulerPortPath": 10162,
    "zmqConnectingEgressBundlesOnlyToBoundIngressPortPath": 10161,
    "zmqBoundIngressToConnectingStoragePortPath": 10110,
    "zmqConnectingStorageToBoundIngressPortPath": 10150,
    "zmqConnectingStorageToBoundEgressPortPath": 10120,
    "zmqBoundEgressToConnectingStoragePortPath": 10130,
    "zmqBoundSchedulerPubSubPortPath": 10200,
    "zmqBoundTelemApiPortPath": 10305,
    "zmqBoundRouterPubSubPortPath": 10210,
    "inductsConfig": {
        "inductConfigName": "myconfig",
        "inductVector": [
            {
                "name": "i1",
                "convergenceLayer": "ltp_over_udp",
                "boundPort": 1113,
                "numRxCircularBufferElements": 101,
                "thisLtpEngineId": 102,
                "remoteLtpEngineId": 103,
                "ltpReportSegmentMtu": 1003,
                "oneWayLightTimeMs": 1004,
                "oneWayMarginTimeMs": 205,
                "clientServiceId": 2,
                "preallocatedRedDataBytes": 200006,
                "ltpMaxRetriesPerSerialNumber": 5,
                "ltpRandomNumberSizeBits": 32,
                "ltpRemoteUdpHostname": "",
                "ltpRemoteUdpPort": 0,
                "ltpRxDataSegmentSessionNumberRecreationPreventerHistorySize": 1000,
                "ltpMaxExpectedSimultaneousSessions": 500,
                "ltpMaxUdpPacketsToSendPerSystemCall": 1,
                "delaySendingOfReportSegmentsTimeMsOrZeroToDisable": 20,
                "keepActiveSessionDataOnDisk": false,
                "activeSessionDataOnDiskNewFileDurationMs": 2000,
                "activeSessionDataOnDiskDirectory": ".\/"
            },
            {
                "name": "i2",
                "convergenceLayer": "udp",
                "boundPort": 4557,
                "numRxCircularBufferElements": 107,
                "numRxCircularBufferBytesPerElement": 65533,
                "activeConnections": ["UDP Src[0]"]
            },
            {
                "name": "i3",
                "convergenceLayer": "tcpcl_v3",
                "boundPort": 4558,
                "numRxCircularBufferElements": 1009,
                "numRxCircularBufferBytesPerElement": 2000,
                "keepAliveIntervalSeconds": 16,
                "tcpclV3MyMaxTxSegmentSizeBytes": 100000000
            },
            {
                "name": "i4",
                "convergenceLayer": "tcpcl_v4",
                "boundPort": 4560,
                "numRxCircularBufferElements": 1009,
                "numRxCircularBufferBytesPerElement": 2000,
                "keepAliveIntervalSeconds": 16,
                "tcpclV4MyMaxRxSegmentSizeBytes": 200000,
                "tlsIsRequired": false,
                "certificatePemFile": "C:\/hdtn_ssl_certificates\/cert.pem",
                "privateKeyPemFile": "C:\/hdtn_ssl_certificates\/privatekey.pem",
                "diffieHellmanParametersPemFile": "C:\/hdtn_ssl_certificates\/dh4096.pem"
            },
            {
                "name": "i5",
                "convergenceLayer": "stcp",
                "boundPort": 4559,
                "numRxCircularBufferElements": 1000,
                "keepAliveIntervalSeconds": 17
            }
        ]
    },
    "outductsConfig": {
        "outductConfigName": "myconfig",
        "outductVector": [
            {
                "name": "o1",
                "convergenceLayer": "ltp_over_udp",
                "nextHopNodeId": 50,
                "remoteHostname": "localhost",
                "remotePort": 1113,
                "maxNumberOfBundlesInPipeline": 5,
                "maxSumOfBundleBytesInPipeline": 50000000,
                "finalDestinationEidUris": [
                    "ipn:1.1",
                    "ipn:2.1",
                    "ipn:7.*"
                ],
                "thisLtpEngineId": 102,
                "remoteLtpEngineId": 103,
                "ltpDataSegmentMtu": 1003,
                "oneWayLightTimeMs": 1004,
                "oneWayMarginTimeMs": 205,
                "clientServiceId": 2,
                "numRxCircularBufferElements": 101,
                "ltpMaxRetriesPerSerialNumber": 5,
                "ltpCheckpointEveryNthDataSegment": 0,
                "ltpRandomNumberSizeBits": 32,
                "ltpSenderBoundPort": 2113,
                "ltpMaxSendRateBitsPerSecOrZeroToDisable": 0,
                "ltpMaxUdpPacketsToSendPerSystemCall": 1,
                "ltpSenderPingSecondsOrZeroToDisable": 15,
                "delaySendingOfDataSegmentsTimeMsOrZeroToDisable": 20,
                "keepActiveSessionDataOnDisk": false,
                "activeSessionDataOnDiskNewFileDurationMs": 2000,
                "activeSessionDataOnDiskDirectory": ".\/"
            },
            {
                "name": "o2",
                "convergenceLayer": "udp",
                "nextHopNodeId": 51,
                "remoteHostname": "localhost",
                "remotePort": 4557,
                "maxNumberOfBundlesInPipeline": 5,
                "maxSumOfBundleBytesInPipeline": 50000000,
                "finalDestinationEidUris": [
                    "ipn:4.1",
                    "ipn:6.1"
                ],
                "udpRateBps": 50000
            },
            {
                "name": "o3",
                "convergenceLayer": "tcpcl_v3",
                "nextHopNodeId": 52,
                "remoteHostname": "localhost",
                "remotePort": 4558,
                "maxNumberOfBundlesInPipeline": 5,
                "maxSumOfBundleBytesInPipeline": 50000000,
                "finalDestinationEidUris": [
                    "ipn:10.1",
                    "ipn:26.1"
                ],
                "keepAliveIntervalSeconds": 16,
                "tcpclV3MyMaxTxSegmentSizeBytes": 200000,
                "tcpclAllowOpportunisticReceiveBundles": true
            },
            {
                "name": "o4",
                "convergenceLayer": "tcpcl_v4",
                "nextHopNodeId": 1,
                "remoteHostname": "localhost",
                "remotePort": 4560,
                "maxNumberOfBundlesInPipeline": 50,
                "maxSumOfBundleBytesInPipeline": 50000000,
                "finalDestinationEidUris": [
                    "ipn:3.1"
                ],
                "keepAliveIntervalSeconds": 17,
                "tcpclAllowOpportunisticReceiveBundles": true,
                "tcpclV4MyMaxRxSegmentSizeBytes": 200000,
                "tryUseTls": false,
                "tlsIsRequired": false,
                "useTlsVersion1_3": false,
                "doX509CertificateVerification": false,
                "verifySubjectAltNameInX509Certificate": false,
                "certificationAuthorityPemFileForVerification": "C:\/hdtn_ssl_certificates\/cert.pem"
            },
            {
                "name": "o4",
                "convergenceLayer": "stcp",
                "nextHopNodeId": 53,
                "remoteHostname": "localhost",
                "remotePort": 4559,
                "maxNumberOfBundlesInPipeline": 5,
                "maxSumOfBundleBytesInPipeline": 50000000,
                "finalDestinationEidUris": [
                    "ipn:100.1",
                    "ipn:200.1",
                    "ipn:300.1"
                ],
                "keepAliveIntervalSeconds": 17
            }
        ]
    },
    "storageConfig": {
        "storageImplementation": "stdio_multi_threaded",
        "tryToRestoreFromDisk": false,
        "autoDeleteFilesOnExit": true,
        "totalStorageCapacityBytes": 8192000000,
        "storageDiskConfigVector": [
            {
                "name": "d1",
                "storeFilePath": ".\/store1.bin"
            },
            {
                "name": "d2",
                "storeFilePath": ".\/store2.bin"
            }
        ]
    }
};

var OUTDUCT_TELEM_UPDATE = {
    "timestampMilliseconds": 0,
    "totalBundlesGivenToOutducts": 2,
    "totalBundleBytesGivenToOutducts": 3,
    "totalTcpclBundlesReceived": 4,
    "totalTcpclBundleBytesReceived": 5,
    "totalStorageToIngressOpportunisticBundles": 6,
    "totalStorageToIngressOpportunisticBundleBytes": 7,
    "totalBundlesSuccessfullySent": 8,
    "totalBundleBytesSuccessfullySent": 9,
    "allOutducts": [
        {
            "convergenceLayer": "ltp_over_udp",
            "totalBundlesAcked": 12,
            "totalBundleBytesAcked": 13,
            "totalBundlesSent": 14,
            "totalBundleBytesSent": 15,
            "totalBundlesFailedToSend": 16,
            "linkIsUpPhysically": false,
            "linkIsUpPerTimeSchedule": false,
            "numCheckpointsExpired": 13,
            "numDiscretionaryCheckpointsNotResent": 14,
            "numDeletedFullyClaimedPendingReports": 15,
            "totalCancelSegmentsStarted": 160,
            "totalCancelSegmentSendRetries": 161,
            "totalCancelSegmentsFailedToSend": 162,
            "totalCancelSegmentsAcknowledged": 163,
            "totalPingsStarted": 164,
            "totalPingRetries": 165,
            "totalPingsFailedToSend": 166,
            "totalPingsAcknowledged": 167,
            "numTxSessionsReturnedToStorage": 168,
            "numTxSessionsCancelledByReceiver": 169,
            "countUdpPacketsSent": 12,
            "countRxUdpCircularBufferOverruns": 10,
            "countTxUdpPacketsLimitedByRate": 11
        },
        {
            "convergenceLayer": "udp",
            "totalBundlesAcked": 3,
            "totalBundleBytesAcked": 4,
            "totalBundlesSent": 5,
            "totalBundleBytesSent": 6,
            "totalBundlesFailedToSend": 7,
            "linkIsUpPhysically": true,
            "linkIsUpPerTimeSchedule": true,
            "totalPacketsSent": 50,
            "totalPacketBytesSent": 51,
            "totalPacketsDequeuedForSend": 52,
            "totalPacketBytesDequeuedForSend": 53,
            "totalPacketsLimitedByRate": 54
        },
        {
            "convergenceLayer": "tcpcl_v3",
            "totalBundlesAcked": 8,
            "totalBundleBytesAcked": 9,
            "totalBundlesSent": 10,
            "totalBundleBytesSent": 11,
            "totalBundlesFailedToSend": 12,
            "linkIsUpPhysically": true,
            "linkIsUpPerTimeSchedule": true,
            "totalFragmentsAcked": 30,
            "totalFragmentsSent": 31,
            "totalBundlesReceived": 32,
            "totalBundleBytesReceived": 33
        },
        {
            "convergenceLayer": "tcpcl_v4",
            "totalBundlesAcked": 8,
            "totalBundleBytesAcked": 9,
            "totalBundlesSent": 10,
            "totalBundleBytesSent": 11,
            "totalBundlesFailedToSend": 12,
            "linkIsUpPhysically": true,
            "linkIsUpPerTimeSchedule": false,
            "totalFragmentsAcked": 40,
            "totalFragmentsSent": 41,
            "totalBundlesReceived": 42,
            "totalBundleBytesReceived": 43
        },
        {
            "convergenceLayer": "stcp",
            "totalBundlesAcked": 4,
            "totalBundleBytesAcked": 5,
            "totalBundlesSent": 6,
            "totalBundleBytesSent": 7,
            "totalBundlesFailedToSend": 8,
            "linkIsUpPhysically": true,
            "linkIsUpPerTimeSchedule": true,
            "totalStcpBytesSent": 20
        }
    ]
};

var STORAGE_TELEM_UPDATE = {
    "timestampMilliseconds": 10000,
    "totalBundlesErasedFromStorageNoCustodyTransfer": 10,
    "totalBundlesErasedFromStorageWithCustodyTransfer": 20,
    "totalBundlesRewrittenToStorageFromFailedEgressSend": 30,
    "totalBundlesSentToEgressFromStorageReadFromDisk": 40,
    "totalBundleBytesSentToEgressFromStorageReadFromDisk": 45,
    "totalBundlesSentToEgressFromStorageForwardCutThrough": 50,
    "totalBundleBytesSentToEgressFromStorageForwardCutThrough": 55,
    "numRfc5050CustodyTransfers": 60,
    "numAcsCustodyTransfers": 70,
    "numAcsPacketsReceived": 80,
    "numBundlesOnDisk": 90,
    "numBundleBytesOnDisk": 100,
    "totalBundleWriteOperationsToDisk": 110,
    "totalBundleByteWriteOperationsToDisk": 120,
    "totalBundleEraseOperationsFromDisk": 130,
    "totalBundleByteEraseOperationsFromDisk": 140,
    "usedSpaceBytes": 0,
    "freeSpaceBytes": 1000
};

var AOCT = {
    "type": "allOutductCapability",
    "outductCapabilityTelemetryList": [
        {
            "type": "outductCapability",
            "outductArrayIndex": 0,
            "maxBundlesInPipeline": 5,
            "maxBundleSizeBytesInPipeline": 50000000,
            "nextHopNodeId": 50,
            "finalDestinationEidsList": [
                "ipn:1.1",
                "ipn:2.1",
                "ipn:7.*"
            ]
        },
        {
            "type": "outductCapability",
            "outductArrayIndex": 1,
            "maxBundlesInPipeline": 5,
            "maxBundleSizeBytesInPipeline": 50000000,
            "nextHopNodeId": 51,
            "finalDestinationEidsList": [
                "ipn:4.1",
                "ipn:6.1"
            ]
        },
        {
            "type": "outductCapability",
            "outductArrayIndex": 2,
            "maxBundlesInPipeline": 5,
            "maxBundleSizeBytesInPipeline": 50000000,
            "nextHopNodeId": 52,
            "finalDestinationEidsList": [
                "ipn:10.1",
                "ipn:26.1"
            ]
        },
        {
            "type": "outductCapability",
            "outductArrayIndex": 3,
            "maxBundlesInPipeline": 50,
            "maxBundleSizeBytesInPipeline": 50000000,
            "nextHopNodeId": 1,
            "finalDestinationEidsList": [
                "ipn:3.1"
            ]
        },
        {
            "type": "outductCapability",
            "outductArrayIndex": 4,
            "maxBundlesInPipeline": 5,
            "maxBundleSizeBytesInPipeline": 50000000,
            "nextHopNodeId": 53,
            "finalDestinationEidsList": [
                "ipn:100.1",
                "ipn:200.1",
                "ipn:300.1"
            ]
        }
    ]
};

function IncOutductTelem(paramIncBundles) {
    OUTDUCT_TELEM_UPDATE.timestampMilliseconds += 1000;
    OUTDUCT_TELEM_UPDATE.allOutducts.forEach(function(outductTelem, i) {
        outductTelem.totalBundlesAcked += paramIncBundles;
        outductTelem.totalBundleBytesAcked += paramIncBundles * 100000;
    });
}

function IncStorageTelem(paramInc) {
    STORAGE_TELEM_UPDATE.timestampMilliseconds += 1000;
    STORAGE_TELEM_UPDATE.usedSpaceBytes += paramInc;
    STORAGE_TELEM_UPDATE.freeSpaceBytes -= paramInc;
    STORAGE_TELEM_UPDATE.totalBundlesSentToEgressFromStorageReadFromDisk += paramInc;
    STORAGE_TELEM_UPDATE.totalBundleBytesSentToEgressFromStorageReadFromDisk += paramInc * 100000;

    STORAGE_TELEM_UPDATE.totalBundlesSentToEgressFromStorageReadFromDisk += paramInc;
    STORAGE_TELEM_UPDATE.totalBundleBytesSentToEgressFromStorageReadFromDisk += paramInc * 100000;

    STORAGE_TELEM_UPDATE.totalBundlesSentToEgressFromStorageForwardCutThrough += paramInc;
    STORAGE_TELEM_UPDATE.totalBundleBytesSentToEgressFromStorageForwardCutThrough += paramInc * 100000;

    STORAGE_TELEM_UPDATE.totalBundleWriteOperationsToDisk += paramInc;
    STORAGE_TELEM_UPDATE.totalBundleByteWriteOperationsToDisk += paramInc * 100000;
}

let changeFunctions = [
    function() {
        IncOutductTelem(10);
        let clone = JSON.parse(JSON.stringify(OUTDUCT_TELEM_UPDATE));
        app.UpdateWithData(clone);
    },
    function() {
        IncStorageTelem(10);
        let clone = JSON.parse(JSON.stringify(STORAGE_TELEM_UPDATE));
        app.UpdateWithData(clone);
    },
    function() {
        let clone = JSON.parse(JSON.stringify(INDUCT_ACTIVE_CONNECTIONS));
        app.UpdateWithData(clone);
    },
    function() {
        let clone = JSON.parse(JSON.stringify(AOCT));
        app.UpdateWithData(clone);
    },
    function() {
        IncOutductTelem(100);
        let clone = JSON.parse(JSON.stringify(OUTDUCT_TELEM_UPDATE));
        app.UpdateWithData(clone);
    },
    function() {
        IncStorageTelem(30);
        let clone = JSON.parse(JSON.stringify(STORAGE_TELEM_UPDATE));
        app.UpdateWithData(clone);
    },
    function() {
        //remove element 0 "ipn:4.1"
        INDUCT_ACTIVE_CONNECTIONS.allInducts[3].inductConnections.splice(1, 1); // 2nd parameter means remove one item only ;
        let clone = JSON.parse(JSON.stringify(INDUCT_ACTIVE_CONNECTIONS));
        app.UpdateWithData(clone);
    },
    function() {
        //remove element 0 "ipn:4.1"
        INDUCT_ACTIVE_CONNECTIONS.allInducts[3].inductConnections.push({
                    "connectionName": "127.0.0.1:62090 ipn:177.0",
                    "inputName": "*:65001",
                    "totalBundlesReceived": 15522,
                    "totalBundleBytesReceived": 1552618710
                });
        let clone = JSON.parse(JSON.stringify(INDUCT_ACTIVE_CONNECTIONS));
        app.UpdateWithData(clone);
    },
    function() {
        //remove element 0 "ipn:4.1"
        INDUCT_ACTIVE_CONNECTIONS.allInducts[4].inductConnections[0].connectionName= "127.0.0.1:62090 ipn:177.0";
        let clone = JSON.parse(JSON.stringify(INDUCT_ACTIVE_CONNECTIONS));
        app.UpdateWithData(clone);
    },
    function() {
        IncOutductTelem(1000);
        let clone = JSON.parse(JSON.stringify(OUTDUCT_TELEM_UPDATE));
        app.UpdateWithData(clone);
    },
    function() {
        IncStorageTelem(50);
        let clone = JSON.parse(JSON.stringify(STORAGE_TELEM_UPDATE));
        app.UpdateWithData(clone);
    },
    function() {
        //remove element 0 "ipn:4.1"
        INDUCT_ACTIVE_CONNECTIONS.allInducts[4].inductConnections[0].connectionName= "null";
        let clone = JSON.parse(JSON.stringify(INDUCT_ACTIVE_CONNECTIONS));
        app.UpdateWithData(clone);
    },
    function() {
        //remove element 0 "ipn:4.1"
        AOCT.outductCapabilityTelemetryList[1].finalDestinationEidsList.splice(0, 1); // 2nd parameter means remove one item only ;
        let clone = JSON.parse(JSON.stringify(AOCT));
        app.UpdateWithData(clone);
    },
    function() {
        IncOutductTelem(10000);
        let clone = JSON.parse(JSON.stringify(OUTDUCT_TELEM_UPDATE));
        app.UpdateWithData(clone);
    },
    function() {
        //remove element 0 "ipn:4.1"
        AOCT.outductCapabilityTelemetryList[1].finalDestinationEidsList.unshift("ipn:4.1");
        let clone = JSON.parse(JSON.stringify(AOCT));
        app.UpdateWithData(clone);
    },
    function() { //outduct 4 goes down physically
        OUTDUCT_TELEM_UPDATE.allOutducts[4].linkIsUpPhysically = false;
        let clone = JSON.parse(JSON.stringify(OUTDUCT_TELEM_UPDATE));
        app.UpdateWithData(clone);
    },
    function() { //outduct 4 goes up physically
        OUTDUCT_TELEM_UPDATE.allOutducts[4].linkIsUpPhysically = true;
        let clone = JSON.parse(JSON.stringify(OUTDUCT_TELEM_UPDATE));
        app.UpdateWithData(clone);
    },
    function() { //outduct 3 goes up time based
        OUTDUCT_TELEM_UPDATE.allOutducts[3].linkIsUpPerTimeSchedule = true;
        let clone = JSON.parse(JSON.stringify(OUTDUCT_TELEM_UPDATE));
        app.UpdateWithData(clone);
    },
    function() { //outduct 3 goes down time based
        OUTDUCT_TELEM_UPDATE.allOutducts[3].linkIsUpPerTimeSchedule = false;
        let clone = JSON.parse(JSON.stringify(OUTDUCT_TELEM_UPDATE));
        app.UpdateWithData(clone);
    }
];
changeFunctions.forEach(function(f, i){
    setTimeout(f, 200 * (i+1)); //slower than transition so will queue animations
});


}//window.location.protocol == file:

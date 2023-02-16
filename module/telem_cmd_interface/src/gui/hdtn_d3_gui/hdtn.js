if(window.location.protocol == 'file:') {

var INDUCT_ACTIVE_CONNECTIONS = [
    {
        "activeConnections": ["Engine 103"]
    },
    {
        "activeConnections": ["UDP Src[0]"]
    },
    {
        "activeConnections": ["Node 160", "Node 161"]
    },
    {
        "activeConnections": ["Node 170", "Node 171", "Node 172"]
    },
    {
        "activeConnections": ["STCP Src[0]", "STCP Src[1]"]
    }
];
var INITIAL_HDTN_CONFIG = {
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
                "activeSessionDataOnDiskDirectory": ".\/",
                "activeConnections": ["Engine 103"]
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
                "tcpclV3MyMaxTxSegmentSizeBytes": 100000000,
                "activeConnections": ["Node 160", "Node 161"]
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
                "diffieHellmanParametersPemFile": "C:\/hdtn_ssl_certificates\/dh4096.pem",
                "activeConnections": ["Node 170", "Node 171", "Node 172"]
            },
            {
                "name": "i5",
                "convergenceLayer": "stcp",
                "boundPort": 4559,
                "numRxCircularBufferElements": 1000,
                "keepAliveIntervalSeconds": 17,
                "activeConnections": ["STCP Src[0]", "STCP Src[1]"]
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


let changeFunctions = [
    function() {
        //remove element 0 "ipn:4.1"
        INITIAL_HDTN_CONFIG.inductsConfig.inductVector[3].activeConnections.splice(1, 1); // 2nd parameter means remove one item only ;
        app.UpdateWithData(INITIAL_HDTN_CONFIG);
    },
    function() {
        //remove element 0 "ipn:4.1"
        INITIAL_HDTN_CONFIG.inductsConfig.inductVector[3].activeConnections.push("Node 177");
        app.UpdateWithData(INITIAL_HDTN_CONFIG);
    },
    function() {
        //remove element 0 "ipn:4.1"
        INITIAL_HDTN_CONFIG.outductsConfig.outductVector[1].finalDestinationEidUris.splice(0, 1); // 2nd parameter means remove one item only ;
        app.UpdateWithData(INITIAL_HDTN_CONFIG);
    },
    function() {
        //remove element 0 "ipn:4.1"
        INITIAL_HDTN_CONFIG.outductsConfig.outductVector[1].finalDestinationEidUris.unshift("ipn:4.1");
        app.UpdateWithData(INITIAL_HDTN_CONFIG);
    }
];
changeFunctions.forEach(function(f, i){
    setTimeout(f, 5000 * (i+1));
});


}//window.location.protocol == file:

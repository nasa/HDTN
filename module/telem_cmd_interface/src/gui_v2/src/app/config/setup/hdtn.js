import { InputTypes } from "../config_fields/utils";

/*
    Shared Options
*/
export const convergenceLayerOptions = [
    {
        label: "STCP",
        value: "stcp"
    },
    {
        label: "UDP",
        value: "udp"
    },
    {
        label: "TCPLv3",
        value: "tcpl_v3"
    },
    {
        label: "TCPLv4",
        value: "tcpl_v4"
    },
    {
        label: "SLIP over UART",
        value: "slip_over_uart"
    },
    {
        label: "LTP over UDP",
        value: "ltp_over_udp"
    },
    {
        label: "LTP over Encap Local Stream",
        value: "ltp_over_encap_local_stream"
    },
    {
        label: "LTP over IPC",
        value: "ltp_over_ipc"
    }
]

/*
    Induct Configurations
*/
export const inductVectorFields = [
    {
        name: "name", 
        label: "Induct Vector Name", 
        default: "", 
        dataType: "string", 
        inputType: InputTypes.TextField, 
        required: true 
    },
    {
        name: "convergenceLayer", 
        label: "Induct Convergence Layer", 
        default: "stcp", 
        inputType: InputTypes.Select, 
        required: true,
        options: convergenceLayerOptions
    }
]

export const inductsConfigFields = [
    { 
        name: "inductConfigName", 
        label: "Induct Config Name", 
        default: "", 
        dataType: "string", 
        inputType: InputTypes.TextField, 
        required: true 
    },
    {
        name: "inductVector",
        label: "Induct Vectors",
        dataType: "array",
        arrayType: "object",
        arrayObjectType: inductVectorFields,
        arrayLabelField: "name"
    }
]

/*
    Outduct Configs
*/
export const outductVectorFields = [
    {
        name: "name", 
        label: "Outduct Vector Name", 
        default: "", 
        dataType: "string", 
        inputType: InputTypes.TextField, 
        required: true 
    },
    {
        name: "convergenceLayer", 
        label: "Induct Convergence Layer", 
        default: "stcp", 
        inputType: InputTypes.Select, 
        required: true,
        options: convergenceLayerOptions
    },
    {
        name: "nextHopNodeId", 
        label: "Next Hop NodeID", 
        default: 0, 
        dataType: "number", 
        inputType: InputTypes.TextField, 
        required: true
    },
    {
        name: "maxNumberOfBundlesInPipeline", 
        label: "Max Number of Bundles in the Pipeline", 
        default: 50, 
        dataType: "number", 
        inputType: InputTypes.TextField, 
        required: true 
    },
    {
        name: "maxSumOfBundleBytesInPipeline", 
        label: "Max Sum of Bundles In Pipeline (Bytes)", 
        default: 50000000, 
        dataType: "number", 
        inputType: InputTypes.TextField, 
        required: true 
    },
    {
        name: "bpEncapLocalSocketOrPipePath", 
        label: "BP Encap Local Socket Or Pipe Path", 
        default: "\\\\.\\pipe\\bp_local_pipe", 
        dataType: "string", 
        inputType: InputTypes.TextField, 
        required: true 
    }
]

export const outductsConfigFields = [
    { 
        name: "outductConfigName", 
        label: "Outduct Config Name", 
        default: "", 
        dataType: "string", 
        inputType: InputTypes.TextField, 
        required: true 
    },
    {
        name: "outductVector",
        label: "Outduct Vectors",
        dataType: "array",
        arrayType: "object",
        arrayObjectType: outductVectorFields,
        arrayLabelField: "name"
    }
]

/*
    Storage Configs
*/
export const storageDiskConfigVectorFields = [
    { 
        name: "name", 
        label: "Disk Config Vector Name", 
        default: "", 
        dataType: "string", 
        inputType: InputTypes.TextField, 
        required: true 
    },
    { 
        name: "storageFilePath", 
        label: "Storage File Path", 
        default: "", 
        dataType: "string", 
        inputType: InputTypes.TextField, 
        required: true 
    }
]

export const storageConfigFields = [
    { 
        name: "storageImplementation", 
        label: "Storage Implementation", 
        default: "asio_single_threaded", 
        inputType: InputTypes.Select, 
        required: true,
        options: [
            {
                label: "ASIO Single Threaded",
                value: "asio_single_threaded"
            },
            {
                label: "STDIO Multithreaded",
                value: "stdio_multi_threaded"
            }
        ]
    },
    { 
        name: "tryToRestoreFromDisk", 
        label: "Try To Restore From Disk", 
        default: false, 
        dataType: "boolean", 
        inputType: InputTypes.Switch, 
        required: true 
    },
    { 
        name: "autoDeleteFilesOnExit", 
        label: "Auto Delete Files On Exit", 
        default: true, 
        dataType: "boolean", 
        inputType: InputTypes.Switch, 
        required: true 
    },
    {
        name: "totalStorageCapacityBytes", 
        label: "Total Storage Capacity (Bytes)", 
        default: 8192000000, 
        dataType: "number", 
        inputType: InputTypes.TextField, 
        required: true 
    },
    {
        name: "storageDeletionPolicy", 
        label: "Storage Deletion Policy", 
        default: "never", 
        inputType: InputTypes.Select, 
        required: true,
        options: [
            {
                label: "Never",
                value: "never"
            },
            {
                label: "On Expiration",
                value: "on_expiration"
            },
            {
                label: "On Storage Full",
                value: "on_storage_full"
            },
        ]
    },
    {
        name: "storageDiskConfigVector",
        label: "Storage Disk Config Vectors",
        dataType: "array",
        arrayType: "object",
        arrayObjectType: storageDiskConfigVectorFields,
        arrayLabelField: "name"
    }
]

/*
    HDTN Configs
*/
export const hdtnConfigFields = [
    {
        name: "hdtnConfigName",
        label: "HDTN Config Name", 
        default: "", 
        dataType: "string", 
        inputType: InputTypes.TextField,
        required: true
    },
    { 
        name: "userInterfaceOn", 
        label: "Enable User Interface", 
        default: true, 
        dataType: "boolean", 
        inputType: InputTypes.Switch, 
        required: true 
    },
    { 
        name: "mySchemeName", 
        label: "Scheme Name", 
        default: "unused_scheme_name", 
        dataType: "string", 
        inputType: InputTypes.TextField,
        required: true 
    },
    { 
        name: "myNodeId", 
        label: "Node ID", 
        default: 10, 
        dataType: "number", 
        inputType: InputTypes.TextField, 
        required: true 
    },
    { 
        name: "myBpEchoServiceId", 
        label: "Bundle Protocol Echo Service ID", 
        default: 2047, 
        dataType: "number", 
        inputType: InputTypes.TextField,
        required: true 
    },
    { 
        name: "myCustodialSsp", 
        label: "Custodial SSP", 
        default: "unused_custodial_ssp",
        dataType: "string", 
        inputType: InputTypes.TextField, 
        required: true 
    },
    { 
        name: "myCustodialServiceId", 
        label: "Custodial Service ID", 
        default: 0, 
        dataType: "number", 
        inputType: InputTypes.TextField, 
        required: true 
    },
    { 
        name: "myRouterServiceId", 
        label: "Router Service ID", 
        default: 100, 
        dataType: "number", 
        inputType: InputTypes.TextField, 
        required: true 
    },
    { 
        name: "isAcsAware", 
        label: "ACS Aware", 
        default: true, 
        dataType: "boolean", 
        inputType: InputTypes.Switch, 
        required: true 
    },
    { 
        name: "acsMaxFillsPerAcsPacket", 
        label: "ACS Max Fills Per ACS Packet", 
        default: 100, 
        dataType: "number", 
        inputType: InputTypes.TextField, 
        required: true 
    },
    { 
        name: "acsSendPeriodMilliseconds", 
        label: "ACS Send Period (ms)", 
        default: 1000, 
        dataType: "number", 
        inputType: InputTypes.TextField, 
        required: true 
    },
    { 
        name: "retransmitBundleAfterNoCustodySignalMilliseconds", 
        label: "Retransmit Bundle After No Custody Signal (ms)", 
        default: 10000, 
        dataType: "number", 
        inputType: InputTypes.TextField,
        required: true
    },
    { 
        name: "maxBundleSizeBytes", 
        label: "Max Bundle Size (Bytes)", 
        default: 10000000, 
        dataType: "number", 
        inputType: InputTypes.TextField,
        required: true 
    },
    { 
        name: "maxIngressBundleWaitOnEgressMilliseconds", 
        label: "Max Ingress Bundle Wait On Egress (ms)", 
        default: 2000, 
        dataType: "number", 
        inputType: InputTypes.TextField, 
        required: true
    },
    { 
        name: "bufferRxToStorageOnLinkUpSaturation", 
        label: "Buffer Rx To Storage on Link Up Saturation", 
        default: false, 
        dataType: "boolean", 
        inputType: InputTypes.Switch, 
        required: true 
    },
    { 
        name: "maxLtpReceiveUdpPacketSizeBytes", 
        label: "Max LTP Receive UDP Packet Size (Bytes)", 
        default: 65536, 
        dataType: "number", 
        inputType: InputTypes.TextField, 
        required: true 
    },
    { 
        name: "neighborDepletedStorageDelaySeconds", 
        label: "Neighbor Depleted Storage Delay (s)", 
        default: 0, 
        dataType: "number", 
        inputType: InputTypes.TextField, 
        required: true 
    },
    { 
        name: "fragmentBundlesLargerThanBytes", 
        label: "Fragment Bundles Larger Than (Bytes)", 
        default: 0, 
        dataType: "number", 
        inputType: InputTypes.TextField, 
        required: true 
    },
    { 
        name: "zmqBoundRouterPubSubPortPath", 
        label: "ZeroMQ Bound Router Pub Sub Port Path", 
        default: 10200,
        dataType: "number", 
        inputType: InputTypes.TextField, 
        required: true 
    },
    { 
        name: "zmqBoundTelemApiPortPath", 
        label: "ZeroMQ Bundle Telementry API Port Path", 
        default: 10305, 
        dataType: "number", 
        inputType: InputTypes.TextField, 
        required: true 
    },
    {
        name: "inductsConfig",
        dataType: "object",
        objectType: inductsConfigFields,
        required: true
    },
    {
        name: "outductsConfig",
        dataType: "object",
        objectType: outductsConfigFields,
        required: true
    },
    {
        name: "storageConfig",
        dataType: "object",
        objectType: storageConfigFields,
        required: true
    }
]
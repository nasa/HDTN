import { InputTypes } from "../config_fields/utils";

export const distributedConfigFields = [
    {
        name: "zmqIngressAddress",
        label: "ZeroMQ Ingress Address", 
        default: "localhost", 
        dataType: "string", 
        inputType: InputTypes.TextField,
        required: true
    },
    {
        name: "zmqEgressAddress",
        label: "ZeroMQ Egress Address", 
        default: "localhost", 
        dataType: "string", 
        inputType: InputTypes.TextField,
        required: true
    },
    {
        name: "zmqStorageAddress",
        label: "ZeroMQ Storage Address", 
        default: "localhost", 
        dataType: "string", 
        inputType: InputTypes.TextField,
        required: true
    },
    {
        name: "zmqRouterAddress",
        label: "ZeroMQ Router Address", 
        default: "localhost", 
        dataType: "string", 
        inputType: InputTypes.TextField,
        required: true
    },
    {
        name: "zmqBoundIngressToConnectingEgressPortPath",
        label: "ZeroMQ Bound Ingress To Connecting Egress Port Path", 
        default: 10100, 
        dataType: "number", 
        inputType: InputTypes.TextField,
        required: true
    },
    {
        name: "zmqConnectingEgressToBoundIngressPortPath",
        label: "ZeroMQ Connecting Egress To Bound Ingress Port Path", 
        default: 10160, 
        dataType: "number", 
        inputType: InputTypes.TextField,
        required: true
    },
    {
        name: "zmqBoundEgressToConnectingRouterPortPath",
        label: "ZeroMQ Bound Egress To Connecting Router Port Path", 
        default: 10162, 
        dataType: "number", 
        inputType: InputTypes.TextField,
        required: true
    },
    {
        name: "zmqConnectingEgressBundlesOnlyToBoundIngressPortPath",
        label: "ZeroMQ Connecting Egress Bundles Only To Bound Ingress Port Path", 
        default: 10161, 
        dataType: "number", 
        inputType: InputTypes.TextField,
        required: true
    },
    {
        name: "zmqBoundIngressToConnectingStoragePortPath",
        label: "ZeroMQ Bound Ingress To Connecting Storage Port Path", 
        default: 10110, 
        dataType: "number", 
        inputType: InputTypes.TextField,
        required: true
    },
    {
        name: "zmqConnectingStorageToBoundIngressPortPath",
        label: "ZeroMQ Connecting Storage To Bound Ingress Port Path", 
        default: 10150, 
        dataType: "number", 
        inputType: InputTypes.TextField,
        required: true
    },
    {
        name: "zmqConnectingStorageToBoundEgressPortPath",
        label: "ZeroMQ Connecting Storage To Bound Egress Port Path", 
        default: 10120, 
        dataType: "number", 
        inputType: InputTypes.TextField,
        required: true
    },
    {
        name: "zmqConnectingStorageToBoundRouterPortPath",
        label: "ZeroMQ Connecting Storage To Bound Router Port Path", 
        default: 10121, 
        dataType: "number", 
        inputType: InputTypes.TextField,
        required: true
    },
    {
        name: "zmqBoundEgressToConnectingStoragePortPath",
        label: "ZeroMQ Bound Egress To Connecting Storage Port Path", 
        default: 10130, 
        dataType: "number", 
        inputType: InputTypes.TextField,
        required: true
    },
    {
        name: "zmqConnectingRouterToBoundEgressPortPath",
        label: "ZeroMQ Connecting Router To Bound Egress Port Path", 
        default: 10210, 
        dataType: "number", 
        inputType: InputTypes.TextField,
        required: true
    },
    {
        name: "zmqConnectingTelemToFromBoundIngressPortPath",
        label: "ZeroMQ Connecting Telemetry To From Bound Ingress Port Path", 
        default: 10301, 
        dataType: "number", 
        inputType: InputTypes.TextField,
        required: true
    },
    {
        name: "zmqConnectingTelemToFromBoundEgressPortPath",
        label: "ZeroMQ Connecting Telemetry To From Bound Egress Port Path ", 
        default: 10302, 
        dataType: "number", 
        inputType: InputTypes.TextField,
        required: true
    },
    {
        name: "zmqConnectingTelemToFromBoundStoragePortPath",
        label: "ZeroMQ Connecting Telemetry To From Bound Storage Port Path", 
        default: 10303, 
        dataType: "number", 
        inputType: InputTypes.TextField,
        required: true
    },
    {
        name: "zmqConnectingTelemToFromBoundRouterPortPath",
        label: "ZeroMQ Connecting Telemetry To From Bound Router Port Path", 
        default: 10304, 
        dataType: "number", 
        inputType: InputTypes.TextField,
        required: true
    }
]
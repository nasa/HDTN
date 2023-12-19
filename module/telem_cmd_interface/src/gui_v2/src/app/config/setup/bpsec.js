import { InputTypes } from "../config_fields/utils";

/*
    Policy Rules Configs
*/

export const securityContextParamConfigFields = [
    {
        name: "paramName",
        label: "Parameter Name",
        dataType: "string",
        default: "aesVariant",
        inputType: InputTypes.Select,
        required: true,
        options: [
            {
                label: "AES Variant",
                value: "aesVariant"
            },
            {
                label: "IV Size Bytes",
                value: "ivSizeBytes"
            },
            {
                label: "Key File",
                value: "keyFile"
            },
            {
                label: "Security Block CRC",
                value: "securityBlockCrc"
            },
            {
                label: "Scope Flags",
                value: "scopeFlags"
            },
            {
                label: "Wrapped Key",
                value: "wrappedKey"
            }
        ]
    },
    {
        name: "value", 
        label: "Value",
        dataType: "string",
        inputType: InputTypes.TextField,
        required: true 
    }
]

export const policyRulesConfigFields = [
    {
        name: "description", 
        label: "Policy Rule Description", 
        default: "", 
        dataType: "string", 
        inputType: InputTypes.TextField, 
        required: true 
    },
    {
        name: "securityPolicyRuleId", 
        label: "Security Policy Rule Id", 
        dataType: "number", 
        inputType: InputTypes.TextField, 
        required: true 
    },
    {
        name: "securityRole", 
        label: "Security Role", 
        default: "acceptor", 
        inputType: InputTypes.Select, 
        required: true,
        options: [
            {
                label: "Acceptor",
                value: "acceptor"
            },
            {
                label: "Source",
                value: "source"
            },
            {
                label: "Verifier",
                value: "verifier"
            }
        ]
    },
    {
        name: "securitySource", 
        label: "Security Source", 
        default: "", 
        dataType: "string", 
        inputType: InputTypes.TextField, 
        required: true 
    },
    {
        name: "bundleSource",
        label: "Bundle Source",
        dataType: "array",
        arrayType: "string",
        inputType: InputTypes.TextField
    },
    {
        name: "bundleFinalDestination",
        label: "Bundle Final Destination",
        dataType: "array",
        arrayType: "string",
        inputType: InputTypes.TextField
    },
    {
        name: "securityTargetBlockTypes",
        label: "Security Target Block Types",
        dataType: "array",
        arrayType: "number",
        default: 0,
        inputType: InputTypes.Select,
        options: [
            {
                label: "Primary Implicit Zero",
                value: 0
            },
            {
                label: "Payload",
                value: 1
            },
            {
                label: "Previous Node",
                value: 6
            },
            {
                label: "Bundle Age",
                value: 7
            },
            {
                label: "Hop Count",
                value: 10
            },
            {
                label: "Integrity",
                value: 11
            },
            {
                label: "Confidentiality",
                value: 12
            }
        ]
    },
    {
        name: "securityService", 
        label: "Security Service", 
        default: "confidentiality", 
        inputType: InputTypes.Select, 
        required: true,
        options: [
            {
                label: "Confidentiality",
                value: "confidentiality"
            },
            {
                label: "Integrity",
                value: "integrity"
            }
        ]
    },
    {
        name: "securityContext", 
        label: "Security Context", 
        default: "aesGcm", 
        inputType: InputTypes.Select, 
        required: true,
        options: [
            {
                label: "AES GCM",
                value: "aesGcm"
            },
            {
                label: "HMAC SHA",
                value: "hmacSha"
            }
        ]
    },
    {
        name: "securityContextParams",
        label: "Security Context Parameters",
        dataType: "array",
        arrayType: "object",
        arrayObjectType: securityContextParamConfigFields
    }
]

/*
    Failure Event Set Configs
*/

export const securityOperationsEventsConfigFields = [
    {
        name: "eventId", 
        label: "Event ID", 
        inputType: InputTypes.Select, 
        required: true,
        options: [
            {
                label: "SOP Misconfigured At Verifier",
                value: "sopMisconfiguredAtVerifier"
            },
            {
                label: "SOP Missing At Verifier",
                value: "sopMissingAtVerifier"
            },
            {
                label: "SOP Corrupted At Verifier",
                value: "sopCorruptedAtVerifier"
            },
            {
                label: "SOP Misconfigured At Acceptor",
                value: "sopMisconfiguredAtAcceptor"
            },
            {
                label: "SOP Missing At Acceptor",
                value: "sopMissingAtAcceptor"
            },
            {
                label: "SOP Corrupted At Acceptor",
                value: "sopCorruptedAtAcceptor"
            }
        ]
    },
    {
        name: "actions",
        label: "Actions",
        dataType: "array",
        arrayType: "string",
        inputType: InputTypes.Select,
        options: [
            {
                label: "Remove Security Operation",
                value: "removeSecurityOperation"
            },
            {
                label: "Remove Security Operation Target Block",
                value: "removeSecurityOperationTargetBlock"
            },
            {
                label: "Remove All Security Target Operations",
                value: "removeAllSecurityTargetOperations"
            },
            {
                label: "Fail Bundle Forwarding",
                value: "failBundleForwarding"
            },
            {
                label: "Request Bundle Storage",
                value: "requestBundleStorage"
            },
            {
                label: "Report Reason Code",
                value: "reportReasonCode"
            },
            {
                label: "Override Security Target Block BPCF",
                value: "overrideSecurityTargetBlockBpcf"
            },
            {
                label: "Override Security Block BPCF",
                value: "overrideSecurityBlockBpcf"
            }
        ]
    }
]

export const securityFailureEventSetsConfigFields = [
    {
        name: "name", 
        label: "Security Failure Event Set Name", 
        default: "", 
        dataType: "string", 
        inputType: InputTypes.TextField, 
        required: true 
    },
    {
        name: "description", 
        label: "Security Failure Event Set Description", 
        default: "", 
        dataType: "string", 
        inputType: InputTypes.TextField, 
        required: true 
    },
    {
        name: "securityOperationEvents",
        label: "Security Operation Events",
        dataType: "array",
        arrayType: "object",
        arrayObjectType: securityOperationsEventsConfigFields
    }
]

/*
    BPSec Configs
*/

export const bpSecConfigFields = [
    {
        name: "bpsecConfigName", 
        label: "BPSec Config Name", 
        default: "", 
        dataType: "string", 
        inputType: InputTypes.TextField, 
        required: true 
    },
    {
        name: "policyRules",
        label: "Policy Rules",
        dataType: "array",
        arrayType: "object",
        arrayObjectType: policyRulesConfigFields,
        arrayLabelField: "description"
    },
    {
        name: "securityFailureEventSets",
        label: "Failure Event Sets",
        dataType: "array",
        arrayType: "object",
        arrayObjectType: securityFailureEventSetsConfigFields,
        arrayLabelField: "name"
    }
]
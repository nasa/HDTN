The following describes the types for a BpSec json config file.  Note: "IFF" stands for "If and only if".

"policyRules": VECTOR of objects

    "description": STRING ANY (not used)
    "securityPolicyRuleId": UINT64 ANY (not used)
    "securityRole": STRING ["acceptor", "verifier", "source"]
    "securitySource": IPN STRING WITH WILDCARDS ["ipn:1.*", "ipn:*.*", "ipn:1.1"] INVALID="ipn:*.1"
    "bundleSource": VECTOR OF [SAME AS securitySource]
    "bundleFinalDestination": VECTOR OF [SAME AS securitySource]
    "securityTargetBlockTypes" (IFF securityRole == source): VECTOR OF UINT64 (BPV7_BLOCK_TYPE_CODE [0..13])
    "securityService": STRING ["confidentiality", "integrity"]
    "securityContext": STRING ["aesGcm", "hmacSha"]
    "securityFailureEventSetReference": STRING,
    "securityContextParams": VECTOR of objects
            "paramName": "aesVariant" (IFF securityRole == source), "value": UINT64 [128,256] default=256
            "paramName": "shaVariant" (IFF securityRole == source), "value": UINT64 [256,384,512] default=384
            "paramName": "ivSizeBytes" (IFF securityRole == source), "value": UINT64 [12,16] default=12
            "paramName": "scopeFlags" (IFF securityRole == source), "value": UINT64 [0..7] default=7
            "paramName": "securityBlockCrc" (IFF securityRole == source), "value": UINT64 [0,16,32] default=0
            "paramName": "keyEncryptionKeyFile", "value": boost::filesystem::path
            "paramName": "keyFile", "value": boost::filesystem::path


"securityFailureEventSets" : VECTOR of objects

    "name": STRING ANY
    "description": STRING ANY
    "securityOperationEvents": VECTOR of objects
            "eventID": STRING, "actions": VECTOR of STRING
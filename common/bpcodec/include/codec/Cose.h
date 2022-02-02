#ifndef COSE_H
#define COSE_H 1
#include <cstdint>
#include <cstddef>
//define the minimum of https://datatracker.ietf.org/doc/html/rfc8152
//per https://datatracker.ietf.org/doc/draft-ietf-dtn-bpsec-default-sc/


//https://www.iana.org/assignments/cose/cose.xhtml
enum class COSE_HEADER_PARAMETERS {
    //name = label           Value Type                             Description
    //------------           ----------                             -----------
    alg = 1, //              int / tstr                             Cryptographic algorithm to use
    crit = 2, //             [+ label]                              Critical headers to be understood
    content_type = 3, //     tstr / uint                            Content type of the payload
    kid = 4, //              bstr                                   Key identifier
    IV = 5, //               bstr                                   Full Initialization Vector
    Partial_IV = 6, //       bstr                                   Partial Initialization Vector
    counter_signature = 7, //COSE_Signature / [+COSE_Signature]     CBOR - encoded signature structure
    CounterSignature0 = 9, //bstr                                   Counter signature with implied signer and headers
    kid_context = 10 //      bstr                                   Identifies the context for the key identifier
};

enum class COSE_ALGORITHMS {
    //name = value           Description
    //------------           -----------
    A128GCM = 1, //          AES-GCM mode w/ 128-bit key, 128-bit tag
    A256GCM = 3, //          AES-GCM mode w/ 256-bit key, 128-bit tag
    HMAC_256_256 = 5, //     HMAC w/ SHA-256
    HMAC_384_384 = 6, //     HMAC w/ SHA-384
    HMAC_512_512 = 7 //      HMAC w/ SHA-512
};

enum class COSE_KEY_COMMON_PARAMETERS {
    //name = label           CBOR Type            Description
    //------------           ----------           -----------
    kty = 1, //              tstr / int           Identification of the key type
    kid = 2, //              bstr                 Key identification value - match to kid in message
    alg = 3, //              tstr / int           Key usage restriction to this algorithm
    key_ops = 4, //          [+ (tstr/int)]       Restrict set of permissible operations
    Base_IV = 5 //           bstr                 Base IV to be XORed with Partial IVs
    
};

enum class COSE_KEY_TYPES {
    //name = value           Description
    //------------           -----------
    OKP = 1, //              Octet Key Pair
    EC2 = 2, //              Elliptic Curve Keys w/ x- and y-coordinate pair
    RSA = 3, //              RSA Key
    Symmetric = 4, //        Symmetric Keys
    HSS_LMS = 5, //          Public key for HSS/LMS hash-based digital signature
    WalnutDSA = 6 //         WalnutDSA public key
};

#endif //COSE_H

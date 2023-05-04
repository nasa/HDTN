/**
 * @file BPSecManager.h
 * @author  Nadia Kortas <nadia.kortas@nasa.gov>
 * @author  Brian Tomko <brian.j.tomko@nasa.gov>
 *
 * @copyright Copyright  2021 United States Government as represented by
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
 * This BPSecManager defines the methods for adding and processing 
 * BCB confidentiality and BIB integrity blocks based on the security
 * policy Rules. It also includes the implementation of the cryptographic 
 * functions using OpenSSL APIs.
 *
 */


#ifndef BPSECMANAGER_H
#define BPSECMANAGER_H 1

#include <string>
#include <cstdint>
#include <vector>
#include "codec/BundleViewV7.h"
#include "codec/bpv7.h"
#include "bpsec_export.h"
#include <openssl/evp.h>
#include <openssl/bio.h>
#include <openssl/err.h>
#include "BPSecManager.h"
#include <forward_list>

class BPSecManager { 
private:
      BPSecManager();
public:
    struct EvpCipherCtxWrapper {
        BPSEC_EXPORT EvpCipherCtxWrapper();
        BPSEC_EXPORT ~EvpCipherCtxWrapper();
        EVP_CIPHER_CTX* m_ctx;
    };
    struct HmacCtxWrapper {
        BPSEC_EXPORT HmacCtxWrapper();
        BPSEC_EXPORT ~HmacCtxWrapper();
        HMAC_CTX* m_ctx;
    };

    BPSEC_EXPORT BPSecManager(const bool isSecEnabled);
    BPSEC_EXPORT ~BPSecManager();

    /**
    * Generates Keyed Hash for integrity
    *
    * @param input The target data to be hashed
    * @param key The HMAC key to be used for hashing
    * @param suite The variant of SHA-2 algorithm to be used
    * to generate the hash
    * @return the computed HMAC digest
    */
    BPSEC_EXPORT static unsigned char* hmac_sha(const EVP_MD *evp_md,
                                         std::string key,
                                         std::string data, unsigned char *md,
                                         unsigned int *md_len);

   /**
    * Adds BIB interity block to the bundle
    *
    * @param bv The bundle BundleViewV7
    * @return True if BIB was added successfully
    */
//    BPSEC_EXPORT bool bpSecSign(BundleViewV7 & bv);
    
   /**
    * Processes the BIB integrity blocks
    *
    * @param bv The bundle BundleViewV7
    * @return true if the keyed Hash is verified correctly
    */
  //  BPSEC_EXPORT bool bpSecVerify(BundleViewV7 & bv);
    
    const bool m_isSecEnabled;
    

    BPSEC_EXPORT static int aes_gcm_encrypt(std::string gcm_pt, std::string gcm_key,
                                  std::string gcm_iv, std::string gcm_aad,
                                  unsigned char* ciphertext, unsigned char* tag,      
                                  int *outlen);

    BPSEC_EXPORT static int aes_gcm_decrypt(std::string gcm_ct, std::string gcm_tag,
                                     std::string gcm_key, std::string gcm_iv,
                                     std::string gcm_aad, unsigned char* pt, int *outlen);

    BPSEC_EXPORT static bool HmacSha(HmacCtxWrapper& ctxWrapper,
        const BPSEC_SHA2_VARIANT variant,
        const std::vector<boost::asio::const_buffer>& ipptParts,
        const uint8_t* key, const uint64_t keyLength,
        uint8_t* messageDigestOut, unsigned int& messageDigestOutSize);

    BPSEC_EXPORT static bool AesGcmEncrypt(EvpCipherCtxWrapper& ctxWrapper,
        const uint8_t* unencryptedData, const uint64_t unencryptedDataLength,
        const uint8_t* key, const uint64_t keyLength,
        const uint8_t* iv, const uint64_t ivLength,
        const std::vector<boost::asio::const_buffer>& aadParts,
        uint8_t* cipherTextOut, uint64_t& cipherTextOutSize, uint8_t* tagOut);

    BPSEC_EXPORT static bool AesGcmDecrypt(EvpCipherCtxWrapper& ctxWrapper,
        const uint8_t* encryptedData, const uint64_t encryptedDataLength,
        const uint8_t* key, const uint64_t keyLength,
        const uint8_t* iv, const uint64_t ivLength,
        const std::vector<boost::asio::const_buffer>& aadParts,
        const uint8_t* tag,
        uint8_t* decryptedDataOut, uint64_t& decryptedDataOutSize);

    BPSEC_EXPORT static bool AesWrapKey(
        const uint8_t* keyEncryptionKey, const unsigned int keyEncryptionKeyLength,
        const uint8_t* keyToWrap, const unsigned int keyToWrapLength,
        uint8_t* wrappedKeyOut, unsigned int& wrappedKeyOutSize);

    BPSEC_EXPORT static bool AesUnwrapKey(
        const uint8_t* keyEncryptionKey, const unsigned int keyEncryptionKeyLength,
        const uint8_t* keyToUnwrap, const unsigned int keyToUnwrapLength,
        uint8_t* unwrappedKeyOut, unsigned int& unwrappedKeyOutSize);

    //return false if there was an error
    BPSEC_EXPORT static bool TryDecryptBundle(EvpCipherCtxWrapper& ctxWrapper,
        BundleViewV7& bv,
        const uint8_t* keyEncryptionKey, const unsigned int keyEncryptionKeyLength, //NULL if not present (for unwrapping DEK only)
        const uint8_t* dataEncryptionKey, const unsigned int dataEncryptionKeyLength, //NULL if not present (when no wrapped key is present)
        std::vector<boost::asio::const_buffer>& aadPartsTemporaryMemory);

    //return false if there was an error
    BPSEC_EXPORT static bool TryEncryptBundle(EvpCipherCtxWrapper& ctxWrapper,
        BundleViewV7& bv,
        BPSEC_BCB_AES_GCM_AAD_SCOPE_MASKS aadScopeMask,
        COSE_ALGORITHMS aesVariant,
        BPV7_CRC_TYPE bibCrcType,
        const cbhe_eid_t& securitySource,
        const uint64_t* targetBlockNumbers, const unsigned int numTargetBlockNumbers,
        const uint8_t* iv, const unsigned int ivLength,
        const uint8_t* keyEncryptionKey, const unsigned int keyEncryptionKeyLength, //NULL if not present (for wrapping DEK only)
        const uint8_t* dataEncryptionKey, const unsigned int dataEncryptionKeyLength, //NULL if not present (when no wrapped key is present)
        std::vector<boost::asio::const_buffer>& aadPartsTemporaryMemory,
        const uint64_t* insertBcbBeforeThisBlockNumberIfNotNull);

   /**
    * Adds BCB confidentiality block to the bundle
    *
    * @param bv The BPv7 bundle BundleViewV7
    * @return true if BCB block is added successfully
    */

     // BPSEC_EXPORT bool bcbAdd(BundleViewV7 & bv, uint64_t target,  std::string gcm_key,
       //                   std::string gcm_iv, std::string gcm_aad);
    
   /**
    * Processes BCB confidentiality block 
    *
    * @param bv The BPv7 bundle BundleViewV7
    * @return true if BCB block is processed successfully
    */
  // BPSEC_EXPORT bool bcbProcess(BundleViewV7 & bv, std::string gcm_key,
    //                      std::string gcm_iv, std::string gcm_aad); 

};


#endif // BPSECMANAGER_H


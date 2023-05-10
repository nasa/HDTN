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

class BPSecManager { 
private:
      BPSecManager();
public:
    struct EvpCipherCtxWrapper {
        BPSEC_EXPORT EvpCipherCtxWrapper();
        BPSEC_EXPORT ~EvpCipherCtxWrapper();
        /// PIMPL idiom
        struct Impl;
        /// Pointer to the internal implementation
        std::unique_ptr<Impl> m_pimpl;
    };
    struct HmacCtxWrapper {
        BPSEC_EXPORT HmacCtxWrapper();
        BPSEC_EXPORT ~HmacCtxWrapper();
        /// PIMPL idiom
        struct Impl;
        /// Pointer to the internal implementation
        std::unique_ptr<Impl> m_pimpl;
    };
    struct ReusableElementsInternal {
        std::vector<boost::asio::const_buffer> constBufferVec; //aadParts and ipptParts
        std::vector<BundleViewV7::Bpv7CanonicalBlockView*> blocks;
    };

    BPSEC_EXPORT BPSecManager(const bool isSecEnabled);
    BPSEC_EXPORT ~BPSecManager();

    
    const bool m_isSecEnabled;
    
    /**
    * Generates Keyed Hash for integrity
    *
    * @param ctxWrapper The reusable (allocated once) openssl HMAC_CTX context.
    * @param variant The SHA variant to use.
    * @param ipptParts The Integrity-Protected Plaintext (IPPT) to hash.
    *        These are pointer-length pieces to avoid having to concatenate everything to contiguous memory.
    * @param key The HMAC key to be used for hashing
    * @param key The HMAC key length to be used for hashing
    * @param messageDigestOut The generated hash output of this function.
    * @param messageDigestOutSize The generated size (in bytes) of the hash output of this function.
    * @return true if there were no errors, false otherwise
    */
    BPSEC_EXPORT static bool HmacSha(HmacCtxWrapper& ctxWrapper,
        const COSE_ALGORITHMS variant,
        const std::vector<boost::asio::const_buffer>& ipptParts,
        const uint8_t* key, const uint64_t keyLength,
        uint8_t* messageDigestOut, unsigned int& messageDigestOutSize);

    
    /**
    * Verifies any BIB block(s) within the preloaded bundle view.  The bundle must be loaded with padded data.
    *
    * @param ctxWrapper The reusable (allocated once) openssl HMAC_CTX context.
    * @param ctxWrapperForKeyUnwrap The reusable (allocated once) openssl EVP_CIPHER_CTX context (for key unwrap operations).
    * @param bv The preloaded bundle view.  The bundle must be loaded with padded data!
    * @param keyEncryptionKey The key used for unwrapping any wrapped keys included in the BIB blocks. (set to NULL if not present)
    * @param keyEncryptionKeyLength The length of the keyEncryptionKey. (should set to 0 if keyEncryptionKey is NULL)
    * @param hmacKey The HMAC key to be used for hashing (when no wrapped key is present)
    * @param hmacKeyLength The length of the hmacKey. (should set to 0 if hmacKey is NULL)
    * @param reusableElementsInternal Create this once throughout the program duration.  This parameter is used internally and
    *        resizes constantly.  This parameter is intended to minimize unnecessary allocations/deallocations.
    * @param markBibForDeletion If true, mark BIB block for deletion on successful verification so that it will be removed
    *                           after the next rerender of the bundleview.
    * @param renderInPlaceWhenFinished Perform a render in place automatically on the bundle view at function completion.
    *                                  Set to false to render manually (i.e. if there are other operations needing completed prior to render).
    * @return true if there were no errors, false otherwise
    */
    BPSEC_EXPORT static bool TryVerifyBundleIntegrity(HmacCtxWrapper& ctxWrapper,
        EvpCipherCtxWrapper& ctxWrapperForKeyUnwrap,
        BundleViewV7& bv,
        const uint8_t* keyEncryptionKey, const unsigned int keyEncryptionKeyLength, //NULL if not present (for unwrapping hmac key only)
        const uint8_t* hmacKey, const unsigned int hmacKeyLength, //NULL if not present (when no wrapped key is present)
        ReusableElementsInternal& reusableElementsInternal,
        const bool markBibForDeletion,
        const bool renderInPlaceWhenFinished);

    /**
    * Adds a BIB block to the preloaded bundle view.  The bundle must be loaded with padded data.
    *
    * @param ctxWrapper The reusable (allocated once) openssl HMAC_CTX context.
    * @param ctxWrapperForKeyWrap The reusable (allocated once) openssl EVP_CIPHER_CTX context (for key wrap operations).
    * @param bv The preloaded bundle view.  The bundle must be loaded with padded data!
    * @param integrityScopeMask The scope mask used for determining the Integrity-Protected Plaintext (IPPT) to hash.
    * @param variant The SHA variant to use.
    * @param bibCrcType Defines the CRC (if any) to use for the new BIB block.
    * @param securitySource The CBHE encoded security source to use for the new BIB block.
    * @param targetBlockNumbers A uint64 array of block numbers that the new BIB block shall target.
    * @param numTargetBlockNumbers The length of the targetBlockNumbers array.
    * @param keyEncryptionKey The key used for wrapping the HMAC key and adding a wrapped key in the new BIB block. (set to NULL if not present)
    * @param keyEncryptionKeyLength The length of the keyEncryptionKey. (should set to 0 if keyEncryptionKey is NULL)
    * @param hmacKey The HMAC key to be used for hashing (when no wrapped key is present)
    * @param hmacKeyLength The length of the hmacKey. (should set to 0 if hmacKey is NULL)
    * @param reusableElementsInternal Create this once throughout the program duration.  This parameter is used internally and
    *        resizes constantly.  This parameter is intended to minimize unnecessary allocations/deallocations.
    * @param insertBibBeforeThisBlockNumberIfNotNull If not NULL, places the BIB before this particular block number, used for making unit tests match examples.
    *        If NULL, the BIB is placed immediately after the primary block.
    * @param renderInPlaceWhenFinished Perform a render in place automatically on the bundle view at function completion.
    *                                  Set to false to render manually (i.e. if there are other operations needing completed prior to render).
    * @return true if there were no errors, false otherwise
    */
    BPSEC_EXPORT static bool TryAddBundleIntegrity(HmacCtxWrapper& ctxWrapper,
        EvpCipherCtxWrapper& ctxWrapperForKeyWrap,
        BundleViewV7& bv,
        BPSEC_BIB_HMAC_SHA2_INTEGRITY_SCOPE_MASKS integrityScopeMask,
        COSE_ALGORITHMS variant,
        BPV7_CRC_TYPE bibCrcType,
        const cbhe_eid_t& securitySource,
        const uint64_t* targetBlockNumbers, const unsigned int numTargetBlockNumbers,
        const uint8_t* keyEncryptionKey, const unsigned int keyEncryptionKeyLength, //NULL if not present (for unwrapping hmac key only)
        const uint8_t* hmacKey, const unsigned int hmacKeyLength, //NULL if not present (when no wrapped key is present)
        ReusableElementsInternal& reusableElementsInternal,
        const uint64_t* insertBibBeforeThisBlockNumberIfNotNull,
        const bool renderInPlaceWhenFinished);

    /**
    * Encrypts data (optionally in-place) for confidentiality. Ciphertext length is equivalent to plaintext length.
    *
    * @param ctxWrapper The reusable (allocated once) openssl EVP_CIPHER_CTX context.
    * @param unencryptedData The plaintext data to encrypt.
    * @param unencryptedDataLength The length in bytes of unencryptedData.
    * @param key The data encryption key (DEK) to be used for encrypting the plaintext data.
    * @param keyLength The length in bytes of the data encryption key (DEK).
    * @param iv The initialization vector to use.
    * @param ivLength The length in bytes of the initialization vector.
    * @param aadParts The additional authenticated data (AAD) to use.
    *        These are pointer-length pieces to avoid having to concatenate everything to contiguous memory.
    * @param cipherTextOut The generated ciphertext of this function.  Must not be partially overlapping with unencryptedData.
    *                      If this cipherTextOut pointer is the same as the unencryptedData pointer (fully overlapping), the encryption will be done in-place.
    * @param cipherTextOutSize The generated size (in bytes) of the ciphertext output of this function (will be equivalent to plaintext length).
    * @return true if there were no errors, false otherwise
    */
    BPSEC_EXPORT static bool AesGcmEncrypt(EvpCipherCtxWrapper& ctxWrapper,
        const uint8_t* unencryptedData, const uint64_t unencryptedDataLength,
        const uint8_t* key, const uint64_t keyLength,
        const uint8_t* iv, const uint64_t ivLength,
        const std::vector<boost::asio::const_buffer>& aadParts,
        uint8_t* cipherTextOut, uint64_t& cipherTextOutSize, uint8_t* tagOut);

    /**
    * Decrypts data (optionally in-place) for confidentiality.  Plaintext length is equivalent to ciphertext length.
    *
    * @param ctxWrapper The reusable (allocated once) openssl EVP_CIPHER_CTX context.
    * @param encryptedData The ciphertext data to decrypt.
    * @param encryptedDataLength The length in bytes of encryptedData.
    * @param key The data encryption key (DEK) to be used for decrypting the plaintext data.
    * @param keyLength The length in bytes of the data encryption key (DEK).
    * @param iv The initialization vector to use.
    * @param ivLength The length in bytes of the initialization vector.
    * @param aadParts The additional authenticated data (AAD) to use.
    *        These are pointer-length pieces to avoid having to concatenate everything to contiguous memory.
    * @param tag The authentication tag to use.
    * @param decryptedDataOut The generated plaintext of this function.  Must not be partially overlapping with encryptedData.
    *                      If this decryptedDataOut pointer is the same as the encryptedData pointer (fully overlapping), the decryption will be done in-place.
    * @param decryptedDataOutSize The generated size (in bytes) of the plaintext output of this function (will be equivalent to encryptedDataLength length).
    * @return true if there were no errors, false otherwise
    */
    BPSEC_EXPORT static bool AesGcmDecrypt(EvpCipherCtxWrapper& ctxWrapper,
        const uint8_t* encryptedData, const uint64_t encryptedDataLength,
        const uint8_t* key, const uint64_t keyLength,
        const uint8_t* iv, const uint64_t ivLength,
        const std::vector<boost::asio::const_buffer>& aadParts,
        const uint8_t* tag,
        uint8_t* decryptedDataOut, uint64_t& decryptedDataOutSize);

    /**
    * Wraps a key.
    *
    * @param ctxWrapper The reusable (allocated once) openssl EVP_CIPHER_CTX context (for keywrap operations).
    * @param keyEncryptionKey The key encryption key (KEK) to be used for encrypting/wrapping the data encryption key (DEK).
    * @param keyEncryptionKeyLength The length in bytes of the key encryption key (KEK).
    * @param keyToWrap The data encryption key (DEK) to be encrypted/wrapped.
    * @param keyToWrapLength The length in bytes of the data encryption key (DEK) that will be getting wrapped.
    * @param wrappedKeyOut The generated wrapped key of this function.
    * @param wrappedKeyOutSize The generated size (in bytes) of the wrapped key (will be equivalent to keyEncryptionKeyLength + 8).
    * @return true if there were no errors, false otherwise
    */
    BPSEC_EXPORT static bool AesWrapKey(EvpCipherCtxWrapper& ctxWrapper,
        const uint8_t* keyEncryptionKey, const unsigned int keyEncryptionKeyLength,
        const uint8_t* keyToWrap, const unsigned int keyToWrapLength,
        uint8_t* wrappedKeyOut, unsigned int& wrappedKeyOutSize);

    /**
    * Unwraps a key.
    *
    * @param ctxWrapper The reusable (allocated once) openssl EVP_CIPHER_CTX context (for key unwrap operations).
    * @param keyEncryptionKey The key encryption key (KEK) to be used for unwrapping the data encryption key (DEK).
    * @param keyEncryptionKeyLength The length in bytes of the key encryption key (KEK).
    * @param keyToUnwrap The wrapped key to be unwrapped into a data encryption key (DEK).
    * @param keyToUnwrapLength The length in bytes of the wrapped key.
    * @param unwrappedKeyOut The generated (unwrapped) data encryption key (DEK) of this function.
    * @param unwrappedKeyOutSize The generated size (in bytes) of the unwrapped key (will be equivalent to keyEncryptionKeyLength).
    * @return true if there were no errors, false otherwise
    */
    BPSEC_EXPORT static bool AesUnwrapKey(EvpCipherCtxWrapper& ctxWrapper,
        const uint8_t* keyEncryptionKey, const unsigned int keyEncryptionKeyLength,
        const uint8_t* keyToUnwrap, const unsigned int keyToUnwrapLength,
        uint8_t* unwrappedKeyOut, unsigned int& unwrappedKeyOutSize);

    /**
    * Decrypts any BCB target block(s) within the preloaded bundle view in-place.  The bundle must be loaded with padded data.
    *
    * @param ctxWrapper The reusable (allocated once) openssl EVP_CIPHER_CTX context.
    * @param ctxWrapperForKeyUnwrap The reusable (allocated once) openssl EVP_CIPHER_CTX context (for key unwrap operations).
    * @param bv The preloaded bundle view.  The bundle must be loaded with padded data!
    * @param keyEncryptionKey The key used for unwrapping any wrapped keys included in the BCB blocks.
    *                         Any wrapped keys would then be unwrapped into a dataEncryptionKey (DEK) which would be used
    *                         in lieu of the function parameter dataEncryptionKey. Set to NULL if not present.
    * @param keyEncryptionKeyLength The length of the keyEncryptionKey. (should set to 0 if keyEncryptionKey is NULL)
    * @param dataEncryptionKey The key (DEK) to be used for decrypting (when no wrapped key is present).
    *                          Set to NULL if always expecting wrapped keys to be included in the received BCBs.
    * @param dataEncryptionKeyLength The length of the dataEncryptionKey. (should set to 0 if dataEncryptionKey is NULL)
    * @param reusableElementsInternal Create this once throughout the program duration.  This parameter is used internally and
    *        resizes constantly.  This parameter is intended to minimize unnecessary allocations/deallocations.
    * @param renderInPlaceWhenFinished Perform a render in place automatically on the bundle view at function completion.
    *                                  Set to false to render manually (i.e. if there are other operations needing completed prior to render).
    * @post The BCB block is marked for deletion on successful in-place decryption, and the bundle view is (optionally) rerendered in-place.
    * @return true if there were no errors, false otherwise
    */
    BPSEC_EXPORT static bool TryDecryptBundle(EvpCipherCtxWrapper& ctxWrapper,
        EvpCipherCtxWrapper& ctxWrapperForKeyUnwrap,
        BundleViewV7& bv,
        const uint8_t* keyEncryptionKey, const unsigned int keyEncryptionKeyLength,
        const uint8_t* dataEncryptionKey, const unsigned int dataEncryptionKeyLength,
        ReusableElementsInternal& reusableElementsInternal,
        const bool renderInPlaceWhenFinished);

    /**
    * Adds a BCB block to the preloaded bundle view and encrypts the targets.  The bundle must be loaded with padded data.
    *
    * @param ctxWrapper The reusable (allocated once) openssl EVP_CIPHER_CTX context.
    * @param ctxWrapperForKeyWrap The reusable (allocated once) openssl EVP_CIPHER_CTX context (for key wrap operations).
    * @param bv The preloaded bundle view.  The bundle must be loaded with padded data!
    * @param aadScopeMask The scope mask used for determining the additional authenticated data (AAD).
    * @param aesVariant The AES variant to use.
    * @param bcbCrcType Defines the CRC (if any) to use for the new BCB block.
    * @param securitySource The CBHE encoded security source to use for the new BCB block.
    * @param targetBlockNumbers A uint64 array of block numbers that the new BCB block shall target.
    * @param numTargetBlockNumbers The length of the targetBlockNumbers array.
    * @param iv The initialization vector to use.
    * @param ivLength The length in bytes of the initialization vector.
    * @param keyEncryptionKey The key used for wrapping the data encryption key (DEK) included in the BCB blocks. (set to NULL if not present)
    * @param keyEncryptionKeyLength The length of the keyEncryptionKey. (should set to 0 if keyEncryptionKey is NULL)
    * @param dataEncryptionKey The key (DEK) to be used for encrypting (when no wrapped key is present)
    * @param dataEncryptionKeyLength The length of the dataEncryptionKey. (should set to 0 if hmacKey is NULL)
    * @param reusableElementsInternal Create this once throughout the program duration.  This parameter is used internally and
    *        resizes constantly.  This parameter is intended to minimize unnecessary allocations/deallocations.
    * @param insertBcbBeforeThisBlockNumberIfNotNull If not NULL, places the BCB before this particular block number, used for making unit tests match examples.
    *        If NULL, the BCB is placed immediately after the primary block.
    * @param renderInPlaceWhenFinished Perform a render in place automatically on the bundle view at function completion.
    *                                  Set to false to render manually (i.e. if there are other operations needing completed prior to render).
    * @post A new BCB block is added on successful in-place encryption of the BCB's target(s) to the bv (BundleViewV7), and the bundle is rerendered in-place.
    * @return true if there were no errors, false otherwise
    */
    BPSEC_EXPORT static bool TryEncryptBundle(EvpCipherCtxWrapper& ctxWrapper,
        EvpCipherCtxWrapper& ctxWrapperForKeyWrap,
        BundleViewV7& bv,
        BPSEC_BCB_AES_GCM_AAD_SCOPE_MASKS aadScopeMask,
        COSE_ALGORITHMS aesVariant,
        BPV7_CRC_TYPE bcbCrcType,
        const cbhe_eid_t& securitySource,
        const uint64_t* targetBlockNumbers, const unsigned int numTargetBlockNumbers,
        const uint8_t* iv, const unsigned int ivLength,
        const uint8_t* keyEncryptionKey, const unsigned int keyEncryptionKeyLength,
        const uint8_t* dataEncryptionKey, const unsigned int dataEncryptionKeyLength,
        ReusableElementsInternal& reusableElementsInternal,
        const uint64_t* insertBcbBeforeThisBlockNumberIfNotNull,
        const bool renderInPlaceWhenFinished);

};


#endif // BPSECMANAGER_H


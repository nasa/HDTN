/**
 * @file BPSecManager.cpp
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
 */

#include "BPSecManager.h"
#include <boost/make_unique.hpp>
#include "BinaryConversions.h"
#include "PaddedVectorUint8.h"
#include "Logger.h"
#include <boost/algorithm/string.hpp>
#include <boost/next_prior.hpp>
#include "CborUint.h"
#include <boost/noncopyable.hpp>
#include <openssl/evp.h>
#include <openssl/hmac.h>
#include <openssl/aes.h>
#include <openssl/bio.h>
#include <openssl/err.h>
#include <openssl/opensslv.h>

#if (OPENSSL_VERSION_NUMBER >= 0x30000000L)
# define BPSEC_USE_SSL3 1
#include <openssl/params.h>
#endif

static constexpr hdtn::Logger::SubProcess subprocess = hdtn::Logger::SubProcess::none;


//#define BPSEC_MANAGER_PRINT_DEBUG

BPSecManager::BPSecManager(const bool isSecEnabled) : 
m_isSecEnabled(isSecEnabled)
{
}

BPSecManager::~BPSecManager() {}



//https://www.openssl.org/docs/man3.0/man7/crypto.html
//Performance:
/*
Performance can be gained by upgrading to openssl 3:

If you perform the same operation many times then it is recommended to use "Explicit fetching"
to prefetch an algorithm once initially,
and then pass this created object to any operations that are currently using "Implicit fetching".
See an example of Explicit fetching in "USING ALGORITHMS IN APPLICATIONS".*/

struct BPSecManager::EvpCipherCtxWrapper::Impl : private boost::noncopyable {
    Impl() : m_ctx(EVP_CIPHER_CTX_new()) {}
    ~Impl() {
        if (m_ctx) {
            EVP_CIPHER_CTX_free(m_ctx);
            m_ctx = NULL;
        }
    }
    EVP_CIPHER_CTX* m_ctx;
};

BPSecManager::EvpCipherCtxWrapper::EvpCipherCtxWrapper() : m_pimpl(boost::make_unique<BPSecManager::EvpCipherCtxWrapper::Impl>()) {}
BPSecManager::EvpCipherCtxWrapper::~EvpCipherCtxWrapper() {}


struct BPSecManager::HmacCtxWrapper::Impl : private boost::noncopyable {
#ifdef BPSEC_USE_SSL3
    Impl() :
        m_mac(EVP_MAC_fetch(NULL, "HMAC", NULL)),
        m_ctx(EVP_MAC_CTX_new(m_mac)) {}
    ~Impl() {
        if (m_ctx) {
            EVP_MAC_CTX_free(m_ctx);
            EVP_MAC_free(m_mac); //could do this right after EVP_MAC_CTX_new in ctor
            m_ctx = NULL;
            m_mac = NULL;
        }
    }
    EVP_MAC* m_mac;
    EVP_MAC_CTX* m_ctx;
#else
    Impl() : m_ctx(HMAC_CTX_new()) {}
    ~Impl() {
        if (m_ctx) {
            HMAC_CTX_free(m_ctx);
            m_ctx = NULL; //EVP_MAC* mac = EVP_MAC_fetch(NULL, getenv("MY_MAC"), NULL);
        }
    }
    HMAC_CTX* m_ctx;
#endif
};
BPSecManager::HmacCtxWrapper::HmacCtxWrapper() : m_pimpl(boost::make_unique<BPSecManager::HmacCtxWrapper::Impl>()) {}
BPSecManager::HmacCtxWrapper::~HmacCtxWrapper() {}

static const uint8_t ALG_MINUS_5_TO_BYTE_LENGTH_LUT[3] = { //7 is highest index in COSE_ALGORITHMS
    256 / 8, //HMAC_256_256 = 5,
    384 / 8, //HMAC_384_384 = 6,
    512 / 8  //HMAC_512_512 = 7
};

#ifdef BPSEC_USE_SSL3
//must be non-const to work as param data
//data is non-const, but should act as const
static char sha256Str[] = "SHA256";
static char sha384Str[] = "SHA384";
static char sha512Str[] = "SHA512";

//https://mta.openssl.org/pipermail/openssl-users/2021-July/013990.html
static const OSSL_PARAM ALG_MINUS_5_TO_PARAM_LUT[3][2] = {
    {OSSL_PARAM_construct_utf8_string("digest", sha256Str, 0), OSSL_PARAM_construct_end()}, //HMAC_256_256 = 5,
    {OSSL_PARAM_construct_utf8_string("digest", sha384Str, 0), OSSL_PARAM_construct_end()}, //HMAC_384_384 = 6,
    {OSSL_PARAM_construct_utf8_string("digest", sha512Str, 0), OSSL_PARAM_construct_end()} //HMAC_512_512 = 7
};
#else
static const EVP_MD* ALG_MINUS_5_TO_EVPMD_LUT[3] = { //7 is highest index in COSE_ALGORITHMS
    EVP_sha256(), //HMAC_256_256 = 5,
    EVP_sha384(), //HMAC_384_384 = 6,
    EVP_sha512()  //HMAC_512_512 = 7 //returns pointer to a static const variable
};
#endif

//https://www.openssl.org/docs/man1.1.1/man3/HMAC_Init_ex.html

bool BPSecManager::HmacSha(HmacCtxWrapper& ctxWrapper,
    const COSE_ALGORITHMS variant,
    const std::vector<boost::asio::const_buffer>& ipptParts,
    const uint8_t* key, const uint64_t keyLength,
    uint8_t* messageDigestOut, unsigned int& messageDigestOutSize)
{
    const uint8_t variantMinus5 = (static_cast<uint8_t>(variant)) - 5u;
    if (variantMinus5 > 2) { //not 0, 1, or 2
        return false;
    }
    messageDigestOutSize = 0;
    const unsigned int expectedHmacSizeBytes = ALG_MINUS_5_TO_BYTE_LENGTH_LUT[variantMinus5];
#ifdef BPSEC_USE_SSL3
    EVP_MAC_CTX* ctx = ctxWrapper.m_pimpl->m_ctx;
    const OSSL_PARAM* osslParams = ALG_MINUS_5_TO_PARAM_LUT[variantMinus5];
    //EVP_MAC_init() sets up the underlying context ctx with information given via the key and params arguments.
    //The MAC key has a length of keylen and the parameters in params are processed before setting the key.
    //If key is NULL, the key must be set via params either as part of this call or separately using EVP_MAC_CTX_set_params().
    //Providing non-NULL params to this function is equivalent to calling EVP_MAC_CTX_set_params() with those params for the same ctx beforehand.
    //EVP_MAC_init() should be called before EVP_MAC_update() and EVP_MAC_final().
    //EVP_MAC_init() return 1 on success, 0 on error.
    if (!EVP_MAC_init(ctx, key, (std::size_t)keyLength, osslParams)) {
        return false;
    }

    //EVP_MAC_update() adds datalen bytes from data to the MAC input.
    //EVP_MAC_update() return 1 on success, 0 on error.
    for (std::size_t i = 0; i < ipptParts.size(); ++i) {
        const boost::asio::const_buffer& cb = ipptParts[i];
        if (!EVP_MAC_update(ctx, reinterpret_cast<const uint8_t*>(cb.data()), cb.size())) {
            return false;
        }
    }

    //EVP_MAC_final() does the final computation and stores the result in the memory pointed at by out of size outsize,
    //and sets the number of bytes written in *outl at.
    //If out is NULL or outsize is too small, then no computation is made.
    //To figure out what the output length will be and allocate space for it dynamically,
    //simply call with out being NULL and outl pointing at a valid location,
    //then allocate space and make a second call with out pointing at the allocated space.
    //EVP_MAC_final() return 1 on success, 0 on error.
    std::size_t macFinalOutSize;
    if (!EVP_MAC_final(ctx, messageDigestOut, &macFinalOutSize, expectedHmacSizeBytes)) {
        return false;
    }
    messageDigestOutSize = static_cast<unsigned int>(macFinalOutSize);
#else
    HMAC_CTX* ctx = ctxWrapper.m_pimpl->m_ctx;
    const EVP_MD* evpMdPtr = ALG_MINUS_5_TO_EVPMD_LUT[variantMinus5];
    //HMAC_Init_ex() initializes or reuses a HMAC_CTX structure to use the hash function evp_md and key key.
    //If both are NULL, or if key is NULL and evp_md is the same as the previous call, then the existing key is reused.
    //ctx must have been created with HMAC_CTX_new() before the first use of an HMAC_CTX in this function.
    //
    //If HMAC_Init_ex() is called with key NULL and evp_md is not the same as the previous digest used by ctx
    //then an error is returned because reuse of an existing key with a different digest is not supported.
    //HMAC_Init_ex() return 1 for success or 0 if an error occurred.
    if (!HMAC_Init_ex(ctx, key, (int)keyLength, evpMdPtr, NULL)) {
        return false;
    }

    //HMAC_Update() can be called repeatedly with chunks of the message to be authenticated (len bytes at data).
    //HMAC_Update() return 1 for success or 0 if an error occurred.
    for (std::size_t i = 0; i < ipptParts.size(); ++i) {
        const boost::asio::const_buffer& cb = ipptParts[i];
        if (!HMAC_Update(ctx, reinterpret_cast<const uint8_t*>(cb.data()), cb.size())) {
            return false;
        }
    }

    //HMAC_Final() places the message authentication code in md, which must have space for the hash function output.
    //HMAC_Final() return 1 for success or 0 if an error occurred.
    if (!HMAC_Final(ctx, messageDigestOut, &messageDigestOutSize)) {
        return false;
    }
#endif
    
    //RFC9173:
    //The output of the HMAC MUST be equal to the size of the SHA2 hashing
    //function: 256 bits for SHA-256, 384 bits for SHA-384, and 512 bits
    //for SHA-512.

    return messageDigestOutSize == expectedHmacSizeBytes;
}

//https://www.openssl.org/docs/man1.1.1/man3/EVP_EncryptUpdate.html
    
//buffer size of cipherTextOut must be at least (unencryptedDataLength + EVP_MAX_BLOCK_LENGTH)
bool BPSecManager::AesGcmEncrypt(EvpCipherCtxWrapper& ctxWrapper,
    const uint8_t* unencryptedData, const uint64_t unencryptedDataLength,
    const uint8_t* key, const uint64_t keyLength,
    const uint8_t* iv, const uint64_t ivLength,
    const std::vector<boost::asio::const_buffer>& aadParts,
    uint8_t* cipherTextOut, uint64_t& cipherTextOutSize, uint8_t* tagOut)
{
    //EVP_CIPHER_CTX_new() returns a pointer to a newly created EVP_CIPHER_CTX for success and NULL for failure.
    EVP_CIPHER_CTX* ctx = ctxWrapper.m_pimpl->m_ctx;

    cipherTextOutSize = 0;
    uint8_t* const cipherTextOutBase = cipherTextOut;

    //The constant EVP_MAX_BLOCK_LENGTH is also the maximum block length for all ciphers.

    if (ctx == NULL) {
        return false;
    }

    // Set cipher type and mode
    //per https://www.openssl.org/docs/man1.1.1/man3/EVP_EncryptUpdate.html
    // New code should use EVP_EncryptInit_ex(), EVP_EncryptFinal_ex(), EVP_DecryptInit_ex(), EVP_DecryptFinal_ex(),
    // EVP_CipherInit_ex() and EVP_CipherFinal_ex() because they can reuse an existing context without allocating and freeing it up on each call.
    const EVP_CIPHER* cipherPtr;
    if (keyLength == 16) {
        cipherPtr = EVP_aes_128_gcm();
    }
    else if (keyLength == 32) {
        cipherPtr = EVP_aes_256_gcm();
    }
    else {
        LOG_ERROR(subprocess) << "Error Incorrect Key length!!";
        return false;
    }
    //EVP_EncryptInit_ex(), EVP_EncryptUpdate() and EVP_EncryptFinal_ex() return 1 for success and 0 for failure.
    if (!EVP_EncryptInit_ex(ctx, cipherPtr, NULL, NULL, NULL)) {
        ERR_print_errors_fp(stderr);
        return false;
    }

    // Set IV length if default 96 bits is not appropriate
    // return value unclear in docs
    if (!EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_SET_IVLEN, (int)ivLength, NULL)) {
        ERR_print_errors_fp(stderr);
        return false;
    }

    // Initialise key and IV
    //It is possible to set all parameters to NULL except type in an initial call and supply the remaining parameters in subsequent calls,
    //all of which have type set to NULL. This is done when the default cipher parameters are not appropriate.
    //EVP_EncryptInit_ex(), EVP_EncryptUpdate() and EVP_EncryptFinal_ex() return 1 for success and 0 for failure.
    if (!EVP_EncryptInit_ex(ctx, NULL, NULL, key, iv)) {
        ERR_print_errors_fp(stderr);
        return false;
    }

    
    //Encrypts inl bytes from the buffer in and writes the encrypted version to out.
    //This function can be called multiple times to encrypt successive blocks of data.
    //The amount of data written depends on the block alignment of the encrypted data.
    //For most ciphers and modes, the amount of data written can be anything from zero bytes to (inl + cipher_block_size - 1) bytes.
    //For wrap cipher modes, the amount of data written can be anything from zero bytes to (inl + cipher_block_size) bytes.
    //For stream ciphers, the amount of data written can be anything from zero bytes to inl bytes.
    //Thus, out should contain sufficient room for the operation being performed.
    //The actual number of bytes written is placed in outl.
    //It also checks if in and out are partially overlapping, and if they are 0 is returned to indicate failure.
    //
    // Zero or more calls to specify any AAD
    //AEAD Interface
    //The EVP interface for Authenticated Encryption with Associated Data (AEAD) modes are subtly
    // altered and several additional ctrl operations are supported depending on the mode specified.
    // To specify additional authenticated data (AAD), a call to EVP_CipherUpdate(), EVP_EncryptUpdate() or EVP_DecryptUpdate()
    // should be made with the output parameter out set to NULL.
    int tmpOutLength;
    //EVP_EncryptInit_ex(), EVP_EncryptUpdate() and EVP_EncryptFinal_ex() return 1 for success and 0 for failure.
    for (std::size_t i = 0; i < aadParts.size(); ++i) {
        const boost::asio::const_buffer& cb = aadParts[i];
        if (!EVP_EncryptUpdate(ctx, NULL, &tmpOutLength, reinterpret_cast<const uint8_t*>(cb.data()), (int)cb.size())) {
            ERR_print_errors_fp(stderr);
            return false;
        }
    }
    
    //inplace encryption should work per: https://github.com/openssl/openssl/issues/6498#issuecomment-397865986
    //Encrypt plaintext
    //EVP_EncryptInit_ex(), EVP_EncryptUpdate() and EVP_EncryptFinal_ex() return 1 for success and 0 for failure.
    if (!EVP_EncryptUpdate(ctx, cipherTextOut, &tmpOutLength, unencryptedData, (int)unencryptedDataLength)) {
        ERR_print_errors_fp(stderr);
        return false;
    }
    cipherTextOut += tmpOutLength;


    // Finalise: note get no output for GCM
    //If padding is enabled (the default) then EVP_EncryptFinal_ex() encrypts the "final" data,
    //that is any data that remains in a partial block.
    //It uses standard block padding (aka PKCS padding) as described in the NOTES section, below.
    //The encrypted final data is written to out which should have sufficient space for one cipher block.
    //The number of bytes written is placed in outl.
    //After this function is called the encryption operation is finished
    //and no further calls to EVP_EncryptUpdate() should be made.
    //EVP_EncryptInit_ex(), EVP_EncryptUpdate() and EVP_EncryptFinal_ex() return 1 for success and 0 for failure.
    if (!EVP_EncryptFinal_ex(ctx, cipherTextOut, &tmpOutLength)) {
        ERR_print_errors_fp(stderr);
        return false;
    }
    cipherTextOut += tmpOutLength;

    //Get tag
    //EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_GET_TAG, taglen, tag):
    //Writes taglen bytes of the tag value to the buffer indicated by tag.
    //This call can only be made when encrypting data and after all
    //data has been processed (e.g. after an EVP_EncryptFinal() call).
    //For OCB, taglen must either be 16 or the value previously set via EVP_CTRL_AEAD_SET_TAG.
    // return value unclear in docs
    if (!EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_GET_TAG, EVP_GCM_TLS_TAG_LEN, tagOut)) { //The length of the authentication tag, prior to any CBOR encoding, MUST be 128 bits.
        ERR_print_errors_fp(stderr);
        return false;
    }

    
    cipherTextOutSize = static_cast<uint64_t>(cipherTextOut - cipherTextOutBase);

    return true;
}

bool BPSecManager::AesGcmDecrypt(EvpCipherCtxWrapper& ctxWrapper,
    const uint8_t* encryptedData, const uint64_t encryptedDataLength,
    const uint8_t* key, const uint64_t keyLength,
    const uint8_t* iv, const uint64_t ivLength,
    const std::vector<boost::asio::const_buffer>& aadParts,
    const uint8_t* tag,
    uint8_t* decryptedDataOut, uint64_t& decryptedDataOutSize)
{
    //EVP_CIPHER_CTX_new() returns a pointer to a newly created EVP_CIPHER_CTX for success and NULL for failure.
    EVP_CIPHER_CTX* ctx = ctxWrapper.m_pimpl->m_ctx;

    decryptedDataOutSize = 0;
    uint8_t* const decryptedDataOutBase = decryptedDataOut;

    //The constant EVP_MAX_BLOCK_LENGTH is also the maximum block length for all ciphers.

    if (ctx == NULL) {
        return false;
    }

    // Set cipher type and mode
    //per https://www.openssl.org/docs/man1.1.1/man3/EVP_EncryptUpdate.html
    // New code should use EVP_EncryptInit_ex(), EVP_EncryptFinal_ex(), EVP_DecryptInit_ex(), EVP_DecryptFinal_ex(),
    // EVP_CipherInit_ex() and EVP_CipherFinal_ex() because they can reuse an existing context without allocating and freeing it up on each call.
    const EVP_CIPHER* cipherPtr;
    if (keyLength == 16) {
        cipherPtr = EVP_aes_128_gcm();
    }
    else if (keyLength == 32) {
        cipherPtr = EVP_aes_256_gcm();
    }
    else {
        LOG_ERROR(subprocess) << "Error Incorrect Key length!!";
        return false;
    }
    //EVP_EncryptInit_ex(), EVP_EncryptUpdate() and EVP_EncryptFinal_ex() return 1 for success and 0 for failure.
    if (!EVP_DecryptInit_ex(ctx, cipherPtr, NULL, NULL, NULL)) {
        ERR_print_errors_fp(stderr);
        return false;
    }

    //Set IV length, omit for 96 bits
    // return value unclear in docs
    if (!EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_SET_IVLEN, (int)ivLength, NULL)) {
        ERR_print_errors_fp(stderr);
        return false;
    }

    // Specify key and IV 
    //EVP_DecryptInit_ex() and EVP_DecryptUpdate() return 1 for success and 0 for failure.
    if (!EVP_DecryptInit_ex(ctx, NULL, NULL, key, iv)) {
        ERR_print_errors_fp(stderr);
        return false;
    }

    // Zero or more calls to specify any AAD
    //AEAD Interface
    //The EVP interface for Authenticated Encryption with Associated Data (AEAD) modes are subtly
    // altered and several additional ctrl operations are supported depending on the mode specified.
    // To specify additional authenticated data (AAD), a call to EVP_CipherUpdate(), EVP_EncryptUpdate() or EVP_DecryptUpdate()
    // should be made with the output parameter out set to NULL.
    int tmpOutLength;
    //EVP_DecryptInit_ex() and EVP_DecryptUpdate() return 1 for success and 0 for failure.
    for (std::size_t i = 0; i < aadParts.size(); ++i) {
        const boost::asio::const_buffer& cb = aadParts[i];
        if (!EVP_DecryptUpdate(ctx, NULL, &tmpOutLength, reinterpret_cast<const uint8_t*>(cb.data()), (int)cb.size())) {
            ERR_print_errors_fp(stderr);
            return false;
        }
    }

    //inplace encryption (perhaps decryption) should work per: https://github.com/openssl/openssl/issues/6498#issuecomment-397865986
    // Decrypt plaintext 
    //EVP_DecryptInit_ex() and EVP_DecryptUpdate() return 1 for success and 0 for failure.
    if (!EVP_DecryptUpdate(ctx, decryptedDataOut, &tmpOutLength, encryptedData, (int)encryptedDataLength)) {
        ERR_print_errors_fp(stderr);
        return false;
    }
    decryptedDataOut += tmpOutLength;

    // Set expected tag value.
    // AEAD Interface:
    //  EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_SET_TAG, taglen, tag)
    //  When decrypting, this call sets the expected tag to taglen bytes from tag.
    //  taglen must be between 1 and 16 inclusive.
    //  The tag must be set prior to any call to EVP_DecryptFinal() or EVP_DecryptFinal_ex().
    // return value unclear in docs
    if (!EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_SET_TAG, EVP_GCM_TLS_TAG_LEN, (void*)tag)) {
        ERR_print_errors_fp(stderr);
        return false;
    }

    // Finalise: note get no output for GCM 
    // AEAD Interface: The tag must be set prior to any call to EVP_DecryptFinal() or EVP_DecryptFinal_ex().
    // EVP_DecryptFinal_ex() returns 0 if the decrypt failed or 1 for success.
    if (!EVP_DecryptFinal_ex(ctx, decryptedDataOut, &tmpOutLength)) {
        return false;
    }

    decryptedDataOutSize = static_cast<uint64_t>(decryptedDataOut - decryptedDataOutBase);

    return true;
}

bool BPSecManager::AesWrapKey(EvpCipherCtxWrapper& ctxWrapper,
    const uint8_t* keyEncryptionKey, const unsigned int keyEncryptionKeyLength,
    const uint8_t* keyToWrap, const unsigned int keyToWrapLength,
    uint8_t* wrappedKeyOut, unsigned int& wrappedKeyOutSize)
{
#ifdef BPSEC_USE_SSL3
    const uint8_t* const wrappedKeyOutBase = wrappedKeyOut;
    EVP_CIPHER_CTX* ctx = ctxWrapper.m_pimpl->m_ctx;
    if (ctx == NULL) {
        return false;
    }

    //Used for Legacy purposes only. This flag needed to be set to indicate the cipher handled wrapping.
    EVP_CIPHER_CTX_set_flags(ctx, EVP_CIPHER_CTX_FLAG_WRAP_ALLOW);

    // Set cipher type and mode
    //per https://www.openssl.org/docs/man1.1.1/man3/EVP_EncryptUpdate.html
    // New code should use EVP_EncryptInit_ex(), EVP_EncryptFinal_ex(), EVP_DecryptInit_ex(), EVP_DecryptFinal_ex(),
    // EVP_CipherInit_ex() and EVP_CipherFinal_ex() because they can reuse an existing context without allocating and freeing it up on each call.
    const EVP_CIPHER* cipherPtr;
    if (keyToWrapLength == 16) {
        cipherPtr = EVP_aes_128_wrap();
    }
    else if (keyToWrapLength == 32) {
        cipherPtr = EVP_aes_256_wrap();
    }
    else {
        LOG_ERROR(subprocess) << "Error Incorrect Key length!!";
        return false;
    }
    //EVP_EncryptInit_ex(), EVP_EncryptUpdate() and EVP_EncryptFinal_ex() return 1 for success and 0 for failure.
    if (!EVP_EncryptInit_ex(ctx, cipherPtr, NULL, keyEncryptionKey, NULL)) {
        ERR_print_errors_fp(stderr);
        return false;
    }

    int tmpOutLength;
    if (!EVP_EncryptUpdate(ctx, wrappedKeyOut, &tmpOutLength, keyToWrap, keyToWrapLength)) {
        ERR_print_errors_fp(stderr);
        return false;
    }
    wrappedKeyOut += tmpOutLength;

    if (!EVP_EncryptFinal_ex(ctx, wrappedKeyOut, &tmpOutLength)) {
        ERR_print_errors_fp(stderr);
        return false;
    }
    wrappedKeyOut += tmpOutLength;

    wrappedKeyOutSize = static_cast<unsigned int>(wrappedKeyOut - wrappedKeyOutBase);
    return wrappedKeyOutSize == (keyEncryptionKeyLength + 8);
#else
    (void)ctxWrapper; //parameter not used
    AES_KEY aesKey;

    //AES_set_encrypt_key() and AES_set_decrypt_key() return 0 for success, -1 if userKey or key is NULL, or -2 if the number of bits is unsupported.
    if (AES_set_encrypt_key(keyEncryptionKey, static_cast<int>(keyEncryptionKeyLength << 3), &aesKey)) {
        return false;
    }

    //definition https://docs.huihoo.com/doxygen/openssl/1.0.1c/aes__wrap_8c_source.html
    //2nd parameter of NULL uses default iv of 0xA6, 0xA6, 0xA6, 0xA6, 0xA6, 0xA6, 0xA6, 0xA6
    //success should return keyEncryptionKeyLength + 8, or -1 if failure
    const int wrappedKeyLength = AES_wrap_key(&aesKey, NULL, wrappedKeyOut, keyToWrap, keyToWrapLength);
    wrappedKeyOutSize = static_cast<unsigned int>(wrappedKeyLength);
    return wrappedKeyLength == (keyEncryptionKeyLength + 8);
#endif
}

bool BPSecManager::AesUnwrapKey(EvpCipherCtxWrapper& ctxWrapper,
    const uint8_t* keyEncryptionKey, const unsigned int keyEncryptionKeyLength,
    const uint8_t* keyToUnwrap, const unsigned int keyToUnwrapLength,
    uint8_t* unwrappedKeyOut, unsigned int& unwrappedKeyOutSize)
{
#ifdef BPSEC_USE_SSL3
    const uint8_t* const unwrappedKeyOutBase = unwrappedKeyOut;
    EVP_CIPHER_CTX* ctx = ctxWrapper.m_pimpl->m_ctx;
    if (ctx == NULL) {
        return false;
    }

    //Used for Legacy purposes only. This flag needed to be set to indicate the cipher handled wrapping.
    EVP_CIPHER_CTX_set_flags(ctx, EVP_CIPHER_CTX_FLAG_WRAP_ALLOW);

    // Set cipher type and mode
    //per https://www.openssl.org/docs/man1.1.1/man3/EVP_EncryptUpdate.html
    // New code should use EVP_EncryptInit_ex(), EVP_EncryptFinal_ex(), EVP_DecryptInit_ex(), EVP_DecryptFinal_ex(),
    // EVP_CipherInit_ex() and EVP_CipherFinal_ex() because they can reuse an existing context without allocating and freeing it up on each call.
    const EVP_CIPHER* cipherPtr;
    if (keyEncryptionKeyLength == 16) {
        cipherPtr = EVP_aes_128_wrap();
    }
    else if (keyEncryptionKeyLength == 32) {
        cipherPtr = EVP_aes_256_wrap();
    }
    else {
        LOG_ERROR(subprocess) << "Error Incorrect Key length!!";
        return false;
    }
    
    if (!EVP_DecryptInit_ex(ctx, cipherPtr, NULL, keyEncryptionKey, NULL)) {
        ERR_print_errors_fp(stderr);
        return false;
    }

    int tmpOutLength;
    if (!EVP_DecryptUpdate(ctx, unwrappedKeyOut, &tmpOutLength, keyToUnwrap, keyToUnwrapLength)) {
        ERR_print_errors_fp(stderr);
        return false;
    }
    unwrappedKeyOut += tmpOutLength;

    if (!EVP_DecryptFinal_ex(ctx, unwrappedKeyOut, &tmpOutLength)) {
        ERR_print_errors_fp(stderr);
        return false;
    }
    unwrappedKeyOut += tmpOutLength;

    unwrappedKeyOutSize = static_cast<unsigned int>(unwrappedKeyOut - unwrappedKeyOutBase);
    return unwrappedKeyOutSize == keyEncryptionKeyLength;
#else
    (void)ctxWrapper; //parameter not used
    AES_KEY aesKey;

    //AES_set_encrypt_key() and AES_set_decrypt_key() return 0 for success, -1 if userKey or key is NULL, or -2 if the number of bits is unsupported.
    if (AES_set_decrypt_key(keyEncryptionKey, static_cast<int>(keyEncryptionKeyLength << 3), &aesKey)) {
        return false;
    }

    //definition https://docs.huihoo.com/doxygen/openssl/1.0.1c/aes__wrap_8c_source.html
    //2nd parameter of NULL uses default iv of 0xA6, 0xA6, 0xA6, 0xA6, 0xA6, 0xA6, 0xA6, 0xA6
    //success should return keyEncryptionKeyLength, or -1 if failure
    const int unwrappedKeyLength = AES_unwrap_key(&aesKey, NULL, unwrappedKeyOut, keyToUnwrap, keyToUnwrapLength);
    unwrappedKeyOutSize = static_cast<unsigned int>(unwrappedKeyLength);
    return unwrappedKeyLength == keyEncryptionKeyLength;
#endif
}


//User of this function provided KEK (key encryption key).
//Bundle provides AES wrapped key, AES variant, IV, tag, and cipherText.
//This function must unwrap key with KEK to get the DEK (data encryption key), then decrypt cipherText.
bool BPSecManager::TryDecryptBundle(EvpCipherCtxWrapper& ctxWrapper,
    EvpCipherCtxWrapper& ctxWrapperForKeyUnwrap,
    BundleViewV7& bv,
    const uint8_t* keyEncryptionKey, const unsigned int keyEncryptionKeyLength, //NULL if not present (for unwrapping DEK only)
    const uint8_t* dataEncryptionKey, const unsigned int dataEncryptionKeyLength, //NULL if not present (when no wrapped key is present)
    ReusableElementsInternal& reusableElementsInternal,
    const bool renderInPlaceWhenFinished)
{
    std::vector<BundleViewV7::Bpv7CanonicalBlockView*>& blocks = reusableElementsInternal.blocks;
    bv.GetCanonicalBlocksByType(BPV7_BLOCK_TYPE_CODE::CONFIDENTIALITY, blocks);
    for (std::size_t i = 0; i < blocks.size(); ++i) {
        BundleViewV7::Bpv7CanonicalBlockView& bcbBlockView = *(blocks[i]);
        Bpv7BlockConfidentialityBlock* bcbPtr = dynamic_cast<Bpv7BlockConfidentialityBlock*>(bcbBlockView.headerPtr.get());
        if (!bcbPtr) {
            return false;
        }
        std::vector<std::vector<uint8_t>*> patPtrs = bcbPtr->GetAllPayloadAuthenticationTagPtrs();
        
        
        bool success;
        COSE_ALGORITHMS variant = bcbPtr->GetSecurityParameterAesVariant(success);
        if (!success) {
            //When not provided, implementations SHOULD assume a value of 3
            //(indicating use of A256GCM), unless an alternate default is
            //established by local security policy at the security source,
            //verifier, or acceptor of this integrity service.
            variant = COSE_ALGORITHMS::A256GCM;
        }
        else if ((variant == COSE_ALGORITHMS::A128GCM) || (variant == COSE_ALGORITHMS::A256GCM)) {
            //ok to continue
        }
        else {
            //invalid variant given
            return false;
        }
        const std::vector<uint8_t>* ivPtr = bcbPtr->GetInitializationVectorPtr();
        if (!ivPtr) {
            return false;
        }

#if BPSEC_MANAGER_PRINT_DEBUG
        {
            std::string ivHexString;
            BinaryConversions::BytesToHexString(*ivPtr, ivHexString);
            boost::to_lower(ivHexString);
            LOG_DEBUG(subprocess) << "iv: " << ivHexString;
        }
#endif

        std::vector<boost::asio::const_buffer>& aadParts = reusableElementsInternal.constBufferVec;
        aadParts.clear();
        aadParts.reserve(4);
        const BPSEC_BCB_AES_GCM_AAD_SCOPE_MASKS scopeMask = bcbPtr->GetSecurityParameterScope();
        const uint8_t scopeMaskAsU8 = static_cast<uint8_t>(scopeMask);
        aadParts.emplace_back(&scopeMaskAsU8, sizeof(scopeMaskAsU8));
        if ((scopeMask & BPSEC_BCB_AES_GCM_AAD_SCOPE_MASKS::INCLUDE_PRIMARY_BLOCK) != BPSEC_BCB_AES_GCM_AAD_SCOPE_MASKS::NO_ADDITIONAL_SCOPE) {
            aadParts.emplace_back(bv.m_primaryBlockView.actualSerializedPrimaryBlockPtr);
        }
        boost::asio::const_buffer* targetHeaderAadPiece = NULL;
        if ((scopeMask & BPSEC_BCB_AES_GCM_AAD_SCOPE_MASKS::INCLUDE_TARGET_HEADER) != BPSEC_BCB_AES_GCM_AAD_SCOPE_MASKS::NO_ADDITIONAL_SCOPE) {
            aadParts.emplace_back(); //placeholder
            targetHeaderAadPiece = &aadParts.back();
        }
        if ((scopeMask & BPSEC_BCB_AES_GCM_AAD_SCOPE_MASKS::INCLUDE_SECURITY_HEADER) != BPSEC_BCB_AES_GCM_AAD_SCOPE_MASKS::NO_ADDITIONAL_SCOPE) {
            const uint8_t * const startPtr = (reinterpret_cast<const uint8_t*>(bcbBlockView.actualSerializedBlockPtr.data())) + 1;
            const Bpv7CanonicalBlock& blk = *bcbBlockView.headerPtr;
            const std::size_t len = blk.GetSerializationSizeOfAadPart();
            aadParts.emplace_back(startPtr, len);
        }
        
        const uint8_t* dataEncryptionKeyToUse;
        unsigned int dataEncryptionKeySizeToUse;
        uint8_t unwrappedKeyBytes[32 + 10]; //32 worse case for 32*8=256bit
        unsigned int unwrappedKeyOutSize;
        const std::vector<uint8_t>* wrappedKeyPtr = bcbPtr->GetAesWrappedKeyPtr();
        if (wrappedKeyPtr) {
            //When an AES-KW wrapped key is present in a security block, it is
            //assumed that security verifiers and security acceptors can
            //independently determine the KEK used in the wrapping of the symmetric
            //AES content-encrypting key.
            if (!keyEncryptionKey) {
                //no KEK present, can't unwrap key
                return false;
            }
            //unwrap key:
            if (!BPSecManager::AesUnwrapKey(ctxWrapperForKeyUnwrap,
                keyEncryptionKey, keyEncryptionKeyLength,
                wrappedKeyPtr->data(), static_cast<const unsigned int>(wrappedKeyPtr->size()),
                unwrappedKeyBytes, unwrappedKeyOutSize))
            {
                return false;
            }
            dataEncryptionKeyToUse = unwrappedKeyBytes;
            dataEncryptionKeySizeToUse = unwrappedKeyOutSize;
        }
        else {
            if (!dataEncryptionKey) {
                //no DEK present, can't decrypt anything
                return false;
            }
            dataEncryptionKeyToUse = dataEncryptionKey;
            dataEncryptionKeySizeToUse = dataEncryptionKeyLength;
        }

        
        const Bpv7AbstractSecurityBlock::security_targets_t& targets = bcbPtr->m_securityTargets;

        
        //The target results MUST be ordered
        //identically to the Security Targets field of the security
        //block.  This means that the first set of target results in this
        //array corresponds to the first entry in the Security Targets
        //field of the security block, and so on.  There MUST be one
        //entry in this array for each entry in the Security Targets
        //field of the security block.
        //(payload authentication tag is the only result)
        if (patPtrs.size() != targets.size()) {
            return false;
        }

        if (targets.empty()) {
            return false;
        }
        
        for (std::size_t stI = 0; stI < targets.size(); ++stI) {
            const uint64_t target = targets[stI];
            const std::vector<uint8_t>& tag = *(patPtrs[stI]);
            BundleViewV7::Bpv7CanonicalBlockView* targetCanonicalBlockViewPtr = bv.GetCanonicalBlockByBlockNumber(target);
            if (!targetCanonicalBlockViewPtr) {
                return false;
            }
            Bpv7CanonicalBlock& targetCanonicalHeader = *(targetCanonicalBlockViewPtr->headerPtr);
            if (targetHeaderAadPiece) {
                const uint8_t* const startPtr = (reinterpret_cast<const uint8_t*>(targetCanonicalBlockViewPtr->actualSerializedBlockPtr.data())) + 1;
                const std::size_t len = targetCanonicalHeader.GetSerializationSizeOfAadPart();
                *targetHeaderAadPiece = boost::asio::const_buffer(startPtr, len);
            }
#ifdef BPSEC_MANAGER_PRINT_DEBUG
            {
                std::string aadHexString;
                BinaryConversions::BytesToHexString(aadParts, aadHexString);
                boost::to_lower(aadHexString);
                LOG_DEBUG(subprocess) << "aad: " << aadHexString;
            }
#endif
#ifdef BPSEC_MANAGER_PRINT_DEBUG
            {
                std::string hexString;
                BinaryConversions::BytesToHexString(targetCanonicalHeader.m_dataPtr, targetCanonicalHeader.m_dataLength, hexString);
                boost::to_lower(hexString);
                LOG_DEBUG(subprocess) << "block (code=" << (int)targetCanonicalHeader.m_blockTypeCode << ") data part before decrypt : " << hexString;
            }
#endif
            
            //overwrite cyphertext with plaintext in-place and compute crc
            uint64_t decryptedDataOutSize;
            //inplace (same in and out buffers)
            if (!BPSecManager::AesGcmDecrypt(ctxWrapper,
                targetCanonicalHeader.m_dataPtr, targetCanonicalHeader.m_dataLength,
                dataEncryptionKeyToUse, dataEncryptionKeySizeToUse,
                ivPtr->data(), ivPtr->size(),
                aadParts, //affects tag only
                tag.data(),
                targetCanonicalHeader.m_dataPtr, decryptedDataOutSize))
            {
                return false;
            }
#ifdef BPSEC_MANAGER_PRINT_DEBUG
            {
                std::string hexString;
                BinaryConversions::BytesToHexString(targetCanonicalHeader.m_dataPtr, targetCanonicalHeader.m_dataLength, hexString);
                boost::to_lower(hexString);
                LOG_DEBUG(subprocess) << "block data part decrypted: " << hexString;
            }
#endif
            //RFC9173:
            //The BCB-AES-GCM security context replaces the block-type-specific
            //data field of its security target with ciphertext generated using the
            //Advanced Encryption Standard (AES) cipher operating in Galois/Counter
            //Mode (GCM) [AES-GCM].  The use of AES-GCM was selected as the cipher
            //suite for this confidentiality mechanism for several reasons:
            //  3. The use of the Galois/Counter Mode produces ciphertext with the
            //     same size as the plaintext making the replacement of target block
            //     information easier as length fields do not need to be changed.
            if (targetCanonicalHeader.m_dataLength != decryptedDataOutSize) {
                return false;
            }

            //recompute crc at end
            targetCanonicalHeader.RecomputeCrcAfterDataModification(
                (uint8_t*)targetCanonicalBlockViewPtr->actualSerializedBlockPtr.data(),
                targetCanonicalBlockViewPtr->actualSerializedBlockPtr.size()); //recompute crc

            //parse now-unencrypted block
            targetCanonicalBlockViewPtr->isEncrypted = false;
            if (!targetCanonicalHeader.Virtual_DeserializeExtensionBlockDataBpv7()) { //requires m_dataPtr and m_dataLength to be set (which should be already done)
                return false;
            }

#ifdef BPSEC_MANAGER_PRINT_DEBUG
            {
                std::string hexString;
                BinaryConversions::BytesToHexString(targetCanonicalBlockViewPtr->actualSerializedBlockPtr.data(), targetCanonicalBlockViewPtr->actualSerializedBlockPtr.size(), hexString);
                boost::to_lower(hexString);
                LOG_DEBUG(subprocess) << "block decrypted: " << hexString;
            }
#endif
            
            
        }
        bcbBlockView.markedForDeletion = true;
    }
    //at least one bcb was marked for deletion, so rerender
    if (renderInPlaceWhenFinished) {
        return bv.RenderInPlace(PaddedMallocator<uint8_t>::PADDING_ELEMENTS_BEFORE);
    }
    return true;
}

//User of this function provided KEK (key encryption key), AAD scope, AES variant, IV, and targets.
//Function generates AES wrapped key, tag, and cipherText.
//Bundle provides AAD data.
//This function must unwrap key with KEK to get the DEK (data encryption key), then decrypt cipherText.
bool BPSecManager::TryEncryptBundle(EvpCipherCtxWrapper& ctxWrapper,
    EvpCipherCtxWrapper& ctxWrapperForKeyWrap,
    BundleViewV7& bv,
    BPSEC_BCB_AES_GCM_AAD_SCOPE_MASKS aadScopeMask,
    COSE_ALGORITHMS aesVariant,
    BPV7_CRC_TYPE bcbCrcType,
    const cbhe_eid_t& securitySource,
    const uint64_t* targetBlockNumbers, const unsigned int numTargetBlockNumbers,
    const uint8_t* iv, const unsigned int ivLength,
    const uint8_t* keyEncryptionKey, const unsigned int keyEncryptionKeyLength, //NULL if not present (for wrapping DEK only)
    const uint8_t* dataEncryptionKey, const unsigned int dataEncryptionKeyLength, //NULL if not present (when no wrapped key is present)
    ReusableElementsInternal& reusableElementsInternal,
    const uint64_t* insertBcbBeforeThisBlockNumberIfNotNull,
    const bool renderInPlaceWhenFinished)
{
    std::vector<BundleViewV7::Bpv7CanonicalBlockView*>& blocks = reusableElementsInternal.blocks;

    std::unique_ptr<Bpv7CanonicalBlock> blockPtr = boost::make_unique<Bpv7BlockConfidentialityBlock>();
    Bpv7BlockConfidentialityBlock& bcb = *(reinterpret_cast<Bpv7BlockConfidentialityBlock*>(blockPtr.get()));

    

    bcb.m_blockNumber = bv.GetNextFreeCanonicalBlockNumber();
    bcb.m_crcType = bcbCrcType;
    Bpv7AbstractSecurityBlock::security_targets_t& securityTargets = bcb.m_securityTargets;
    securityTargets.assign(targetBlockNumbers, targetBlockNumbers + numTargetBlockNumbers);
    const bool doesTargetPayload = (std::find(securityTargets.begin(), securityTargets.end(), 1) != securityTargets.end());

    //BCBs MUST have the "Block must be replicated in every fragment"
    //flag set if one of the targets is the payload block.  Having
    //that BCB in each fragment indicates to a receiving node that
    //the payload portion of each fragment represents ciphertext.
    bcb.m_blockProcessingControlFlags = (doesTargetPayload) ? BPV7_BLOCKFLAG::MUST_BE_REPLICATED : BPV7_BLOCKFLAG::NO_FLAGS_SET;

    //bcb.m_securityContextId = static_cast<uint64_t>(BPSEC_SECURITY_CONTEXT_IDENTIFIERS::BCB_AES_GCM); //handled by constructor
    bcb.m_securityContextFlags = 0;
    bcb.SetSecurityContextParametersPresent();
    bcb.m_securitySource = securitySource;
    
    std::vector<uint8_t>* ivPtr = bcb.AddAndGetInitializationVectorPtr();
    ivPtr->assign(iv, iv + ivLength);

    if (!bcb.AddOrUpdateSecurityParameterAesVariant(aesVariant)) {
        return false;
    }


    std::vector<boost::asio::const_buffer>& aadParts = reusableElementsInternal.constBufferVec;
    aadParts.clear();
    aadParts.reserve(4);
    const uint8_t scopeMaskAsU8 = static_cast<uint8_t>(aadScopeMask);
    aadParts.emplace_back(&scopeMaskAsU8, sizeof(scopeMaskAsU8));
    if ((aadScopeMask & BPSEC_BCB_AES_GCM_AAD_SCOPE_MASKS::INCLUDE_PRIMARY_BLOCK) != BPSEC_BCB_AES_GCM_AAD_SCOPE_MASKS::NO_ADDITIONAL_SCOPE) {
        aadParts.emplace_back(bv.m_primaryBlockView.actualSerializedPrimaryBlockPtr);
    }
    boost::asio::const_buffer* targetHeaderAadPiece = NULL;
    if ((aadScopeMask & BPSEC_BCB_AES_GCM_AAD_SCOPE_MASKS::INCLUDE_TARGET_HEADER) != BPSEC_BCB_AES_GCM_AAD_SCOPE_MASKS::NO_ADDITIONAL_SCOPE) {
        aadParts.emplace_back(); //placeholder
        targetHeaderAadPiece = &aadParts.back();
    }
    uint8_t securityHeaderAadSerialization[3*9];
    if ((aadScopeMask & BPSEC_BCB_AES_GCM_AAD_SCOPE_MASKS::INCLUDE_SECURITY_HEADER) != BPSEC_BCB_AES_GCM_AAD_SCOPE_MASKS::NO_ADDITIONAL_SCOPE) {
        //m_blockTypeCode, m_blockNumber, and m_blockProcessingControlFlags must be set prior to this call
        std::size_t len = static_cast<std::size_t>(bcb.SerializeAadPart(securityHeaderAadSerialization));
        aadParts.emplace_back(securityHeaderAadSerialization, len);
    }

    if (!dataEncryptionKey) {
        //no DEK present
        return false;
    }
    if (keyEncryptionKey) { //will be wrapping the DEK
        std::vector<uint8_t>* wrappedKeyPtr = bcb.AddAndGetAesWrappedKeyPtr();
        wrappedKeyPtr->resize(32 + 10); //32+8 worse case for 32*8=256bit (KEKlen + 8)
        unsigned int wrappedKeyOutSize;
        if (!BPSecManager::AesWrapKey(ctxWrapperForKeyWrap,
            keyEncryptionKey, keyEncryptionKeyLength,
            dataEncryptionKey, dataEncryptionKeyLength, //Wrapping this key
            wrappedKeyPtr->data(), wrappedKeyOutSize))
        {
            return false;
        }
        wrappedKeyPtr->resize(wrappedKeyOutSize);
    }

    //do this after the key wrapping so the results appear in order and match unit tests
    if (!bcb.AddSecurityParameterScope(aadScopeMask)) {
        return false;
    }

    if (securityTargets.empty()) {
        return false;
    }

    for (std::size_t stI = 0; stI < securityTargets.size(); ++stI) {
        const uint64_t target = securityTargets[stI];
        BundleViewV7::Bpv7CanonicalBlockView* targetCanonicalBlockViewPtr = bv.GetCanonicalBlockByBlockNumber(target);
        if (!targetCanonicalBlockViewPtr) {
            return false;
        }
        if (targetCanonicalBlockViewPtr->dirty || (!targetCanonicalBlockViewPtr->actualSerializedBlockPtr.data())) { //must be rendered
            return false;
        }
        Bpv7CanonicalBlock& targetCanonicalHeader = *(targetCanonicalBlockViewPtr->headerPtr);
        if (targetHeaderAadPiece) {
            const uint8_t* const startPtr = (reinterpret_cast<const uint8_t*>(targetCanonicalBlockViewPtr->actualSerializedBlockPtr.data())) + 1;
            const std::size_t len = targetCanonicalHeader.GetSerializationSizeOfAadPart();
            *targetHeaderAadPiece = boost::asio::const_buffer(startPtr, len);
        }

        //The target results MUST be ordered
        //identically to the Security Targets field of the security
        //block.  This means that the first set of target results in this
        //array corresponds to the first entry in the Security Targets
        //field of the security block, and so on.  There MUST be one
        //entry in this array for each entry in the Security Targets
        //field of the security block.
        //(payload authentication tag is the only result)
        std::vector<uint8_t>* tagPtr = bcb.AppendAndGetPayloadAuthenticationTagPtr();

        //Regardless of the variant, the generated authentication tag MUST
        //always be 128 bits.
        tagPtr->resize(EVP_GCM_TLS_TAG_LEN);

        //overwrite plaintext with cyphertext in-place and compute crc
        uint64_t encryptedDataOutSize;
        //inplace (same in and out buffers)
        if (!BPSecManager::AesGcmEncrypt(ctxWrapper,
            targetCanonicalHeader.m_dataPtr, targetCanonicalHeader.m_dataLength,
            dataEncryptionKey, dataEncryptionKeyLength,
            ivPtr->data(), ivPtr->size(),
            aadParts, //affects tag only
            targetCanonicalHeader.m_dataPtr, encryptedDataOutSize,
            tagPtr->data()))
        {
            return false;
        }

        //RFC9173:
        //The BCB-AES-GCM security context replaces the block-type-specific
        //data field of its security target with ciphertext generated using the
        //Advanced Encryption Standard (AES) cipher operating in Galois/Counter
        //Mode (GCM) [AES-GCM].  The use of AES-GCM was selected as the cipher
        //suite for this confidentiality mechanism for several reasons:
        //  3. The use of the Galois/Counter Mode produces ciphertext with the
        //     same size as the plaintext making the replacement of target block
        //     information easier as length fields do not need to be changed.
        if (targetCanonicalHeader.m_dataLength != encryptedDataOutSize) {
            return false;
        }

        //recompute crc at end
        targetCanonicalHeader.RecomputeCrcAfterDataModification(
            (uint8_t*)targetCanonicalBlockViewPtr->actualSerializedBlockPtr.data(),
            targetCanonicalBlockViewPtr->actualSerializedBlockPtr.size()); //recompute crc

        targetCanonicalBlockViewPtr->isEncrypted = true;
    }

    //at least one bcb was added, so rerender
    if (insertBcbBeforeThisBlockNumberIfNotNull) { //for matching unit test examples
        bv.InsertMoveCanonicalBlockBeforeBlockNumber(std::move(blockPtr), *insertBcbBeforeThisBlockNumberIfNotNull);
    }
    else {
        bv.PrependMoveCanonicalBlock(std::move(blockPtr));
    }
    if (renderInPlaceWhenFinished) {
        return bv.RenderInPlace(PaddedMallocator<uint8_t>::PADDING_ELEMENTS_BEFORE);
    }
    return true;
}

//User of this function provided KEK (key encryption key).
//Bundle provides AES wrapped key, AES variant, IV, tag, and cipherText.
//This function must unwrap key with KEK to get the DEK (data encryption key), then decrypt cipherText.
bool BPSecManager::TryVerifyBundleIntegrity(HmacCtxWrapper& ctxWrapper,
    EvpCipherCtxWrapper& ctxWrapperForKeyUnwrap,
    BundleViewV7& bv,
    const uint8_t* keyEncryptionKey, const unsigned int keyEncryptionKeyLength, //NULL if not present (for unwrapping hmac key only)
    const uint8_t* hmacKey, const unsigned int hmacKeyLength, //NULL if not present (when no wrapped key is present)
    ReusableElementsInternal& reusableElementsInternal,
    const bool markBibForDeletion,
    const bool renderInPlaceWhenFinished)
{
    std::vector<BundleViewV7::Bpv7CanonicalBlockView*>& blocks = reusableElementsInternal.blocks;
    uint8_t primaryByteStringHeader[10]; //must be at least 9
    bv.GetCanonicalBlocksByType(BPV7_BLOCK_TYPE_CODE::INTEGRITY, blocks);
    for (std::size_t i = 0; i < blocks.size(); ++i) {
        BundleViewV7::Bpv7CanonicalBlockView& bibBlockView = *(blocks[i]);
        if (bibBlockView.isEncrypted) {
            return false;
        }
        Bpv7BlockIntegrityBlock* bibPtr = dynamic_cast<Bpv7BlockIntegrityBlock*>(bibBlockView.headerPtr.get());
        if (!bibPtr) {
            return false;
        }
        

        bool success;
        COSE_ALGORITHMS variant = bibPtr->GetSecurityParameterShaVariant(success);
        if (!success) {
            //When not provided, implementations SHOULD assume a value of 6
            //(indicating use of HMAC 384/384), unless an alternate default is
            //established by local security policy at the security source,
            //verifiers, or acceptor of this integrity service.
            variant = COSE_ALGORITHMS::HMAC_384_384;
        }
        else if ((variant == COSE_ALGORITHMS::HMAC_512_512) || (variant == COSE_ALGORITHMS::HMAC_384_384) || (variant == COSE_ALGORITHMS::HMAC_256_256)) {
            //ok to continue
        }
        else {
            //invalid variant given
            return false;
        }
        

        std::vector<boost::asio::const_buffer>& ipptParts = reusableElementsInternal.constBufferVec;
        ipptParts.clear();
        ipptParts.reserve(5); //5 parts per RFC9173 - 3.7.  Canonicalization Algorithms
        const BPSEC_BIB_HMAC_SHA2_INTEGRITY_SCOPE_MASKS scopeMask = bibPtr->GetSecurityParameterScope();
        const uint8_t scopeMaskAsU8 = static_cast<uint8_t>(scopeMask);
        ipptParts.emplace_back(&scopeMaskAsU8, sizeof(scopeMaskAsU8)); //the only one guaranteed to be present and not change

        

        const uint8_t* hmacKeyToUse;
        unsigned int hmacKeySizeToUse;
        uint8_t unwrappedKeyBytes[32 + 10]; //32 worse case for 32*8=256bit
        unsigned int unwrappedKeyOutSize;
        const std::vector<uint8_t>* wrappedKeyPtr = bibPtr->GetAesWrappedKeyPtr();
        if (wrappedKeyPtr) {
            //When an AES-KW wrapped key is present in a security block, it is
            //assumed that security verifiers and security acceptors can
            //independently determine the KEK used in the wrapping of the symmetric
            //AES content-encrypting key.
            if (!keyEncryptionKey) {
                //no KEK present, can't unwrap key
                return false;
            }
            //unwrap key:
            if (!BPSecManager::AesUnwrapKey(ctxWrapperForKeyUnwrap,
                keyEncryptionKey, keyEncryptionKeyLength,
                wrappedKeyPtr->data(), static_cast<const unsigned int>(wrappedKeyPtr->size()),
                unwrappedKeyBytes, unwrappedKeyOutSize))
            {
                return false;
            }
            hmacKeyToUse = unwrappedKeyBytes;
            hmacKeySizeToUse = unwrappedKeyOutSize;
        }
        else {
            if (!hmacKey) {
                //no hmacKey present, can't verify anything
                return false;
            }
            hmacKeyToUse = hmacKey;
            hmacKeySizeToUse = hmacKeyLength;
        }


        const Bpv7AbstractSecurityBlock::security_targets_t& targets = bibPtr->m_securityTargets;

        std::vector<std::vector<uint8_t>*> expectedHmacPtrs = bibPtr->GetAllExpectedHmacPtrs();

        //The target results MUST be ordered
        //identically to the Security Targets field of the security
        //block.  This means that the first set of target results in this
        //array corresponds to the first entry in the Security Targets
        //field of the security block, and so on.  There MUST be one
        //entry in this array for each entry in the Security Targets
        //field of the security block.
        //(hmac is the only result)
        if (expectedHmacPtrs.size() != targets.size()) {
            return false;
        }

        if (targets.empty()) {
            return false;
        }

        for (std::size_t stI = 0; stI < targets.size(); ++stI) {
            const uint64_t target = targets[stI];
            const std::vector<uint8_t>* expectedHmacPtr = expectedHmacPtrs[stI];
            if (!expectedHmacPtr) {
                return false; //null for some reason
            }
            const std::vector<uint8_t>& expectedHmac = *expectedHmacPtr;
            ipptParts.resize(1); //ipptParts[0] is the scopeMask itself which is constant
            BundleViewV7::Bpv7CanonicalBlockView* targetCanonicalBlockViewPtr = NULL;
            if (target != 0) {
                targetCanonicalBlockViewPtr = bv.GetCanonicalBlockByBlockNumber(target);
                if (!targetCanonicalBlockViewPtr) {
                    return false;
                }
                if (targetCanonicalBlockViewPtr->isEncrypted) {
                    return false;
                }
            }
            if (targetCanonicalBlockViewPtr) { //targetIsNotPrimary
                //NOTE: When the security target is the bundle's primary block,
                //the canonicalization steps associated with the primary block
                //flag and the target header flag are skipped.  Skipping primary
                //block flag processing, in this case, avoids adding the bundle's
                //primary block twice in the IPPT calculation.  Skipping target
                //header flag processing, in this case, is necessary because the
                //primary block of a bundle does not have the expected elements
                //of a block header such as block number and block processing
                //control flags.
                if ((scopeMask & BPSEC_BIB_HMAC_SHA2_INTEGRITY_SCOPE_MASKS::INCLUDE_PRIMARY_BLOCK) != BPSEC_BIB_HMAC_SHA2_INTEGRITY_SCOPE_MASKS::NO_ADDITIONAL_SCOPE) {
                    ipptParts.emplace_back(bv.m_primaryBlockView.actualSerializedPrimaryBlockPtr);
                }
                if ((scopeMask & BPSEC_BIB_HMAC_SHA2_INTEGRITY_SCOPE_MASKS::INCLUDE_TARGET_HEADER) != BPSEC_BIB_HMAC_SHA2_INTEGRITY_SCOPE_MASKS::NO_ADDITIONAL_SCOPE) {
                    const uint8_t* const startPtr = (reinterpret_cast<const uint8_t*>(targetCanonicalBlockViewPtr->actualSerializedBlockPtr.data())) + 1;
                    const std::size_t len = targetCanonicalBlockViewPtr->headerPtr->GetSerializationSizeOfAadPart(); //target is already rendered in this case                    
                    ipptParts.emplace_back(startPtr, len);
                }
            }
            if ((scopeMask & BPSEC_BIB_HMAC_SHA2_INTEGRITY_SCOPE_MASKS::INCLUDE_SECURITY_HEADER) != BPSEC_BIB_HMAC_SHA2_INTEGRITY_SCOPE_MASKS::NO_ADDITIONAL_SCOPE) {
                const uint8_t* const startPtr = (reinterpret_cast<const uint8_t*>(bibBlockView.actualSerializedBlockPtr.data())) + 1;
                const std::size_t len = bibBlockView.headerPtr->GetSerializationSizeOfAadPart(); //bib is already rendered in this case 
                ipptParts.emplace_back(startPtr, len);
            }
            if (targetCanonicalBlockViewPtr) { //targetIsNotPrimary, so this is the canonical form of the block-type-specific data of the security target
                //The canonical blocks block-type-specific data is already in the form of a CBOR byte string,
                //and therefore the byte string header is already present in the rendered encoding,
                //so just back up the pointer by the encoding length to include this byte-string header with the block-type-specific data.
                const Bpv7CanonicalBlock& targetBlock = *(targetCanonicalBlockViewPtr->headerPtr);
                const uint64_t cborByteStringHeaderLength = CborGetEncodingSizeU64(targetBlock.m_dataLength);
                ipptParts.emplace_back(targetBlock.m_dataPtr - cborByteStringHeaderLength, targetBlock.m_dataLength + cborByteStringHeaderLength);
            }
            else { //target is primary, so this is the canonical form of the primary block
                //The canonical form of the primary block is as specified in [RFC9171]
                //with the following constraint.
                // *  CBOR values from the primary block MUST be canonicalized using the
                //    rules for Deterministically Encoded CBOR, as specified in [RFC8949].
                //
                //The canonical blocks block-type-specific data is already in the form of a CBOR byte string,
                //but the primary is not in byte string format, so a byte-string header must be generated.
                const boost::asio::const_buffer& cbPrimary = bv.m_primaryBlockView.actualSerializedPrimaryBlockPtr;
                const uint64_t cborByteStringHeaderLength = CborEncodeU64BufSize9(primaryByteStringHeader, cbPrimary.size());
                primaryByteStringHeader[0] |= (2U << 5); //change from major type 0 (unsigned integer) to major type 2 (byte string)
                ipptParts.emplace_back(primaryByteStringHeader, cborByteStringHeaderLength);
                ipptParts.emplace_back(cbPrimary);
            }
#ifdef BPSEC_MANAGER_PRINT_DEBUG
            {
                std::string ipptHexString;
                BinaryConversions::BytesToHexString(ipptParts, ipptHexString);
                boost::to_lower(ipptHexString);
                LOG_DEBUG(subprocess) << "ippt: " << ipptHexString;
            }
#endif

            uint8_t messageDigestCalculated[64 + 10]; //64*8 = 512bits worse case (with some extra padding)
            unsigned int messageDigestOutSize;
            //not inplace test (separate in and out buffers)
            if (!BPSecManager::HmacSha(ctxWrapper,
                variant,
                ipptParts,
                hmacKeyToUse, hmacKeySizeToUse,
                messageDigestCalculated, messageDigestOutSize))
            {
                return false;
            }
            //expectedHmac

            
#ifdef BPSEC_MANAGER_PRINT_DEBUG
            LOG_DEBUG(subprocess) << "target block number " << target << ":";
            {
                std::string hexString;
                BinaryConversions::BytesToHexString(expectedHmac, hexString);
                boost::to_lower(hexString);
                LOG_DEBUG(subprocess) << "  expectedHmac: " << hexString;
            }
            {
                std::string hexString;
                BinaryConversions::BytesToHexString(messageDigestCalculated, messageDigestOutSize, hexString);
                boost::to_lower(hexString);
                LOG_DEBUG(subprocess) << "  calculatedHmac: " << hexString;
            }
#endif
            
            if (messageDigestOutSize != expectedHmac.size()) {
                return false;
            }
            if (memcmp(expectedHmac.data(), messageDigestCalculated, messageDigestOutSize) != 0) {
                return false;
            }
            


        }
        if (markBibForDeletion) {
            bibBlockView.markedForDeletion = true;
        }
    }
    if (markBibForDeletion && renderInPlaceWhenFinished) {
        //at least one bib was marked for deletion, so rerender
        return bv.RenderInPlace(PaddedMallocator<uint8_t>::PADDING_ELEMENTS_BEFORE);
    }
    return true;
}

bool BPSecManager::TryAddBundleIntegrity(HmacCtxWrapper& ctxWrapper,
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
    const bool renderInPlaceWhenFinished)
{
    const uint8_t variantMinus5 = (static_cast<uint8_t>(variant)) - 5u;
    if (variantMinus5 > 2) { //not 0, 1, or 2
        return false;
    }

    uint8_t primaryByteStringHeader[10]; //must be at least 9
    std::vector<BundleViewV7::Bpv7CanonicalBlockView*>& blocks = reusableElementsInternal.blocks;

    std::unique_ptr<Bpv7CanonicalBlock> blockPtr = boost::make_unique<Bpv7BlockIntegrityBlock>();
    Bpv7BlockIntegrityBlock& bib = *(reinterpret_cast<Bpv7BlockIntegrityBlock*>(blockPtr.get()));



    bib.m_blockNumber = bv.GetNextFreeCanonicalBlockNumber();
    bib.m_crcType = bibCrcType;
    Bpv7AbstractSecurityBlock::security_targets_t& securityTargets = bib.m_securityTargets;
    securityTargets.assign(targetBlockNumbers, targetBlockNumbers + numTargetBlockNumbers);
    
    bib.m_blockProcessingControlFlags = BPV7_BLOCKFLAG::NO_FLAGS_SET;

    //bcb.m_securityContextId = static_cast<uint64_t>(BPSEC_SECURITY_CONTEXT_IDENTIFIERS::BCB_AES_GCM); //handled by constructor
    bib.m_securityContextFlags = 0;
    bib.SetSecurityContextParametersPresent();
    bib.m_securitySource = securitySource;

    
    if (!bib.AddOrUpdateSecurityParameterShaVariant(variant)) {
        return false;
    }

    std::vector<boost::asio::const_buffer>& ipptParts = reusableElementsInternal.constBufferVec;
    ipptParts.clear();
    ipptParts.reserve(5); //5 parts per RFC9173 - 3.7.  Canonicalization Algorithms
    const uint8_t scopeMaskAsU8 = static_cast<uint8_t>(integrityScopeMask);
    ipptParts.emplace_back(&scopeMaskAsU8, sizeof(scopeMaskAsU8)); //the only one guaranteed to be present and not change


    uint8_t securityHeaderIpptSerialization[3 * 9];
    std::size_t securityHeaderIpptSerializationLength = 0; //used also as a flag
    if ((integrityScopeMask & BPSEC_BIB_HMAC_SHA2_INTEGRITY_SCOPE_MASKS::INCLUDE_SECURITY_HEADER) != BPSEC_BIB_HMAC_SHA2_INTEGRITY_SCOPE_MASKS::NO_ADDITIONAL_SCOPE) {
        //m_blockTypeCode, m_blockNumber, and m_blockProcessingControlFlags must be set prior to this call
        securityHeaderIpptSerializationLength = static_cast<std::size_t>(bib.SerializeAadPart(securityHeaderIpptSerialization)); //this block is not serialized yet
    }

    
    if (!hmacKey) {
        //no key present
        return false;
    }
    if (keyEncryptionKey) { //will be wrapping the DEK
        std::vector<uint8_t>* wrappedKeyPtr = bib.AddAndGetAesWrappedKeyPtr();
        wrappedKeyPtr->resize(32 + 10); //32+8 worse case for 32*8=256bit (KEKlen + 8)
        unsigned int wrappedKeyOutSize;
        if (!BPSecManager::AesWrapKey(ctxWrapperForKeyWrap,
            keyEncryptionKey, keyEncryptionKeyLength,
            hmacKey, hmacKeyLength, //Wrapping this key
            wrappedKeyPtr->data(), wrappedKeyOutSize))
        {
            return false;
        }
        wrappedKeyPtr->resize(wrappedKeyOutSize);
    }

    //do this after the key wrapping so the results appear in order and match unit tests
    if (!bib.AddSecurityParameterIntegrityScope(integrityScopeMask)) {
        return false;
    }

    if (securityTargets.empty()) {
        return false;
    }

    for (std::size_t stI = 0; stI < securityTargets.size(); ++stI) {
        const uint64_t target = securityTargets[stI];
        ipptParts.resize(1); //ipptParts[0] is the scopeMask itself which is constant
        BundleViewV7::Bpv7CanonicalBlockView* targetCanonicalBlockViewPtr = NULL;
        if (target != 0) {
            targetCanonicalBlockViewPtr = bv.GetCanonicalBlockByBlockNumber(target);
            if (!targetCanonicalBlockViewPtr) {
                return false;
            }
            //RFC9172 3.1 - BIBs are never used for
            //integrity protection of the ciphertext provided by a BCB.
            if (targetCanonicalBlockViewPtr->isEncrypted) {
                return false;
            }
        }
        if (targetCanonicalBlockViewPtr) { //targetIsNotPrimary
            //NOTE: When the security target is the bundle's primary block,
            //the canonicalization steps associated with the primary block
            //flag and the target header flag are skipped.  Skipping primary
            //block flag processing, in this case, avoids adding the bundle's
            //primary block twice in the IPPT calculation.  Skipping target
            //header flag processing, in this case, is necessary because the
            //primary block of a bundle does not have the expected elements
            //of a block header such as block number and block processing
            //control flags.
            if ((integrityScopeMask & BPSEC_BIB_HMAC_SHA2_INTEGRITY_SCOPE_MASKS::INCLUDE_PRIMARY_BLOCK) != BPSEC_BIB_HMAC_SHA2_INTEGRITY_SCOPE_MASKS::NO_ADDITIONAL_SCOPE) {
                ipptParts.emplace_back(bv.m_primaryBlockView.actualSerializedPrimaryBlockPtr);
            }
            if ((integrityScopeMask & BPSEC_BIB_HMAC_SHA2_INTEGRITY_SCOPE_MASKS::INCLUDE_TARGET_HEADER) != BPSEC_BIB_HMAC_SHA2_INTEGRITY_SCOPE_MASKS::NO_ADDITIONAL_SCOPE) {
                const uint8_t* const startPtr = (reinterpret_cast<const uint8_t*>(targetCanonicalBlockViewPtr->actualSerializedBlockPtr.data())) + 1;
                const std::size_t len = targetCanonicalBlockViewPtr->headerPtr->GetSerializationSizeOfAadPart(); //target is already rendered in this case                    
                ipptParts.emplace_back(startPtr, len);
            }
        }
        if (securityHeaderIpptSerializationLength) { //INCLUDE_SECURITY_HEADER
            ipptParts.emplace_back(securityHeaderIpptSerialization, securityHeaderIpptSerializationLength);
        }
        if (targetCanonicalBlockViewPtr) { //targetIsNotPrimary, so this is the canonical form of the block-type-specific data of the security target
            //The canonical blocks block-type-specific data is already in the form of a CBOR byte string,
            //and therefore the byte string header is already present in the rendered encoding,
            //so just back up the pointer by the encoding length to include this byte-string header with the block-type-specific data.
            const Bpv7CanonicalBlock& targetBlock = *(targetCanonicalBlockViewPtr->headerPtr);
            const uint64_t cborByteStringHeaderLength = CborGetEncodingSizeU64(targetBlock.m_dataLength);
            ipptParts.emplace_back(targetBlock.m_dataPtr - cborByteStringHeaderLength, targetBlock.m_dataLength + cborByteStringHeaderLength);
        }
        else { //target is primary, so this is the canonical form of the primary block
            //The canonical form of the primary block is as specified in [RFC9171]
            //with the following constraint.
            // *  CBOR values from the primary block MUST be canonicalized using the
            //    rules for Deterministically Encoded CBOR, as specified in [RFC8949].
            //
            //The canonical blocks block-type-specific data is already in the form of a CBOR byte string,
            //but the primary is not in byte string format, so a byte-string header must be generated.
            const boost::asio::const_buffer& cbPrimary = bv.m_primaryBlockView.actualSerializedPrimaryBlockPtr;
            const uint64_t cborByteStringHeaderLength = CborEncodeU64BufSize9(primaryByteStringHeader, cbPrimary.size());
            primaryByteStringHeader[0] |= (2U << 5); //change from major type 0 (unsigned integer) to major type 2 (byte string)
            ipptParts.emplace_back(primaryByteStringHeader, cborByteStringHeaderLength);
            ipptParts.emplace_back(cbPrimary);
        }

#ifdef BPSEC_MANAGER_PRINT_DEBUG
        {
            std::string ipptHexString;
            BinaryConversions::BytesToHexString(ipptParts, ipptHexString);
            boost::to_lower(ipptHexString);
            LOG_DEBUG(subprocess) << "ippt: " << ipptHexString;
        }
#endif


        //The target results MUST be ordered
        //identically to the Security Targets field of the security
        //block.  This means that the first set of target results in this
        //array corresponds to the first entry in the Security Targets
        //field of the security block, and so on.  There MUST be one
        //entry in this array for each entry in the Security Targets
        //field of the security block.
        //(hmac is the only result)
        std::vector<uint8_t>* hmacPtr = bib.AppendAndGetExpectedHmacPtr();
        hmacPtr->resize(ALG_MINUS_5_TO_BYTE_LENGTH_LUT[variantMinus5]);

        unsigned int messageDigestOutSize;
        //not inplace test (separate in and out buffers)
        if (!BPSecManager::HmacSha(ctxWrapper,
            variant,
            ipptParts,
            hmacKey, hmacKeyLength,
            hmacPtr->data(), messageDigestOutSize))
        {
            return false;
        }
        if (hmacPtr->size() != messageDigestOutSize) {
            //fatal error (may have overwritten memory)
            LOG_FATAL(subprocess) << "hmacPtr->size() != messageDigestOutSize (may have overwritten memory)";
            return false;
        }
#ifdef BPSEC_MANAGER_PRINT_DEBUG
        LOG_DEBUG(subprocess) << "target block number " << target << ":";
        {
            std::string hexString;
            BinaryConversions::BytesToHexString(*hmacPtr, hexString);
            boost::to_lower(hexString);
            LOG_DEBUG(subprocess) << "  calculatedHmac: " << hexString;
        }
#endif
    }

    //at least one bib was added, so rerender
    if (insertBibBeforeThisBlockNumberIfNotNull) { //for matching unit test examples
        bv.InsertMoveCanonicalBlockBeforeBlockNumber(std::move(blockPtr), *insertBibBeforeThisBlockNumberIfNotNull);
    }
    else {
        bv.PrependMoveCanonicalBlock(std::move(blockPtr));
    }
    if (renderInPlaceWhenFinished) {
        return bv.RenderInPlace(PaddedMallocator<uint8_t>::PADDING_ELEMENTS_BEFORE);
    }
    return true;
}

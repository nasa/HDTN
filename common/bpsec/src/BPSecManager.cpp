/**
 * @file BPSecManager.cpp
 * @author  Nadia Kortas <nadia.kortas@nasa.gov>
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

#include "../include/BPSecManager.h"
#include <iostream>
#include "TimestampUtil.h"
#include "Uri.h"
#include <boost/make_unique.hpp>
#include <openssl/hmac.h>
#include <openssl/evp.h>
#include <string>
#include <cstring>
#include <sstream>
#include "codec/BundleViewV7.h"
#include "codec/bpv7.h"
#include "BinaryConversions.h"
#include "PaddedVectorUint8.h"
#include <vector>
#include <iostream>
#include <openssl/bio.h>
#include <openssl/err.h>
#include <openssl/rand.h>
#include <openssl/ssl.h>
#include <openssl/x509v3.h>
#include <openssl/aes.h>
#include <openssl/evp.h>
#include <openssl/rand.h>
#include <string>
#include <sstream>
#include <vector>
#include <iostream>
#include "Logger.h"
#include "codec/bpv7.h"
#include <openssl/bn.h>
#include <openssl/rsa.h>
#include <openssl/pem.h>
#include <openssl/engine.h>
#include <assert.h>
#include <stdio.h>
#include <boost/algorithm/hex.hpp>
#include <boost/algorithm/string.hpp>

using std::string;
using std::vector;
using std::cout;
using std::endl;
using namespace boost::algorithm;

static constexpr hdtn::Logger::SubProcess subprocess = hdtn::Logger::SubProcess::none;

BPSecManager::BPSecManager(const bool isSecEnabled) : 
m_isSecEnabled(isSecEnabled)
{
}

BPSecManager::~BPSecManager() {}

unsigned char* BPSecManager::hmac_sha(const EVP_MD *evp_md,
                    std::string key,
                    std::string data, unsigned char *md,
                    unsigned int *md_len)
{
    HMAC_CTX *c = NULL;
    if ((c = HMAC_CTX_new()) == NULL)
        goto err;

    printf("key:\n");
    BIO_dump_fp(stdout, (const char*) key.c_str(), key.length());
    /*
    std::cout << "Key Length: " << key.length() << std::endl;
    std::cout << "Plaintext: " << data << std::endl;
    std::cout << "Plaintext length: " << data.length()  << std::endl;
    */

    printf("Plaintext:\n");
    BIO_dump_fp(stdout, (const char*) data.c_str(), data.length());

    if (!HMAC_Init_ex(c, (const unsigned char*) key.c_str(), key.length(), evp_md, NULL))
        goto err;
    if (!HMAC_Update(c, (const unsigned char*) data.c_str(), data.length()))
        goto err;
    if (!HMAC_Final(c, md, md_len))
        goto err;
    HMAC_CTX_free(c);

    printf("HMAC Digest:\n");
    BIO_dump_fp(stdout, (const char*) md, *md_len);

    return md;
 err:
    HMAC_CTX_free(c);
    return NULL;
}

int BPSecManager::aes_gcm_encrypt(std::string gcm_pt, std::string gcm_key, 
                                  std::string gcm_iv, std::string gcm_aad, 
				  unsigned char* ciphertext, unsigned char* tag, 
				  int *outlen) 
{
    int ret = 0;
    EVP_CIPHER_CTX *ctx;
    int tmplen;

    printf("AES GCM Encrypt:\n");
    printf("Plaintext:\n");
    BIO_dump_fp(stdout, (const char*) gcm_pt.c_str(), gcm_pt.length());
 

    printf("key:\n");
    BIO_dump_fp(stdout, (const char*) gcm_key.c_str(), gcm_key.length());

    printf("IV:\n");
    BIO_dump_fp(stdout, (const char*) gcm_iv.c_str(), gcm_iv.length());

    printf("aad:\n");
    BIO_dump_fp(stdout, (const char*) gcm_aad.c_str(), gcm_aad.length());

    if ((ctx = EVP_CIPHER_CTX_new()) == NULL)
        goto err; 
	
    /* Set cipher type and mode */
    if (gcm_key.length() == 16) {
        if (!EVP_EncryptInit_ex(ctx, EVP_aes_128_gcm(), NULL, NULL, NULL))
	    goto err;
    } else if (gcm_key.length() == 32) {
        if (!EVP_EncryptInit_ex(ctx, EVP_aes_256_gcm(), NULL, NULL, NULL))
            goto err;
    } else { 
       printf("Error Incorrect Key length!!\n");
       goto err;    
    }

    /* Set IV length if default 96 bits is not appropriate */
    if (!EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_SET_IVLEN, 
			    gcm_iv.length(), NULL))
	goto err; 
    
    /* Initialise key and IV */
    if (!EVP_EncryptInit_ex(ctx, NULL, NULL, (const unsigned char*) gcm_key.c_str(),  
			    (const unsigned char*) gcm_iv.c_str()))
	goto err; 
    
    /* Zero or more calls to specify any AAD */
    if (!EVP_EncryptUpdate(ctx, NULL, outlen, 
			   (const unsigned char*) gcm_aad.c_str(), 
			   gcm_aad.length()))
	goto err;
    
    /* Encrypt plaintext */
    if (!EVP_EncryptUpdate(ctx, ciphertext, outlen, (const unsigned char*) gcm_pt.c_str(), 
			    gcm_pt.length()))
	goto err; 
    
    /* Output encrypted block */
    printf("Ciphertext:\n");
    BIO_dump_fp(stdout, (const char*) ciphertext, *outlen);
    std::cout << "Ciphertext Len "  << *outlen << std::endl;  

    memcpy(tag, ciphertext, EVP_GCM_TLS_TAG_LEN); //The length of the authentication tag, prior to any CBOR encoding, MUST be 128 bits.

    /* Finalise: note get no output for GCM */
   if (!EVP_EncryptFinal_ex(ctx, tag, outlen))
	goto err;
    

    /* Get tag */
   if (!EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_GET_TAG, EVP_GCM_TLS_TAG_LEN, tag)) //The length of the authentication tag, prior to any CBOR encoding, MUST be 128 bits.
	goto err; 


    /* Output tag */
     printf("Tag:\n");
    BIO_dump_fp(stdout, (const char*) tag, EVP_GCM_TLS_TAG_LEN);

    ret = 1; 

err:
    if (!ret)
        ERR_print_errors_fp(stderr);

    EVP_CIPHER_CTX_free(ctx);
    
    return ret;
}

int  BPSecManager::aes_gcm_decrypt(std::string gcm_ct, std::string gcm_tag,
                                   std::string gcm_key, std::string gcm_iv, 
				   std::string gcm_aad, unsigned char* plaintext, 
				   int *outlen)
{
    int ret = 0;
    EVP_CIPHER_CTX *ctx;
    int tmplen, rv;
    unsigned char outbuf[512];
    EVP_CIPHER *cipher = NULL;

    printf("AES GCM Decrypt:\n");
    printf("Ciphertext:\n");
    BIO_dump_fp(stdout, (const char*) gcm_ct.c_str(), gcm_ct.length());
    
    if ((ctx = EVP_CIPHER_CTX_new()) == NULL)
        goto err;

    // Select cipher 
    if (gcm_key.length() == 16) {
        if (!EVP_DecryptInit_ex(ctx, EVP_aes_128_gcm(), NULL, NULL, NULL))
	    goto err;
    } else if (gcm_key.length() == 32) {
        if (!EVP_DecryptInit_ex(ctx, EVP_aes_256_gcm(), NULL, NULL, NULL))
            goto err;
    } else {
        printf("Error Incorrect Key length!!\n");
       goto err;

    }	    

    //Set IV length, omit for 96 bits
    if (!EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_SET_IVLEN, gcm_iv.length(), NULL))
	    goto err; 


    // Specify key and IV 
    if (!EVP_DecryptInit_ex(ctx, NULL, NULL, (const unsigned char*) gcm_key.c_str(), 
			    (const unsigned char*) gcm_iv.c_str()))
	    goto err; 

    // Zero or more calls to specify any AAD 
    if (!EVP_DecryptUpdate(ctx, NULL, outlen, (const unsigned char*) gcm_aad.c_str(), 
			   gcm_aad.length()))
		    goto err; 

    // Decrypt plaintext 
    if (!EVP_DecryptUpdate(ctx, plaintext, outlen, (const unsigned char*) gcm_ct.c_str(), 
		           gcm_ct.length()))
    goto err;

    // Output decrypted block 
    printf("Plaintext:\n");
    BIO_dump_fp(stdout, (const char*) plaintext, *outlen);
    std::cout << "plaintext Len "  << *outlen << std::endl;

    printf("Tag :\n");
    BIO_dump_fp(stdout, (const char*)gcm_tag.c_str(), gcm_tag.length());

    printf("Key :\n");
    BIO_dump_fp(stdout, (const char*)gcm_iv.c_str(), gcm_iv.length());

    printf("IV :\n");
    BIO_dump_fp(stdout, (const char*)gcm_key.c_str(), gcm_key.length());

    printf("Ciphertext:\n");
    BIO_dump_fp(stdout, (const char*) gcm_ct.c_str(), gcm_ct.length());


    // Set expected tag value. 
    if (!EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_SET_TAG, gcm_tag.length(),
                            (void *)gcm_tag.c_str()))
        goto err;
    
    // Finalise: note get no output for GCM 
    rv = EVP_DecryptFinal_ex(ctx, plaintext, outlen);
    
    printf("***Tag Verify %s\n", rv > 0 ? "Successful!" : "Failed!");
    
    ret = 1; 

err:
 
    if (!ret) {
	ERR_print_errors_fp(stderr);
        std::cout << "Error Decrypt!!! " << std::endl; 
    }

    EVP_CIPHER_CTX_free(ctx);

    return ret;
}







/**
 * @file BPSecManager.h
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


class BPSecManager { 
private:
      BPSecManager();
public:
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
    BPSEC_EXPORT unsigned char* hmac_sha(const EVP_MD *evp_md,
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
    

    BPSEC_EXPORT int aes_gcm_encrypt(std::string gcm_pt, std::string gcm_key,
                                  std::string gcm_iv, std::string gcm_aad,
                                  unsigned char* ciphertext, unsigned char* tag,      
                                  int *outlen);

    BPSEC_EXPORT int aes_gcm_decrypt(std::string gcm_ct, std::string gcm_tag,
                                     std::string gcm_key, std::string gcm_iv,
                                     std::string gcm_aad, unsigned char* pt, int *outlen);

    
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


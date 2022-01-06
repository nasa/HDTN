#include <cstdio>
#include <cstdlib>
#include <fcntl.h>
#include <unistd.h>
#include <iostream>
#include <cstring>

#include "codec/bpv6.h"
#include "codec/bpv7.h"
#include "codec/bpv6-ext-block.h"

#define BUNDLE_SZ_MAX  (8192)

using namespace hdtn;

int main(int argc, char* argv[]) {
    size_t sz = 0;
    uint64_t offset = 0;
    uint64_t block_start=0;
    uint8_t bpv6_buf[BUNDLE_SZ_MAX];
    const char * filename= "../test/ion_bundle";
    memset(bpv6_buf, 0x42, BUNDLE_SZ_MAX);
    if(argc >1)
    {
    	filename=argv[1];
    }
 
    int fd = open(filename, O_RDONLY);
    if(fd < 0) {
        perror("Failed to open target file:");
        return -1;
    }

    sz = read(fd, bpv6_buf, BUNDLE_SZ_MAX);
    if(sz < 0) {
        perror("Failed to read bundle data:");
        return -2;
    }
   
    bpv6_primary_block bpv6_primary;
    bpv6_canonical_block bpv6_block;
    memset(&bpv6_primary, 0, sizeof(bpv6_primary_block));
    memset(&bpv6_block, 0, sizeof(bpv6_canonical_block));
    bpv6_bplib_bib_block bpv6_bib;
    memset(&bpv6_bib, 0, sizeof(bpv6_bplib_bib_block));
    int crc_result=0;


    offset = bpv6_primary.cbhe_bpv6_primary_block_decode((char *)bpv6_buf, 0,sz);
    bpv6_primary.bpv6_primary_block_print();

    while((bpv6_block.flags & BPV6_BLOCKFLAG_LAST_BLOCK) != BPV6_BLOCKFLAG_LAST_BLOCK){
    	block_start=offset;
    	offset += bpv6_block.bpv6_canonical_block_decode((char *)bpv6_buf, offset,sz);
    	switch(bpv6_block.type){
    	case BPV6_BLOCKTYPE_CUST_TRANSFER_EXT:{
    		bpv6_cust_transfer_ext_block bpv6_cteb;
    		memset(&bpv6_cteb, 0, sizeof(bpv6_cust_transfer_ext_block));
    		bpv6_cteb.type=bpv6_block.type;
    		bpv6_cteb.flags=bpv6_block.flags;
    		bpv6_cteb.length=bpv6_block.length;
            bpv6_cteb.bpv6_cteb_decode((char *)bpv6_buf,block_start,offset,sz);
            bpv6_cteb.bpv6_cteb_print();
    		break;
    	}
    	case BPV6_BLOCKTYPE_BPLIB_BIB :{
    		bpv6_bib.type=bpv6_block.type;
    		bpv6_bib.flags=bpv6_block.flags;
    		bpv6_bib.length=bpv6_block.length;
            bpv6_bib.bpv6_bib_decode((char *)bpv6_buf,offset,sz);
            bpv6_bib.bpv6_bib_print();
    		break;
    	}
    	case BPV6_BLOCKTYPE_PREV_HOP_INSERTION :{
    		bpv6_prev_hop_ext_block bpv6_phn;
    		memset(&bpv6_phn, 0, sizeof(bpv6_prev_hop_ext_block));
    		bpv6_phn.type=bpv6_block.type;
    		bpv6_phn.flags=bpv6_block.flags;
    		bpv6_phn.length=bpv6_block.length;
            bpv6_phn.bpv6_prev_hop_decode((char *)bpv6_buf,block_start,offset,sz);
            bpv6_phn.bpv6_prev_hop_print();
    		break;
    	 }
    	case BPV6_BLOCKTYPE_BUNDLE_AGE :{
    		bpv6_bundle_age_ext_block bpv6_bae;
    		memset(&bpv6_bae, 0, sizeof(bpv6_bundle_age_ext_block));
    		bpv6_bae.type=bpv6_block.type;
    		bpv6_bae.flags=bpv6_block.flags;
    		bpv6_bae.length=bpv6_block.length;
            bpv6_bae.bpv6_bundle_age_decode((char *)bpv6_buf,offset,sz);
            bpv6_bae.bpv6_bundle_age_print();
    		break;
    	}
    	case BPV6_BLOCKTYPE_PAYLOAD :{
    		uint8_t payload[bpv6_block.length];
    		memcpy(payload,(bpv6_buf+offset),bpv6_block.length);
            bpv6_block.bpv6_canonical_block_print();
#if 0
    		if((bpv6_bib.security_result_len>0)&&(bpv6_bib.cipher_suite_id==BPLIB_BIB_CRC16_X25))
    		{
    			crc_result=bib_verify(&payload, bpv6_block.length,&bpv6_bib);
    			if(crc_result==1)
    			{
    				printf("CRC verified successfully for payload\n");
    			}
    			else
    				printf("CRC verification failed for payload\n");
    		}
#endif
    		break;
    	}

    	}

    	offset += bpv6_block.length;
    }
 
    return 0;
}

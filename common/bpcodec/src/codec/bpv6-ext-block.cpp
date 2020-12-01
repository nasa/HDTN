#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <cstdlib>
#include "codec/bpv6.h"
#include "codec/crc.h"
#include "codec/bpv6-ext-block.h"


void bpv6_block_flags_print(bpv6_canonical_block* block){
	printf("Flags: 0x%llx\n", block->flags);
	if(block->flags & BPV6_BLOCKFLAG_LAST_BLOCK) {
		printf("* Last block in this bundle.\n");
	}
	if(block->flags & BPV6_BLOCKFLAG_DISCARD_BLOCK_FAILURE) {
		printf("* Block should be discarded upon failure to process.\n");
	}
	if(block->flags & BPV6_BLOCKFLAG_DISCARD_BUNDLE_FAILURE) {
		printf("* Bundle should be discarded upon failure to process.\n");
	}
	if(block->flags & BPV6_BLOCKFLAG_EID_REFERENCE) {
		printf("* This block references elements from the dictionary.\n");
	}
	if(block->flags & BPV6_BLOCKFLAG_FORWARD_NOPROCESS) {
		printf("* This block was forwarded without being processed.\n");
	}

}

uint8_t bpv6_prev_hop_decode(bpv6_prev_hop_ext_block* block, const char* buffer,const size_t block_start,const size_t offset, const size_t bufsz) {
   uint64_t index = offset;
   uint64_t data_len = 0;
   char data[46];//using ipn scheme the max number of ascii characters in an eid would be 4+20+1+20 = "ipn:"+2^64+"." + 2^64
   memset(&data, 0, sizeof(data));
   uint8_t  incr  = 0;
   data_len=block->length-index+block_start;//calculate the length of the custodian eid string
   memcpy(&data, buffer+index,data_len);
   strncpy(block->scheme,data, data_len);//assuming "ipn" or "dtn " + NULL
   strncpy(block->scheme_specific_eid,data+4, data_len-4);
   return 0;

}

void bpv6_prev_hop_print(bpv6_prev_hop_ext_block* block){
	if( block->type==BPV6_BLOCKTYPE_PREV_HOP_INSERTION)
	{
		printf("\nPrevious Hop Extension Block [type %u]\n", block->type);
		bpv6_block_flags_print(block);
		printf("Block length: %lu bytes\n", block->length);
		printf("Scheme: %s\n",block->scheme);
		printf("Scheme Specific Node Name: %s\n",block->scheme_specific_eid);
	}
	else{
		printf("Block is not Previous Hop Extension Block\n");
	}
}

uint8_t bpv6_cteb_decode(bpv6_cust_transfer_ext_block* block, const char* buffer, const size_t block_start,const size_t offset, const size_t bufsz) {
   uint64_t index = offset;
   uint64_t custodian_eid_len = 0;
   bpv6_eid custodian;
   memset(&custodian, 0, sizeof(bpv6_eid));
   uint8_t  incr  = 0;
   incr = bpv6_sdnv_decode(&block->cust_id, buffer, index, bufsz);
   index += incr;
   custodian_eid_len=block->length-index+block_start;//calculate the length of the custodian eid string
   char_to_bpv6_eid(&custodian, buffer,index, custodian_eid_len,bufsz);
   block->cteb_creator_node=custodian.node;
   block->cteb_creator_service=custodian.service;
   return 0;

}

void bpv6_cteb_print(bpv6_cust_transfer_ext_block* block){
	if( block->type==BPV6_BLOCKTYPE_CUST_TRANSFER_EXT)
	{
		printf("\nCustody Transfer Extension Block [type %u]\n", block->type);
		bpv6_block_flags_print(block);
		printf("Block length: %lu bytes\n", block->length);
		printf("Custody Id: %lu\n",block->cust_id);
		printf("CTEB creator custodian EID: ipn:%d.%d\n",(int)block->cteb_creator_node,(int)block->cteb_creator_service);
	}
	else{
		printf("Block is not Custody Transfer Extension Block\n");
	}
}

uint8_t bpv6_bib_decode(bpv6_bplib_bib_block* block, const char* buffer,const size_t offset, const size_t bufsz) {
	uint64_t index = offset;
	uint8_t  incr  = 0;
	uint16_t crc16=0;
	incr = bpv6_sdnv_decode(&block->num_targets, buffer, index, bufsz);
	index += incr;
	incr = bpv6_sdnv_decode(&block->target_type, buffer, index, bufsz);
	index += incr;
	incr = bpv6_sdnv_decode(&block->target_sequence, buffer, index, bufsz);
	index += incr;
	incr = bpv6_sdnv_decode(&block->cipher_suite_id, buffer, index, bufsz);
	index += incr;
	incr = bpv6_sdnv_decode(&block->cipher_suite_flags, buffer, index, bufsz);
	index += incr;
	incr = bpv6_sdnv_decode(&block->num_security_results, buffer, index, bufsz);
	index += incr;
	incr = bpv6_sdnv_decode(&block->security_result_type, buffer, index, bufsz);
	index += incr;
	incr = bpv6_sdnv_decode(&block->security_result_len, buffer, index, bufsz);
	index += incr;
	uint8_t valptr[block->security_result_len];
	memcpy(&valptr, buffer+index,block->security_result_len);
     //crc is big endian, not encoded as sdnv
	if (block->cipher_suite_id == BPLIB_BIB_CRC16_X25 && block->security_result_len == 2)
	{
		crc16 |= (((uint16_t) valptr[0]) << 8);
		crc16 |= ((uint16_t) valptr[1]);
		block->security_result=crc16;
	}
	index += 2;
	return 0;
}

void bpv6_bib_print(bpv6_bplib_bib_block* block){
	if( block->type==BPV6_BLOCKTYPE_BPLIB_BIB)
	{
		printf("\nBplib Bundle Integrity Block [type %u]\n", block->type);
		bpv6_block_flags_print(block);
		printf("Block length: %lu bytes\n", block->length);
		printf("Number of security targets: %lu\n",block->num_targets);
		printf("Security target type: %lu\n",block->target_type);
		printf("Security target sequence: %lu\n",block->target_sequence);
		printf("Cipher suite id: %lu\n",block->cipher_suite_id);
		printf("Cipher suite flags: %lu\n",block->cipher_suite_flags);
		printf("Number of security results: %lu\n",block->num_security_results);
		printf("Security result type: %lu\n",block->security_result_type);
		printf("Security result length: %lu\n",block->security_result_len);
		printf("Security result : %lu\n\n",block->security_result);

	}
	else{
		printf("Block is not bplib Bundle Integrity Block\n\n");
	}
}

uint8_t bpv6_bundle_age_decode(bpv6_bundle_age_ext_block* block,const char* buffer,const size_t offset, const size_t bufsz)
{
	uint64_t index = offset;
	uint8_t  incr  = 0;
	incr = bpv6_sdnv_decode(&block->bundle_age, buffer, index, bufsz);
	index += incr;
	return 0;
}

void bpv6_bundle_age_print(bpv6_bundle_age_ext_block* block){
	if( block->type==BPV6_BLOCKTYPE_BUNDLE_AGE)
	{
		printf("\n Bundle Age Block [type %u]\n", block->type);
		bpv6_block_flags_print(block);
		printf("Block length: %lu bytes\n", block->length);
		printf("Bundle Age: %lu\n\n",block->bundle_age);
	}
	else{
		printf("Block is not bundle age block\n\n");
	}
}


uint8_t char_to_bpv6_eid(bpv6_eid* eid, const char* buffer, const size_t offset, const size_t eid_len,const size_t bufsz){
	char * pch;
	char* endpch;
	char eid_str[45];
	uint8_t node_len;
	if(bufsz<offset+eid_len)
	{
		printf("Bad string length: %lu\n", eid_len);
		return 0;  
	}
	if(buffer[offset] != 'i' || buffer[offset+1]!= 'p' ||  buffer[offset+2] != 'n' ||  buffer[offset+3] != ':' )
	{
			printf("Not IPN EID\n");
			return 0;  // make sure this is ipn scheme eid
	}
	else
	{
		memcpy(eid_str,&buffer[offset+4],eid_len);
		pch=strchr(eid_str,'.');
		if(pch!=NULL){
			eid->node = strtoull((const char*)eid_str,&pch,10);
			pch++;
			eid->service = strtoull((const char*)pch,&endpch,10);
		}
		return 1;
	}
}

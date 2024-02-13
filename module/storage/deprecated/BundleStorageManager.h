/**
 * @file BundleStorageManager.h
 * @author  Brian Tomko <brian.j.tomko@nasa.gov>
 *
 * @copyright Copyright (c) 2021 United States Government as represented by
 * the National Aeronautics and Space Administration.
 * No copyright is claimed in the United States under Title 17, U.S.Code.
 * All Other Rights Reserved.
 *
 * @section LICENSE
 * Released under the NASA Open Source Agreement (NOSA)
 * See LICENSE.md in the source root directory for more information.
 */

#ifndef _BUNDLE_STORAGE_MANAGER_H
#define _BUNDLE_STORAGE_MANAGER_H

#include <boost/integer.hpp>
#include <stdint.h>
#include <map>
#include <vector>
#include <string>
#include <cstdio>
#include <boost/iostreams/device/mapped_file.hpp>
#include "BundleStorageConfig.h"

//#define USE_VECTOR_CIRCULAR_BUFFER 1
#ifdef USE_VECTOR_CIRCULAR_BUFFER
typedef boost::uint32_t segment_id_t;
typedef std::vector<segment_id_t> segment_id_vec_t;
typedef std::vector<segment_id_vec_t> expiration_circular_buf_t;
typedef std::vector<expiration_circular_buf_t> priority_vec_t;
typedef std::map<std::string, priority_vec_t> destination_map_t; //results in about 12MB per link
#else
typedef boost::uint32_t segment_id_t;
typedef boost::uint64_t abs_expiration_t;
typedef std::vector<segment_id_t> segment_id_vec_t;
typedef std::map<abs_expiration_t, segment_id_vec_t> expiration_map_t;
typedef std::vector<expiration_map_t> priority_vec_t;
typedef std::map<std::string, priority_vec_t> destination_map_t;
#endif

//two days
#define NUMBER_OF_EXPIRATIONS (86400*2)
#define NUMBER_OF_PRIORITIES (3)
//#define USE_MEMORY_MAPPED_FILES 1

class BundleStorageManager {
public:
	BundleStorageManager();
	~BundleStorageManager();
	
	void AddLink(const std::string & linkName);
	void StoreBundle(const std::string & linkName, const unsigned int priorityIndex, const abs_expiration_t absExpiration, const segment_id_t segmentId, const unsigned char * const data, std::size_t dataSize);
	segment_id_t GetBundle(const std::vector<std::string> & availableDestLinks, std::size_t & retLinkIndex, unsigned int & retPriorityIndex, abs_expiration_t & retAbsExpiration, unsigned char * const data, std::size_t dataSize);
	
	static bool UnitTest();
	static bool TimeRandomReadsAndWrites();

private:
	void OpenFile();
	void CloseFile();
private:
	destination_map_t m_destMap;
#ifdef USE_MEMORY_MAPPED_FILES
	boost::iostreams::mapped_file  m_mappedFile;
#else
	FILE * m_fileHandle;
#endif
};


#endif //_BUNDLE_STORAGE_MANAGER_H

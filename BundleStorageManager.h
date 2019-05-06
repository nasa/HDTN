#ifndef _BUNDLE_STORAGE_MANAGER_H
#define _BUNDLE_STORAGE_MANAGER_H

#include <boost/integer.hpp>
#include <stdint.h>
#include <map>
#include <vector>
#include <string>

typedef boost::uint32_t segment_id_t;
typedef std::vector<segment_id_t> segment_id_vec_t;
typedef std::vector<segment_id_vec_t> expiration_circular_buf_t;
typedef std::vector<expiration_circular_buf_t> priority_vec_t;
typedef std::map<std::string, priority_vec_t> destination_map_t;

//two days
#define NUMBER_OF_EXPIRATIONS (86400*2)
#define NUMBER_OF_PRIORITIES (3)

class BundleStorageManager {
public:

	void AddLink(const std::string & linkName);
	
	static bool UnitTest();

private:
	
private:
	destination_map_t m_destMap;
	
};


#endif //_BUNDLE_STORAGE_MANAGER_H

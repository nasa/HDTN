/**
 *
 * @section DESCRIPTION
 *
 * LogicalEndpointIDentifier (LEIDer) 
 */

#ifndef _LEIDER_H
#define _LEIDER_H 1

//#include "leider_lib_export.h"
//#ifndef CLASS_VISIBILITY_LEIDER_LIB
//#  ifdef _WIN32
//#    define CLASS_VISIBILITY_LEIDER_LIB
//#  else
//#    define CLASS_VISIBILITY_LEIDER_LIB LEIDER_LIB_EXPORT
//#  endif
//#endif

#include "codec/BundleViewV6.h"
#include "codec/BundleViewV7.h"

namespace hdtn {

//class CLASS_VISIBILITY_LEIDER_LIB Leider {
//public:
//	LEIDER_LIB_EXPORT virtual cbhe_eid_t query(const BundleViewV6&) = 0;
//	LEIDER_LIB_EXPORT virtual cbhe_eid_t query(const BundleViewV7&) = 0;
//};

class Leider {
public:
	virtual cbhe_eid_t query(const BundleViewV6&) = 0;
	virtual cbhe_eid_t query(const BundleViewV7&) = 0;
};

}

#endif  //_LEIDER_H
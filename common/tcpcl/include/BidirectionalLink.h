/**
 * @file BidirectionalLink.h
 * @author  Brian Tomko <brian.j.tomko@nasa.gov>
 *
 * @copyright Copyright © 2021 United States Government as represented by
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
 * This BidirectionalLink pure virtual base class defines common functionality
 * that is shared between versions 3 and 4 of the TCP Convergence-Layer Protocol.
 */

#ifndef _BIDIRECTIONAL_LINK_H
#define _BIDIRECTIONAL_LINK_H 1
#include "tcpcl_lib_export.h"
#ifndef CLASS_VISIBILITY_TCPCL_LIB
#  ifdef _WIN32
#    define CLASS_VISIBILITY_TCPCL_LIB
#  else
#    define CLASS_VISIBILITY_TCPCL_LIB TCPCL_LIB_EXPORT
#  endif
#endif
#include <cstdint>

class BidirectionalLink {
public:
    virtual std::size_t Virtual_GetTotalBundlesAcked() = 0;
    virtual std::size_t Virtual_GetTotalBundlesSent() = 0;
    virtual std::size_t Virtual_GetTotalBundlesUnacked() = 0;
    virtual std::size_t Virtual_GetTotalBundleBytesAcked() = 0;
    virtual std::size_t Virtual_GetTotalBundleBytesSent() = 0;
    virtual std::size_t Virtual_GetTotalBundleBytesUnacked() = 0;

    virtual unsigned int Virtual_GetMaxTxBundlesInPipeline() = 0;
};



#endif  //_BIDIRECTIONAL_LINK_H

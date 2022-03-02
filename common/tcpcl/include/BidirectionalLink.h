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
#include <string>
#include <boost/thread.hpp>
#include <boost/asio.hpp>
#include <map>
#include <vector>
#include "Tcpcl.h"
#include "TcpAsyncSender.h"
#include "CircularIndexBufferSingleProducerSingleConsumerConfigurable.h"
#include "PaddedVectorUint8.h"

typedef boost::function<void(padded_vector_uint8_t & movableBundle)> OutductOpportunisticProcessReceivedBundleCallback_t;

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

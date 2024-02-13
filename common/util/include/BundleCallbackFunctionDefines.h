/**
 * @file BundleCallbackFunctionDefines.h
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
 *
 * @section DESCRIPTION
 *
 * This BundleCallbackFunctionDefines file defines generic bundle callbacks to be used across all outducts and inducts.
 */

#ifndef BUNDLE_CALLBACK_FUNCTION_DEFINES_H
#define BUNDLE_CALLBACK_FUNCTION_DEFINES_H 1


#include <cstdint>
#include <vector>
#include "PaddedVectorUint8.h"
#include <boost/function.hpp>
#include <zmq.hpp>
//bool successCallbackCalled shall denote that, if ltp sender session is storing to disk, an OnSuccessfulBundleSendCallback_t shall be called
// NOT when confirmation that red part was received by the receiver to close the session,
// BUT RATHER when the session was fully written to disk and the SessionStart callback was called.
// The OnSuccessfulBundleSendCallback_t (or OnFailed callbacks) are needed for Egress to send acks to storage or ingress in order
// to free up ZMQ bundle pipeline.
// If successCallbackCalled is true, then the OnFailed callbacks shall not ack ingress or storage to free up ZMQ pipeline,
// but instead treat the returned movableBundle as a new bundle.
typedef boost::function<void(padded_vector_uint8_t& movableBundle, std::vector<uint8_t>& userData, uint64_t outductUuid, bool successCallbackCalled)> OnFailedBundleVecSendCallback_t;
typedef boost::function<void(zmq::message_t & movableBundle, std::vector<uint8_t>& userData, uint64_t outductUuid, bool successCallbackCalled)> OnFailedBundleZmqSendCallback_t;
typedef boost::function<void(std::vector<uint8_t>& userData, uint64_t outductUuid)> OnSuccessfulBundleSendCallback_t;
typedef boost::function<void(bool isLinkDownEvent, uint64_t outductUuid)> OnOutductLinkStatusChangedCallback_t;

//Bidirectional Links
typedef boost::function<void(padded_vector_uint8_t& movableBundle)> OutductOpportunisticProcessReceivedBundleCallback_t;

#endif // BUNDLE_CALLBACK_FUNCTION_DEFINES_H


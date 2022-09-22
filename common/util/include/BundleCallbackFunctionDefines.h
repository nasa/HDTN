/**
 * @file BundleCallbackFunctionDefines.h
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
 * This BundleCallbackFunctionDefines file defines generic bundle callbacks to be used across all outducts and inducts.
 */

#ifndef BUNDLE_CALLBACK_FUNCTION_DEFINES_H
#define BUNDLE_CALLBACK_FUNCTION_DEFINES_H 1


#include <cstdint>
#include <vector>
#include <boost/function.hpp>
#include <zmq.hpp>

typedef boost::function<void(std::vector<uint8_t> & movableBundle, uint64_t outductUuid)> OnFailedBundleVecSendCallback_t;
typedef boost::function<void(zmq::message_t & movableBundle, uint64_t outductUuid)> OnFailedBundleZmqSendCallback_t;
typedef boost::function<void(std::vector<uint8_t>& userData, uint64_t outductUuid)> OnSuccessfulBundleSendCallback_t;
typedef boost::function<void(bool isLinkDownEvent, uint64_t outductUuid)> OnOutductLinkStatusChangedCallback_t;

#endif // BUNDLE_CALLBACK_FUNCTION_DEFINES_H


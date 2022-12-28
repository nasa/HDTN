/**
 * @file BpGenAsync.h
 * @author  Brian Tomko <brian.j.tomko@nasa.gov>
 * @author  Gilbert Clark
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
 * The BpGenAsync class is a child class of BpSourcePattern.  It is an app used for
 * sending fixed-payload size bundles, either at a defined rate,
 * or as fast as possible.  The app copies a tiny payload to the beginning of the bundle
 * payload block used for counting bundles (in order or out of order).
 * The remaining data in the bundle payload block is unitialized.
 * This app is intended to be used with the BpSink app.
 */

#ifndef _BPGEN_ASYNC_H
#define _BPGEN_ASYNC_H 1

#include "app_patterns/BpSourcePattern.h"

class BpGenAsync : public BpSourcePattern {
private:
    BpGenAsync();
public:
    BpGenAsync(uint64_t bundleSizeBytes);
    virtual ~BpGenAsync() override;
    
protected:
    virtual uint64_t GetNextPayloadLength_Step1() override;
    virtual bool CopyPayload_Step2(uint8_t * destinationBuffer) override;
private:
    uint64_t m_bundleSizeBytes;
    uint64_t m_bpGenSequenceNumber;
};


#endif //_BPGEN_ASYNC_H

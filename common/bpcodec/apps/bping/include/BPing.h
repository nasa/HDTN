/**
 * @file BPing.h
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
 * The BPing class is a child class of BpSourcePattern.  It is an app used for
 * sending periodic, wait-for-a-response, bundles, to another bundle agent
 * with a running echo service.  The app copies a tiny payload to the bundle
 * payload block containg a timestamp and sequence number.
 */

#ifndef _BPING_H
#define _BPING_H 1

#include "app_patterns/BpSourcePattern.h"

class BPing : public BpSourcePattern {
public:
    BPing();
    virtual ~BPing() override;
    
protected:
    virtual uint64_t GetNextPayloadLength_Step1() override;
    virtual bool CopyPayload_Step2(uint8_t * destinationBuffer) override;
    virtual bool ProcessNonAdminRecordBundlePayload(const uint8_t * data, const uint64_t size) override;
private:
    uint64_t m_bpingSequenceNumber;
};


#endif //_BPING_H

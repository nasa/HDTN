/**
 * @file BpSinkAsync.h
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
 * The BpSinkAsync class is a child class of BpSinkPattern.  It is an app used for
 * receiving any size bundles from the BpGen app.
 * The app reads a tiny payload at the beginning of the bundle
 * payload block used for counting bundles (in order or out of order).
 * This app is intended to be used with the BpGen app.
 */

#ifndef _BP_SINK_ASYNC_H
#define _BP_SINK_ASYNC_H

#include "app_patterns/BpSinkPattern.h"

struct FinalStatsBpSink {
    FinalStatsBpSink() : m_totalBytesRx(0), m_totalBundlesRx(0), m_receivedCount(0), m_duplicateCount(0),
        m_seqHval(0), m_seqBase(0) {};
    uint64_t m_totalBytesRx;
    uint64_t m_totalBundlesRx;
    uint64_t m_receivedCount;
    uint64_t m_duplicateCount;
    uint64_t m_seqHval;
    uint64_t m_seqBase;
};

class BpSinkAsync : public BpSinkPattern {
public:
    BpSinkAsync();
    virtual ~BpSinkAsync() override;

protected:
    virtual bool ProcessPayload(const uint8_t * data, const uint64_t size) override;
public:

    FinalStatsBpSink m_FinalStatsBpSink;
};



#endif  //_BP_SINK_ASYNC_H

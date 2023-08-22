/**
 * @file Masker.h
 * @author  Timothy Recker <timothy.recker@nasa.gov>
 * 
 * @section DESCRIPTION
 *
 * Masker is merely an object that can be queried for EIDs. It was implemented to make it possible to "apply an EID mask to a bundle",
 * or, stated more simply, to "mask a bundle". An EID mask is an EID assigned to a bundle field and used in HDTN 
 * internal bookkeeping--by including it in ZMQ messages that accompany bundles--without modifying the 
 * bundle itself. Thus, the bundle leaves the HDTN node with its original EIDs valid and intact. Currently,
 * the only time HDTN might mask a bundle EID is in Ingress, where it may mask the destination EID if configured to do so.
 * 
 * To motivate this concept, consider a DTN with endpoints A, B, C and contacts 
 * A->B from time 0 to 1 with data rate 2 mb/s
 * B->C from time 1 to 2 with data rate 1 mb/s
 * A->C from time 2 to 3 with data rate 1 mb/s
 * 
 * Now consider we want to send 2 bundles of size 1 mb from A to C. 
 * In this situation, the fastest route is A->B from time 0 to 1, B->C from time 1 to 2, which will be the assigned path for bundles with destination C. 
 * Both bundles would be forwarded along A->B from time 0 to 1 due to the contact having room to transmit both bundles, but 
 * we encounter a problem because B->C only has room to transmit 1 of the 2 bundles, so 1 of the bundles never gets forwarded along.
 * In the optimal case, one bundle is transmitted along the route #1 A->B->C and one is transmitted along route #2 A->C. 
 *
 * This ideal setting is only possible with extra control. One way to provide this control is to invent a destination D as an 
 * "endpoint mask" (alternatively a "pseudo-destination") and then "mask the destination of the second bundle" (alternatively "mask the second bundle" or
 * "apply the pseudo-destination to the second bundle").
 * 
 * To do this, the Router would send a RouteUpdate message to Egress associating the destination D 
 * with the hop A->C. The Router would also update Ingress telling it to apply the destination mask D to the second bundle. 
 * This solution allows fine control over routing while maintaining the "final destination" as the sole criteria for 
 * bundle release. This is useful in HDTN because Storage is optimized around the "final destination" as the
 * linchpin for bundle catalog bookkeeping.
 * 
 */

#ifndef _MASKER_H
#define _MASKER_H 1

#include "codec/BundleViewV6.h"
#include "codec/BundleViewV7.h"

namespace hdtn {

class Masker {
public:
	virtual cbhe_eid_t query(const BundleViewV6&) = 0;
	virtual cbhe_eid_t query(const BundleViewV7&) = 0;
    static std::shared_ptr<Masker> makePointer(std::string impl);
};

}

#endif  //_MASKER_H
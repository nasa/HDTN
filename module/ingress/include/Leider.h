/**
 *
 * @section DESCRIPTION
 *
 * LogicalEndpointIDentifier (LEIDer, pronounced "lied-ur")
 * 
 * Endpoint: a set of one or more nodes
 * Endpoint ID (EID): identifies an endpoint
 * Logical EID (LEID): identifies an endpoint conditioned by some additional criteria
 * A network with a finite number of nodes has a finite number of possible EIDs referring to distinct endpoints but
 * an infinite number of possible distinct, meaningful LEIDs.
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
 * This ideal setting is only possible with extra control. One way to provide this control is to introduce a Logical Endpoint "D" as the 
 * final destination for bundle routing and release. The Router would then send a RouteUpdate message to Egress associating the hop A->C
 * with the destination "D". The Router would also update Ingress telling it to apply the LEID "D" to the second bundle. 
 * This solution allows fine control over routing while maintaining the "final destination" as the sole criteria for 
 * bundle release. This is useful in HDTN because Storage is optimized around the "final destination" as 
 * linchpin for bundle catalog bookkeeping.
 
 * The meaning of the Logical Endpoint "D" could be interpreted in a couple different ways
 * (a) it means something like "the bundle endpoint C after route #1 has been scheduled to its capacity" or
 * (b) it is a mask HDTN puts over the endpoint C but only some bundles are aware of the mask.
 * Either way, in this case the IDs "C" and "D" refer to the same physical endpoint (the same singleton node or set of nodes) yet 
 * they are logically distinct. If paragraph does not make sense or the abstraction seems ill-conceived, ignore this and
 * just consider all this a hack.
 * 
 * Thus a LEIDer is merely an object that assigns LEIDs to bundles. Currently its only use in HDTN is to match bundles to Logical Destinations
 * for greater control over bundle routing/scheduling; from another point of view, it helps us "lie" to HDTN about bundle destinations so we
 * can get HDTN to do what we want without making great changes to its internal data structures or modifying the bundles themselves.
 * 
 */

#ifndef _LEIDER_H
#define _LEIDER_H 1

#define LEIDER_IMPLEMENTATION_CLASS ShiftingLeider

#include "codec/BundleViewV6.h"
#include "codec/BundleViewV7.h"

namespace hdtn {

class Leider {
public:
	virtual cbhe_eid_t query(const BundleViewV6&) = 0;
	virtual cbhe_eid_t query(const BundleViewV7&) = 0;
};

}

#endif  //_LEIDER_H

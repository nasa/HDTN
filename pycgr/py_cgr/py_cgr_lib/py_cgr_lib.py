import sys
import copy
from random import randint

# This library contains prototype clases and methods to evaluate
# Contact Graph Routing (CGR) routines as follows.
# - contact plans (cp): load plans from file or generate randomly
# - contact graph routing (cgr): compute routes using cgr techniques
# - forward (fwd): process resulting routes for a specific bundle
# - plot (plt): plot graph in gdf format for gephi visualization


class Contact:
    def __init__(self, frm, to, start, end, rate, confidence=1., owlt=0):
        # fixed parameters
        self.frm = frm
        self.to = to
        self.start = start
        self.end = end
        self.rate = rate
        self.owlt = owlt
        self.volume = rate * (end - start)
        self.confidence = confidence        

        # variable parameters
        self.mav = [self.volume, self.volume, self.volume]

        # route search working area
        self.arrival_time = sys.maxsize
        self.visited = False
        self.visited_nodes = []
        self.predecessor = 0

        # route management working area
        self.suppressed = False
        self.suppressed_next_hop = []

        # forwarding working area
        self.first_byte_tx_time = None
        self.last_byte_tx_time = None
        self.last_byte_arr_time = None
        self.effective_volume_limit = None

    def clear_dijkstra_working_area(self):
        self.arrival_time = sys.maxsize
        self.visited = False
        self.predecessor = 0
        self.visited_nodes = []

    def clear_management_working_area(self):
        self.suppressed = False
        self.suppressed_next_hop = []

    def __repr__(self):

        # replace with inf
        if self.end == sys.maxsize:
            end = "inf"
        else:
            end = self.end

        # mav in %
        volume = 100 * min(self.mav) / self.volume

        return "%s->%s(%s-%s,d%s)[mav%d%%]" % (self.frm, self.to, self.start, end, self.owlt, volume)


class Route:
    def __init__(self, contact, parent=None):
        # dfs: carry on from where parent route left
        self.parent = parent

        # fixed parameters
        self.hops = []
        if parent is None:
            self.to_node = None
            self.next_node = None
            self.from_time = 0
            self.to_time = sys.maxsize
            self.best_delivery_time = 0
            self.volume = sys.maxsize
            self.confidence = 1
            
            self.__visited = {}
        else:
            self.to_node = parent.to_node
            self.next_node = parent.next_node
            self.from_time = parent.from_time
            self.to_time = parent.to_time
            self.best_delivery_time = parent.best_delivery_time
            self.volume = parent.volume
            self.confidence = parent.confidence

            self.__visited = copy.copy(parent.__visited)

        # initial contact
        self.append(contact)

    def get_last_node(self):
        return self.get_last_contact().to

    def get_last_contact(self):
        return self.hops[-1]

    def visited(self, node):
        return node in self.__visited and self.__visited[node]

    def append(self, contact):
        assert (self.eligible(contact))
        self.hops.append(contact)
        self.__visited[contact.frm] = True
        self.__visited[contact.to] = True

        self.refresh_metrics()

    def refresh_metrics(self):
        assert self.hops
        self.to_node = self.get_hops()[-1].to
        self.next_node = self.get_hops()[0].to
        self.from_time = self.get_hops()[0].start
        self.to_time = sys.maxsize
        self.best_delivery_time = 0
        self.confidence = 1
        for contact in self.get_hops():
            self.to_time = min(self.to_time, contact.end)
            self.best_delivery_time = max(self.best_delivery_time + contact.owlt, contact.start + contact.owlt)
            self.confidence *= contact.confidence

        # volume
        prev_last_byte_arr_time = 0
        min_effective_volume_limit = sys.maxsize
        for contact in self.get_hops():
            if contact == self.get_hops()[0]:
                contact.first_byte_tx_time = contact.start
            else:
                contact.first_byte_tx_time = max(contact.start, prev_last_byte_arr_time)
            bundle_tx_time = 0  # immediate transmission
            contact.last_byte_tx_time = contact.first_byte_tx_time + bundle_tx_time
            contact.last_byte_arr_time = contact.last_byte_tx_time + contact.owlt
            prev_last_byte_arr_time = contact.last_byte_arr_time

            effective_start_time = contact.first_byte_tx_time
            min_succ_stop_time = sys.maxsize
            index = self.get_hops().index(contact)
            for successor in self.get_hops()[index:]:
                if successor.end < min_succ_stop_time:
                    min_succ_stop_time = successor.end
            effective_stop_time = min(contact.end, min_succ_stop_time)
            effective_duration = effective_stop_time - effective_start_time
            contact.effective_volume_limit = min(effective_duration * contact.rate, contact.volume)
            if contact.effective_volume_limit < min_effective_volume_limit:
                min_effective_volume_limit = contact.effective_volume_limit
        self.volume = min_effective_volume_limit

    # This method decides whether a contact can be inserted in this path
    # Modify this method to set more constraints (include owlt ?)
    def eligible(self, contact):

        try:
            return not self.visited(contact.to) \
                   and contact.end > self.get_last_contact().start + self.get_last_contact().owlt
        except IndexError:
            # if self.parent is not None:
            # return self.parent.eligible(contact)
            # else:
            return True

    # OPERATOR OVERLOAD FOR SELECTION #
    # Less than = this route is better than the other (less costly)
    def __lt__(self, other_route):
        # 1st priority : arrival time
        if self.best_delivery_time < other_route.best_delivery_time:
            return True

        # 2nd: volume
        elif self.best_delivery_time == other_route.best_delivery_time:
            if self.volume > other_route.volume:
                return True

                # 3rd: confidence
            elif self.volume == other_route.volume:
                if self.confidence >= other_route.confidence:
                    return True

        return False

    # now we do not have all the hops, so reconstruct the full path from parents
    def get_hops(self):
        if self.parent is None:
            return self.hops
        return self.parent.get_hops() + self.hops

    # utility methods
    def __repr__(self):
        return "to:%s|via:%s(%03d,%03d)|bdt:%s|hops:%s|vol:%s|conf:%s|%s" % \
               (self.to_node, self.next_node, self.from_time, self.to_time, self.best_delivery_time, len(self.get_hops()),
                self.volume, self.confidence, self.get_hops())

    def __add__(self, contact):
        return Route(contact, self)


class Bundle:
    def __init__(self, src, dst, size, deadline, priority, critical=False, sender=0, custody=False, fragment=True):
        # bundle primary block parameters
        self.src = src
        self.dst = dst
        self.size = size
        self.priority = priority
        self.critical = critical
        self.custody = custody
        self.fragment = fragment
        self.deadline = deadline

        # computed parameters
        self.evc = max(size*1.03, 100)        
        self.sender = sender


# load contact plan file with the format:
# a contact +<start> +<end> <from> <to> <rate> <range>
def cp_load(file_name, max_contacts=None):
    __contact_plan = []
    nodes = set()
    with open(file_name, 'r') as cf:
        for contact in cf.readlines():

            if contact[0] == '#':
                continue
            if not contact.startswith('a contact'):
                continue

            fields = contact.split(' ')[2:]  # ignore "a contact"
            start, end, frm, to, rate, owlt = map(int, fields)
            nodes.add(frm)
            nodes.add(to)
            __contact_plan.append(
                Contact(start=start, end=end, frm=frm, to=to, rate=rate, owlt=owlt))
            if len(__contact_plan) == max_contacts:
                break

    print('Load contact plan: %s contacts were read.' % len(__contact_plan))
    # print(__contact_plan)
    return __contact_plan


# construct a random contact plan
def cp_random(max_contacts, max_nodes):
    __contact_plan = []
    for _ in range(max_contacts):
        start = randint(0, 999)
        end = start + randint(1, 100)
        frm = randint(1, max_nodes)
        to = randint(1, max_nodes)
        while to == frm:
            to = randint(1, max_nodes)
        rate = 1
        owlt = 1
        __contact_plan.append(Contact(start=start, end=end, frm=frm, to=to, rate=rate, owlt=owlt))
    return __contact_plan


# compute the best route with dijkstra # (root_contact.arrival_time sets the current_time)
def cgr_dijkstra(root_contact, destination, contact_plan):

    debug = False

    # Clear working area of all contacts except the root, which in the case of Yens' it
    # can be any of the contacts in the plan, and requires to carry the work area intact
    for contact in contact_plan:
        if contact is not root_contact:
            contact.clear_dijkstra_working_area()

    # store contact plan in hash table (key: starting node, value: list of contacts with same from node)
    # No necessary to build every time if executed several times
    contact_plan_hash = {}
    for contact in contact_plan:
        if contact.frm not in contact_plan_hash:
            contact_plan_hash[contact.frm] = []
        if contact.to not in contact_plan_hash:
            contact_plan_hash[contact.to] = []
        contact_plan_hash[contact.frm].append(contact)

    route = None
    final_contact = None
    earliest_fin_arr_t = sys.maxsize

    current = root_contact
    if root_contact.to not in root_contact.visited_nodes:
        root_contact.visited_nodes.append(root_contact.to)

    if debug:
        print("\t\t\tDijkstra from", root_contact, " to", destination, "arrival time:", root_contact.arrival_time)
    while True:

        if debug:
            print("\t\t\tCurrent contact: ", current)
        # Calculate cost of all proximate contacts
        for contact in contact_plan_hash[current.to]:

            if debug:
                print("\t\t\t\tExplore contact: ", contact, " - ", end='')

            if contact in current.suppressed_next_hop:
                if debug:
                    print("\t\t\t\tignore (suppressed_next_hop - Yens')")
                continue
            if contact.suppressed:
                if debug:
                    print("\t\t\t\tignore (suppressed)")
                continue
            if contact.visited:
                if debug:
                    print("\t\t\t\tignore (contact visited)")
                continue
            if contact.to in current.visited_nodes:
                if debug:
                    print("\t\t\t\tignore (node visited)")
                continue
            if contact.end <= current.arrival_time:  # <= important!
                if debug:
                    print("\t\t\t\tignore (contact ends before arrival_time)", contact.end, current.arrival_time)
                continue
            if max(contact.mav) <= 0:
                if debug:
                    print("\t\t\t\tignore (no residual volume)")
                continue
            if current.frm == contact.to and current.to == contact.frm:
                if debug:
                    print("\t\t\t\tignore (return to previous node)")
                continue
            if debug:
                print("\t\t\t\tcontact not ignored - ", end='')

            # Calculate arrival time (cost)
            if contact.start < current.arrival_time:
                arrvl_time = current.arrival_time + contact.owlt
                if debug:
                    print("arrival_time: ", arrvl_time, " - ", end='')
            else:
                arrvl_time = contact.start + contact.owlt
                if debug:
                    print("arrival_time: ", arrvl_time, " - ", end='')

            # Update cost if better or equal
            if arrvl_time <= contact.arrival_time:
                if debug:
                    print("updated from: ", contact.arrival_time, " - ", end='')
                contact.arrival_time = arrvl_time
                contact.predecessor = current
                contact.visited_nodes = current.visited_nodes[:]
                contact.visited_nodes.append(contact.to)
                if debug:
                    print("visited nodes: ", contact.visited_nodes, " - ", end='')

                # Mark if destination reached
                if contact.to == destination and contact.arrival_time < earliest_fin_arr_t:
                    if debug:
                        print("marked as final! - ", end='')
                    earliest_fin_arr_t = contact.arrival_time
                    final_contact = contact
            else:
                if debug:
                    print("not updated (previous: ", contact.arrival_time, ") - ", end='')

            if debug:
                print("done")

        current.visited = True

        # Determine best next contact among all in contactPlan
        earliest_arr_t = sys.maxsize
        next_contact = 0

        for contact in contact_plan:

            # Ignore suppressed, visited
            if contact.suppressed or contact.visited:
                continue

            # If we know there is another better contact, continue
            if contact.arrival_time > earliest_fin_arr_t:
                continue

            if contact.arrival_time < earliest_arr_t:
                earliest_arr_t = contact.arrival_time
                next_contact = contact
                # print("   Next contact set to: ", next_contact)

        if next_contact == 0:
            # print("   No next contact found")
            break

        current = next_contact

    # Done contact graph exploration, check and store new route
    if final_contact is not None:

        hops = []
        contact = final_contact
        while contact != root_contact:
            hops.insert(0, contact)
            contact = contact.predecessor

        # print("route:", hops)
        route = Route(hops[0])
        for hop in hops[1:]:
            route.append(hop)

    return route


# computes all routes using deph first
def cgr_depth(source, destination, contact_plan):
    # store contact plan in hash table (key: starting node, value: list of contacts with same starting node)
    contacts = {}
    source_in_plan = False
    destination_in_plan = False
    for contact in contact_plan:
        if contact.frm == source:
            source_in_plan = True
        if contact.to == destination:
            destination_in_plan = True
        if contact.frm not in contacts:
            contacts[contact.frm] = []
        contacts[contact.frm].append(contact)

    assert (source_in_plan and destination_in_plan)

    # Now initialize the list of routes with direct contacts from source
    routes = []
    for contact in contacts[source]:
        routes.append(Route(contact))

    # finally, fill in the paths
    for i, route in enumerate(routes):
        while True:
            candidates = [c for c in contacts[routes[i].get_last_node()] if routes[i].eligible(c)]
            if candidates and routes[i].get_last_node() != destination:
                fork = False
                for other_option in candidates[1:]:
                    routes.append(routes[i] + other_option)
                    fork = True
                if fork:
                    routes[i] = routes[i] + candidates[0]
                else:
                    routes[i].append(candidates[0])
            else:
                break

    # filter out the paths that did not lead to the destination
    routes = [route for route in routes if route.get_last_node() == destination]

    # by now all possible paths should be contained in "routes".
    # Sort routes from best to worst
    routes.sort()

    return routes


# calculates best num_routes (K) routes out of a contact graph
def cgr_yen(source, destination, curr_time, contact_plan, num_routes):

    debug = False

    # best routes (container A)
    routes = []
    # potential_routes route list (container B)
    potential_routes = []

    # create Root Contact
    root_contact = Contact(source, source, 0, sys.maxsize, 100, 1.0, 0)  # root contact
    root_contact.arrival_time = curr_time

    # reset contacts
    for contact in contact_plan:
        contact.clear_dijkstra_working_area()
        contact.clear_management_working_area()

    # get first route and add the root contact as the first hop.
    # in yen's formulation we add the root contact to each new route
    # so we can use it as spur_contact as well.
    route = cgr_dijkstra(root_contact, destination, contact_plan)
    if route is None:
        return routes  # No route found, return None
    routes.append(route)
    routes[0].hops.insert(0, root_contact)

    for k in range(num_routes - 1):

        if debug:
            print("iter:", k, "last route:", routes[-1])
        for spur_contact in routes[-1].hops[:-1]:

            # create root_path from root_contact until spur_contact
            spur_contact_index = routes[-1].hops.index(spur_contact)
            if debug:
                print("\titer:", k, " spur:", spur_contact_index, "spur_contact:", spur_contact)

            root_path = Route(routes[-1].hops[0])
            for hop in routes[-1].hops[1:spur_contact_index + 1]:
                root_path.append(hop)
            if debug:
                print("\troot_path:", root_path)

            # reset contacts
            for contact in contact_plan:
                contact.clear_dijkstra_working_area()
                contact.clear_management_working_area()

            # suppress all contacts in root_path except spur_contact
            for contact in root_path.hops[:-1]:
                contact.suppressed = True
                if debug:
                    print("\t\tsuppressing node:", contact)

            # suppress all outgoing edges from spur_contact already covered by known routes
            for route in routes:
                if root_path.hops == route.hops[0:(len(root_path.hops))]:
                    if route.hops[len(root_path.hops)] not in spur_contact.suppressed_next_hop:
                        spur_contact.suppressed_next_hop.append(route.hops[len(root_path.hops)])
                        if debug:
                            print("\t\tsuppressing edge:", spur_contact, "to", route.hops[len(root_path.hops)])

            # prepare spur_contact as root contact
            spur_contact.clear_dijkstra_working_area()
            spur_contact.arrival_time = root_path.best_delivery_time
            for hop in root_path.hops:  # add visited nodes to spur_contact
                spur_contact.visited_nodes.append(hop.to)
            if debug:
                print("\t\tvisited nodes:", spur_contact.visited_nodes, "arrival time:", spur_contact.arrival_time)

            # try to find a spur_path with dijkstra
            spur_path = cgr_dijkstra(spur_contact, destination, contact_plan)

            # if found store new route in potential_routes
            if spur_path:
                total_path = Route(root_path.hops[0])
                for hop in root_path.hops[1:]:  # append root_path
                    total_path.append(hop)
                for hop in spur_path.hops:  # append spur_path
                    total_path.append(hop)
                potential_routes.append(total_path)
                if debug:
                    print("\t\t- NEW route:", total_path)
            else:
                if debug:
                    print("\t\t- no new route found")

        # if no more potential routes end search
        if not potential_routes:
            break

        # sort potential routes by arrival_time
        potential_routes.sort()

        # add best route to routes
        routes.append(potential_routes[0])
        potential_routes.pop(0)

    # remove root_contact from hops and refresh values
    for route in routes:
        # print ("routes:",route)
        route.hops.pop(0)
        route.refresh_metrics()

    return routes


# compute route list using anchor search (ION 3.6 and older)
def cgr_anchor(source, destination, curr_time, contact_plan):
    routes = []

    # create Root Contact
    root_contact = Contact(source, source, 0, sys.maxsize, 100, 1.0, 0)  # root contact
    root_contact.arrival_time = curr_time

    # reset suppressed contacts
    for contact in contact_plan:
        contact.clear_dijkstra_working_area()
        contact.clear_management_working_area()

    limit_contact = None
    anchor_contact = None
    while True:

        route = cgr_dijkstra(root_contact, destination, contact_plan)
        if not route:
            break  # no more routes in contact graph

        first_contact = route.hops[0]

        # if anchored search on-going and first_contact is no longer
        # the anchor, end anchored search and discard this route
        if anchor_contact and anchor_contact is not first_contact:
            # reset suppressed contacts
            for contact in contact_plan:
                contact.clear_dijkstra_working_area()
                if contact.frm != source:
                    contact.suppressed = True
            anchor_contact.suppressed = True
            anchor_contact = None
            continue  # go straight to next dijkstra

        routes.append(route)

        # find limiting contact and suppress it
        if route.to_time == first_contact.end:
            limit_contact = first_contact
        else:
            # the first is not a limiting contact: start anchor search
            anchor_contact = first_contact
            for contact in route.hops:
                if contact.end == route.to_time:
                    limit_contact = contact
                    break
        limit_contact.suppressed = True

        # reset working area
        for contact in contact_plan:
            contact.clear_dijkstra_working_area()

    return routes


# compute route list using first-ended (time-based search)
def cgr_ended(source, destination, curr_time, contact_plan):
    routes = []

    # create Root Contact
    root_contact = Contact(source, source, 0, sys.maxsize, 100, 1.0, 0)  # root contact
    root_contact.arrival_time = curr_time

    while True:

        route = cgr_dijkstra(root_contact, destination, contact_plan)
        if not route:
            break  # no more routes in contact graph

        # consume volume in all hops and supress limiting hop
        for hop in route.hops:
            if hop.end == route.to_time:
                hop.suppressed = True

        routes.append(route)

    return routes


# compute route list using first-depleted (capacity-oriented search)
def cgr_depleted(source, destination, curr_time, contact_plan, keep_residual_volume=False):
    routes = []

    # create Root Contact
    root_contact = Contact(source, source, 0, sys.maxsize, 100, 1.0, 0)  # root contact
    root_contact.arrival_time = curr_time

    # reset residual volume
    if not keep_residual_volume:
        for contact in contact_plan:
            contact.clear_management_working_area()

    while True:

        route = cgr_dijkstra(root_contact, destination, contact_plan)
        if not route:
            break  # no more routes in contact graph

        # consume volume in all hops and supress limiting hop
        for hop in route.hops:
            hop.effective_volume_limit -= route.volume
            if hop.effective_volume_limit == 0:
                hop.suppressed = True

        routes.append(route)

    return routes


def plot_routes(name, contact_plan, routes, source, destination):
    with open(name + '-route-graph.gdf', 'w') as f:

        # reset contacts (we use predecessor to store highest depth in path)
        for contact in contact_plan:
            contact.clear_working_area()

        # estimate max_depth and max_arrival_time of each contact
        max_depth = 0
        max_arrival_time = 0
        for route in routes:
            if route.arrival_time > max_arrival_time:
                max_arrival_time = route.arrival_time
            if len(route.get_hops()) > max_depth:
                max_depth = len(route.get_hops())
            for i, contact in enumerate(route.get_hops()):
                if contact.predecessor < (i + 1):
                    contact.predecessor = i + 1

        # write nodes
        height = [0] * (max_depth + 1)
        f.write("nodedef>name VARCHAR,label VARCHAR,x DOUBLE,y DOUBLE,labelvisible BOOLEAN, color VARCHAR\n")
        # root node
        f.write("{},{},{},{},true,green\n".format('root', source, 0, 0))
        for contact in contact_plan:
            if contact.predecessor > 0:
                f.write("{},{},{},{},true,blue\n".format(contact_plan.index(contact), contact,
                                                         contact.predecessor * 400,
                                                         height[contact.predecessor] * 100))
                if height[contact.predecessor] > 0:
                    height[contact.predecessor] = -height[contact.predecessor]
                else:
                    height[contact.predecessor] = -(height[contact.predecessor] - 1)
        # dst node
        f.write("{},{},{},{},true,green\n".format('dst', destination, (max_depth + 1) * 400, 0))

        f.write("edgedef>node1 VARCHAR,node2 VARCHAR,directed BOOLEAN,weight DOUBLE"
                ",color VARCHAR,arr_time DOUBLE\n")
        for route in routes:
            weight = 1
            for i, contact in enumerate(route.get_hops()):
                if i == 0:
                    f.write("{},{},true,{},'{},{},{}',{}\n".format('root', contact_plan.index(contact),
                                                                   weight, 0, 0, 0, route.arrival_time))
                if i > 0:
                    f.write("{},{},true,{},'{},{},{}',{}\n".format(contact_plan.index(route.get_hops()[i - 1]),
                                                                   contact_plan.index(contact), weight, 0, 0, 0,
                                                                   route.arrival_time))
                if i == (len(route.get_hops())-1):
                    f.write("{},{},true,{},'{},{},{}',{}\n".format(contact_plan.index(contact),
                                                                   'dst',
                                                                   weight,
                                                                   0, 0, 0,
                                                                   route.arrival_time))


def plot_contact_graph(name, contact_plan, source=None, destination=None):

    # determine maximum storage episode
    max_storage_time = 0
    for contact1 in contact_plan:
        for contact2 in contact_plan:
            tx_time = contact1.start
            rx_time = max(contact1.start, contact2.start)
            storage_time = rx_time - tx_time
            if storage_time > max_storage_time:
                max_storage_time = storage_time

    with open(name + '-contact-graph.gdf', 'w') as f:

        # write nodes
        f.write("nodedef>name VARCHAR,label VARCHAR,x DOUBLE,y DOUBLE,labelvisible BOOLEAN, color VARCHAR\n")
        for contact in contact_plan:
            f.write("{},{},{},{},true,blue\n".format(contact_plan.index(contact), contact, ' ', ' '))
        if source is not None:
            f.write("S,{},{},{},true,green\n".format(Contact(source, source, 0, 0, 100, 1.0, 0), ' ', ' '))
        if destination is not None:
            f.write("D,{},{},{},true,green\n".format(Contact(destination, destination, 0, 0, 100, 1.0, 0), ' ', ' '))

        # write edges
        f.write("edgedef>node1 VARCHAR,node2 VARCHAR,directed BOOLEAN,color VARCHAR,weight DOUBLE\n")
        for contact1 in contact_plan:
            for contact2 in contact_plan:
                if contact1.to == contact2.frm and contact1.start < contact2.end:
                    index1 = contact_plan.index(contact1)
                    index2 = contact_plan.index(contact2)
                    tx_time = contact1.start
                    rx_time = max(contact1.start, contact2.start)
                    storage_time = rx_time - tx_time
                    f.write("{},{},true,blue,{}\n".format(index1, index2, 1.1 - storage_time / max_storage_time))
            if contact1.frm == source:
                storage_time = contact1.start
                f.write("{},{},true,green,{}\n".format('S', contact_plan.index(contact1),
                                                       1.1 - storage_time / max_storage_time))
            if contact1.to == destination:
                storage_time = contact1.start
                f.write("{},{},true,green,{}\n".format(contact_plan.index(contact1), 'D',
                                                       1.1 - storage_time / max_storage_time))


# compute candidate routes for a bundle
def fwd_candidate(curr_time, curr_node, contact_plan, bundle, routes, excluded_nodes):

    debug = False

    return_to_sender = False
    candidate_routes = []

    for route in routes:

        # 3.2.5.2 a) preparation: backward propagation
        if not return_to_sender:
            if route.next_node is bundle.sender:
                excluded_nodes.append(route.next_node)
                if debug:
                    print("preparation: next node is sender", route.next_node)
                continue

        # 3.2.6.9 a)
        if route.best_delivery_time > bundle.deadline:
            print("not candidate: best delivery time (bdt) is later than deadline")
            continue

        # 3.2.6.9 b)
        if route.next_node in excluded_nodes:
            if debug:
                print("not candidate: next node in excluded nodes list")
            continue

        # 3.2.6.9 c)
        for contact in route.hops:
            if contact.to is curr_node:
                if debug:
                    print("not candidate: contact in route tx to current node")
                continue

        # 3.2.6.9 d) calculate eto and if it is later than 1st contact end time, ignore
        adjusted_start_time = max(curr_time, route.hops[0].start)
        applicable_backlog_p = 0  # todo: this the current route.next_node queue status now for p or higher
        applicable_backlog_relief = 0
        for contact in contact_plan:
            if contact.frm == route.hops[0].frm and contact.to == route.hops[0].to:
                if contact.end > curr_time and contact.start < route.hops[0].start:
                    applicable_duration = contact.end - max(curr_time, contact.start)
                    applicable_prior_contact_volume = applicable_duration * contact.rate
                    applicable_backlog_relief += applicable_prior_contact_volume
        residual_backlog = max(0, applicable_backlog_p - applicable_backlog_relief)
        backlog_lien = residual_backlog / route.hops[0].rate
        early_tx_opportunity = adjusted_start_time + backlog_lien
        if early_tx_opportunity > route.hops[0].end:
            if debug:
                print("not candidate: earlier transmission opportunity is later than end of 1st contact")
            continue

        # 3.2.6.9 e) use eto to compute projected arrival time
        prev_last_byte_arr_time = 0
        for contact in route.hops:
            if contact == route.hops[0]:
                contact.first_byte_tx_time = early_tx_opportunity
            else:
                contact.first_byte_tx_time = max(contact.start, prev_last_byte_arr_time)
            bundle_tx_time = bundle.size / contact.rate
            contact.last_byte_tx_time = contact.first_byte_tx_time + bundle_tx_time
            contact.last_byte_arr_time = contact.last_byte_tx_time + contact.owlt
            prev_last_byte_arr_time = contact.last_byte_arr_time
        proj_arr_time = prev_last_byte_arr_time
        if proj_arr_time > bundle.deadline:
            if debug:
                print("not candidate: projected arrival time is later than deadline")
            continue

        # 3.2.6.9 f) if route depleted for bundle priority P, ignore
        reserved_volume_p = 0  # todo: sum of al bundle.evc with p or higher that were forwarded via this route
        min_effective_volume_limit = sys.maxsize
        for contact in route.hops:
            if reserved_volume_p >= contact.volume:
                if debug:
                    print("not candidate: route depleted for bundle priority")
                continue

            effective_start_time = contact.first_byte_tx_time
            min_succ_stop_time = sys.maxsize
            index = route.hops.index(contact)
            for successor in route.hops[index:]:
                if successor.end < min_succ_stop_time:
                    min_succ_stop_time = successor.end
            effective_stop_time = min(contact.end, min_succ_stop_time)
            effective_duration = effective_stop_time - effective_start_time
            contact.effective_volume_limit = min(effective_duration * contact.rate,
                                                 contact.mav[bundle.priority])
            if contact.effective_volume_limit < min_effective_volume_limit:
                min_effective_volume_limit = contact.effective_volume_limit
        route_volume_limit = min_effective_volume_limit
        if route_volume_limit <= 0:
            if debug:
                print("not candidate: route is depleted for the bundle priority")
            continue

        # 3.2.6.9 g) if frag is False and route rvl(P) < bundle.evc, ignore
        if not bundle.fragment:
            if route_volume_limit < bundle.evc:
                if debug:
                    print("not candidate: route volume limit is less than bundle evc and no fragment allowed")
                continue

        if debug:
            print("new candidate:", route)
        candidate_routes.append(route)

    candidate_routes.sort()

    return candidate_routes

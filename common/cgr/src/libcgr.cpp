#include "libcgr.h"

#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>

#include <iostream>
#include <queue>

namespace cgr {

/*
 * Class method implementations.
 */
void Contact::clear_dijkstra_working_area() {
    arrival_time = MAX_SIZE;
    visited = false;
    predecessor = NULL;
    visited_nodes.clear();
}

bool Contact::operator==(const Contact contact) const {
    return (frm == contact.frm &&
        to == contact.to &&
        start == contact.start &&
        end == contact.end &&
        rate == contact.rate &&
        owlt == contact.owlt &&
        confidence == contact.confidence);
}

bool Contact::operator!=(const Contact contact) const {
    return !(*this == contact);
}

Contact::Contact(nodeId_t frm, nodeId_t to, int start, int end, int rate, float confidence, int owlt)
    : frm(frm), to(to), start(start), end(end), rate(rate), confidence(confidence), owlt(owlt)
{
    // fixed parameters
    volume = rate * (end - start);

    // variable parameters
    mav = std::vector<int>({ volume, volume, volume });

    // route search working area
    arrival_time = MAX_SIZE;
    visited = false;
    visited_nodes = std::vector<nodeId_t>();
    predecessor = NULL;

    // route management working area
    suppressed = false;
    suppressed_next_hop = std::vector<Contact>();

    // forwarding working area
    // first_byte_tx_time = NULL;
    // last_byte_tx_time = NULL;
    // last_byte_arr_time = NULL;
    // effective_volume_limit = NULL;
}

Contact::Contact()
{
}

Contact::~Contact() {}

Route::Route()
{
}

Route::~Route() {}

Route::Route(Contact contact, Route* parent)
    : parent(parent)
{
    hops = std::vector<Contact>();
    if (NULL == parent) {
        // to_node = NULL;
        // next_node = NULL;
        from_time = 0;
        to_time = MAX_SIZE;
        best_delivery_time = 0;
        volume = MAX_SIZE;
        confidence = 1;
        __visited = std::map<nodeId_t, bool>();
    }
    else {
        to_node = parent->to_node;
        next_node = parent->next_node;
        from_time = parent->from_time;
        to_time = parent->to_time;
        best_delivery_time = parent->best_delivery_time;
        volume = parent->volume;
        confidence = parent->confidence;
        __visited = std::map<nodeId_t, bool>(parent->__visited); // copy
    }

    append(contact);
}

Contact Route::get_last_contact() {
    if (hops.empty()) {
        throw EmptyContainerError();
    }
    else {
        return hops.back();
    }
}

bool Route::visited(nodeId_t node) {
    //const int id = node;
    return __visited.count(node) && __visited[node];
}

void Route::append(Contact contact) {
    assert(eligible(contact));
    hops.push_back(contact);
    __visited[contact.frm] = true;
    __visited[contact.to] = true;

    refresh_metrics();
}

void Route::refresh_metrics() {
    assert(!hops.empty());
    std::vector<Contact> allHops = get_hops();
    to_node = allHops.back().to;
    next_node = allHops[0].to;
    from_time = allHops[0].start;
    to_time = MAX_SIZE;
    best_delivery_time = 0;
    confidence = 1;
    for (Contact contact : allHops) {
        to_time = std::min(to_time, contact.end);
        best_delivery_time = std::max(best_delivery_time + contact.owlt, contact.start + contact.owlt);
        confidence *= contact.confidence;
    }

    // volume
    int prev_last_byte_arr_time = 0;
    int min_effective_volume_limit = MAX_SIZE;
    for (Contact& contact : allHops) {
        if (contact == allHops[0]) {
            contact.first_byte_tx_time = contact.start;
        }
        else {
            contact.first_byte_tx_time = std::max(contact.start, prev_last_byte_arr_time);
        }
        int bundle_tx_time = 0;
        contact.last_byte_tx_time = contact.first_byte_tx_time + bundle_tx_time;
        contact.last_byte_arr_time = contact.last_byte_tx_time + contact.owlt;
        prev_last_byte_arr_time = contact.last_byte_arr_time;

        int effective_start_time = contact.first_byte_tx_time;
        int min_succ_stop_time = MAX_SIZE;
        std::vector<Contact>::iterator it = std::find(allHops.begin(), allHops.end(), contact);
        for (it; it < allHops.end(); ++it) {
            Contact successor = *it;
            if (successor.end < min_succ_stop_time) {
                min_succ_stop_time = successor.end;
            }
        }
        int effective_stop_time = std::min(contact.end, min_succ_stop_time);
        int effective_duration = effective_stop_time - effective_start_time;
        contact.effective_volume_limit = std::min(effective_duration * contact.rate, contact.volume);
        if (contact.effective_volume_limit < min_effective_volume_limit) {
            min_effective_volume_limit = contact.effective_volume_limit;
        }
    }
    volume = min_effective_volume_limit;
}

bool Route::eligible(Contact contact) {
    try {
        Contact last = get_last_contact();
        return (!visited(contact.to) && contact.end > last.start + last.owlt);
    }
    catch (EmptyContainerError) {
        return true;
    }
}

std::vector<Contact> Route::get_hops() {
    if (NULL == parent) {
        return hops;
    }
    else {
        std::vector<Contact> v(parent->get_hops());
        v.insert(v.end(), hops.begin(), hops.end());
        return v;
    }
}
Vertex::Vertex() {
    // this should never be used
}

Vertex::Vertex(nodeId_t node_id) {
    id = node_id;
    adjacencies = std::unordered_map<nodeId_t, std::vector<int>>();
    arrival_time = MAX_SIZE;
    visited = false;
    predecessor = NULL;
}

bool Vertex::operator<(const Vertex& v) const {
    return arrival_time < v.arrival_time;
}

ContactMultigraph::ContactMultigraph(std::vector<Contact> contact_plan, nodeId_t dest_id) {
    vertices = std::unordered_map<nodeId_t, Vertex>();
    auto vertices_end = vertices.end();
    //for (Contact& contact : contact_plan) {
    for (int contact_i = 0; contact_i < contact_plan.size(); ++contact_i) {
        Contact& contact = contact_plan[contact_i];
        if (vertices.find(contact.frm) == vertices_end) {
            Vertex frm(contact.frm);

            //std::vector<Contact> adj = frm.adjacencies[contact.to]; // get the right list of contacts to this adjacency, will instantiate it as well
            //adj.push_back(contact);

            frm.adjacencies[contact.to].push_back(contact_i);

            vertices.insert({ contact.frm, frm });
        }
        else {
            //Vertex frm = vertices[contact.frm];
            std::vector<int> adj = vertices[contact.frm].adjacencies[contact.to];
            // if the map can't find the key it creates a default constructed element for it
            // https://stackoverflow.com/questions/10124679/what-happens-if-i-read-a-maps-value-where-the-key-does-not-exist
            if (adj.empty() || contact.start > contact_plan[adj.back()].start) {
                vertices[contact.frm].adjacencies[contact.to].push_back(contact_i);
            }
            else {
                // insert contact sorted by start time
                // assuming non-overlapping contacts

                // turn indices into contacts
                std::vector<Contact> adj_contacts;
                for (int i = 0; i < adj.size(); ++i) {
                    adj_contacts[i] = contact_plan[adj[i]];
                }


                int index = cgr::contact_search_index(adj_contacts, contact.start);
                vertices[contact.frm].adjacencies[contact.to].insert(vertices[contact.frm].adjacencies[contact.to].begin() + index, contact_i);
            }
        }
    }
    if (vertices.find(dest_id) == vertices_end) {
        Vertex dest(dest_id);
        vertices.insert({ dest_id, dest });
    }

    predecessors = std::unordered_map<nodeId_t, int>();
    visited = std::unordered_map<nodeId_t, bool>();
    arrival_time = std::unordered_map<nodeId_t, int>();
    for (auto v : vertices) {
        visited[v.first] = false;
        predecessors[v.first] = MAX_SIZE; // just a way to identify. really should make integer instead of int then set to null
        arrival_time[v.first] = MAX_SIZE;
    }

    /*for (Contact &c : vertices[5].adjacencies[4]) {
        std::cout << &c << std::endl;
    }*/
}


/*
* Library function implementations, e.g. loading, routing algorithms, etc.
*/
std::vector<Contact> cp_load(std::string filename, int max_contacts) {
    std::vector<Contact> contactsVector;
    boost::property_tree::ptree pt;
    boost::property_tree::read_json(filename, pt);
    const boost::property_tree::ptree& contactsPt
        = pt.get_child("contacts", boost::property_tree::ptree());
    for (const boost::property_tree::ptree::value_type& eventPt : contactsPt) {
        Contact new_contact = Contact(eventPt.second.get<int>("source", 0),
            eventPt.second.get<int>("dest", 0),
            static_cast<int>(eventPt.second.get<int>("startTime", 0)),
            eventPt.second.get<int>("endTime", 0),
            eventPt.second.get<int>("rate", 0));
        // new_contact.id = eventPt.second.get<int>("contact", 0);
        contactsVector.push_back(new_contact);
        if (contactsVector.size() == max_contacts) {
            break;
        }
    }
    return contactsVector;
}

Route dijkstra(Contact* root_contact, nodeId_t destination, std::vector<Contact> contact_plan) {
    // Need to clear the real contacts in the contact plan
    // so we loop using Contact& instead of Contact
    for (Contact& contact : contact_plan) {
        if (contact != *root_contact) {
            contact.clear_dijkstra_working_area();
        }
    }

    // Make sure we map to pointers so we can modify the underlying contact_plan
    // using the hashmap. The hashmap helps us find the neighbors of a node.
    std::map<nodeId_t, std::vector<Contact*>> contact_plan_hash;
    for (Contact& contact : contact_plan) {
        if (!contact_plan_hash.count(contact.frm)) {
            contact_plan_hash[contact.frm] = std::vector<Contact*>();
        }
        if (!contact_plan_hash.count(contact.to)) {
            contact_plan_hash[contact.to] = std::vector<Contact*>();
        }
        contact_plan_hash[contact.frm].push_back(&contact);
    }

    Route route;
    Contact* final_contact = NULL;
    int earliest_fin_arr_t = MAX_SIZE;
    int arrvl_time;

    Contact* current = root_contact;

    if (!vector_contains(root_contact->visited_nodes, root_contact->to)) {
        root_contact->visited_nodes.push_back(root_contact->to);
    }

    while (true) {
        // loop over the neighbors of the current contact's source node
        for (Contact* contact : contact_plan_hash[current->to]) {
            if (vector_contains(current->suppressed_next_hop, *contact)) {
                continue;
            }
            if (contact->suppressed) {
                continue;
            }
            if (contact->visited) {
                continue;
            }
            if (vector_contains(current->visited_nodes, contact->to)) {
                continue;
            }
            if (contact->end <= current->arrival_time) {
                continue;
            }
            if (*std::max_element(contact->mav.begin(), contact->mav.end()) <= 0) {
                continue;
            }
            if (current->frm == contact->to && current->to == contact->frm) {
                continue;
            }

            // Calculate arrival time (cost)
            if (contact->start < current->arrival_time) {
                arrvl_time = current->arrival_time + contact->owlt;
            }
            else {
                arrvl_time = contact->start + contact->owlt;
            }

            if (arrvl_time <= contact->arrival_time) {
                contact->arrival_time = arrvl_time;
                contact->predecessor = current;
                contact->visited_nodes = current->visited_nodes;
                contact->visited_nodes.push_back(contact->to);

                if (contact->to == destination && contact->arrival_time < earliest_fin_arr_t) {
                    earliest_fin_arr_t = contact->arrival_time;
                    final_contact = &(*contact);
                }
            }
        }

        current->visited = true;

        // determine next best contact
        int earliest_arr_t = MAX_SIZE;
        Contact* next_contact = NULL;
        // @source DtnSim
        // "Warning: we need to point finalContact to
        // the real contact in contactPlan..."
        // This is why we loop with a Contact& rather than a Contact
        for (Contact& contact : contact_plan) {
            if (contact.suppressed || contact.visited) {
                continue;
            }
            if (contact.arrival_time > earliest_fin_arr_t) {
                continue;
            }
            if (contact.arrival_time < earliest_arr_t) {
                earliest_arr_t = contact.arrival_time;
                next_contact = &contact;
            }
        }

        if (NULL == next_contact) {
            break;
        }

        current = next_contact;
    }

    if (final_contact != NULL) {
        std::vector<Contact> hops;
        Contact contact;
        for (contact = *final_contact; contact != *root_contact; contact = *contact.predecessor) {
            hops.push_back(contact);
        }

        route = Route(hops.back());
        hops.pop_back();
        while (!hops.empty()) {
            route.append(hops.back());
            hops.pop_back();
        }
    }

    return route;


}

/*
 * Helper functions
 */
template <typename T>
bool vector_contains(std::vector<T> vec, T ele) {
    typename std::vector<T>::iterator it = std::find(vec.begin(), vec.end(), ele);
    return it != std::end(vec);
}

// Throw this exception in methods that would otherwise try to access an empty container
// E.g. calling std::vector::back() on an empty vector is undefined behavior.
// Following the approach of the Python version CGR library, we use this class for Route::eligible()
// as a substitue for Python's IndexError
const char* EmptyContainerError::what() const throw() {
    return "Tried to access element of an empty container";
}

std::ostream& operator<<(std::ostream& out, const std::vector<Contact>& obj) {
    out << "[";
    for (int i = 0; i < obj.size() - 1; i++) {
        out << obj[i] << ", ";
    }
    out << obj[obj.size() - 1] << "]";

    return out;
}

std::ostream& operator<<(std::ostream& out, const Contact& obj) {
    static const boost::format fmtTemplate("%d->%d(%d-%d,d%d)[mav%.0f%%]");
    boost::format fmt(fmtTemplate);

    int min_vol = *std::min_element(obj.mav.begin(), obj.mav.end());
    double volume = 100.0 * min_vol / obj.volume;
    fmt% obj.frm% obj.to% obj.start% obj.end% obj.owlt% volume;
    const std::string message(std::move(fmt.str()));

    out << message;
    return out;
}

std::ostream& operator<<(std::ostream& out, const Route& obj) {
    static const boost::format fmtTemplate("to:%d|via:%d(%03d,%03d)|bdt:%d|hops:%d|vol:%d|conf:%.1f|%s");
    boost::format fmt(fmtTemplate);

    std::vector<Contact> routeHops = static_cast<Route>(obj).get_hops();

    fmt% obj.to_node% obj.next_node% obj.from_time% obj.to_time% obj.best_delivery_time
        % routeHops.size() % obj.volume% obj.confidence% routeHops;
    const std::string message(std::move(fmt.str()));

    out << message;
    return out;
}

/*
* Multigraph Routing
*/

// finds contact C in contacts with smallest end time
// s.t. C.end >= arrival_time && C.start <= arrival time
// assumes non-overlapping intervals - would want to optimize if they do overlap
Contact contact_search(std::vector<Contact>& contacts, int arrival_time) {
    int index = contact_search_index(contacts, arrival_time);
    return contacts[index];
}

int contact_search_index(std::vector<Contact>& contacts, int arrival_time) {
    int left = 0;
    int right = contacts.size() - 1;
    if (contacts[left].end > arrival_time) {
        return left;
    }
    int mid;
    while (left < right - 1) {
        mid = (left + right) / 2;
        if (contacts[mid].end > arrival_time) {
            right = mid;
        }
        else {
            left = mid;
        }
    }
    return right;
}

// made this function because of implementation issues
//Contact* contact_search_predecessor(std::vector<Contact>& contacts, int arrival_time) {
//    int index = contact_search_index(contacts, arrival_time);
//    return &contacts[index];
//}

int contact_search_predecessor(std::vector<int>& contacts_i, int& arrival_time, std::vector<Contact>& contact_plan) {
    int left = 0;
    int right = contacts_i.size() - 1;
    if (contact_plan[contacts_i[left]].end > arrival_time) {
        return contacts_i[left];
    }
    int mid;
    while (left < right - 1) {
        mid = (left + right) / 2;
        if (contact_plan[contacts_i[mid]].end > arrival_time) {
            right = mid;
        }
        else {
            left = mid;
        }
    }
    return contacts_i[right];
}


// multigraph review procedure
// modifies PQ
//void MRP(ContactMultigraph &CM, std::priority_queue<Vertex, std::vector<Vertex>, CompareArrivals> &PQ, Vertex &v_curr) {
//    // address testing good here (self note)
//    for (auto adj : v_curr.adjacencies) {
//        Vertex u = CM.vertices[adj.first];
//        if (CM.visited[u.id]) {
//            continue;
//        }
//        // check if there is any viable contact
//        std::vector<Contact> v_curr_to_u = v_curr.adjacencies[u.id];
//        if (v_curr_to_u.back().end < CM.arrival_time[v_curr.id]) {
//            continue;
//        }
//        // find earliest usable contact from v_curr to u
//        Contact best_contact = contact_search(v_curr_to_u, CM.arrival_time[v_curr.id]);
//        // should owlt_mgn be included in best arrival time?
//        int best_arr_time = std::max(best_contact.start, CM.arrival_time[v_curr.id]) + best_contact.owlt;
//        if (best_arr_time < CM.arrival_time[u.id]) {
//            CM.arrival_time[u.id] = best_arr_time;
//            // update PQ
//            // using "lazy deletion"
//            // Source: https://stackoverflow.com/questions/9209323/easiest-way-of-using-min-priority-queue-with-key-update-in-c
//            //u.predecessor = contact_search_predecessor(v_curr_to_u, v_curr.arrival_time); old way
//            Contact* p = contact_search_predecessor(v_curr_to_u, v_curr.arrival_time);
//            CM.predecessors[u.id] = p;
//
//            // still want to update u node's arrival time for sake of pq
//            u.arrival_time = best_arr_time;
//            PQ.push(u); // c++ priority_queue allows duplicate values
//        }
//    }
//    CM.visited[v_curr.id] = true;
//}

Route cmr_dijkstra(Contact* root_contact, nodeId_t destination, std::vector<Contact> contact_plan) {
    // Construct Contact Multigraph from Contact Plan
    ContactMultigraph CM(contact_plan, destination);

    // Default construction for each vertex sets arrival time to infinity,
    // visited to false, predecessor to null
    //CM.vertices[root_contact->frm].arrival_time = root_contact->start; old outdated way
    CM.arrival_time[root_contact->frm] = root_contact->start;


    // Construct min PQ ordered by arrival time
    std::priority_queue<Vertex, std::vector<Vertex>, CompareArrivals> PQ;
    for (auto v : CM.vertices) {
        PQ.push(v.second);
    }
    Vertex v_curr;
    Vertex v_next;
    v_curr = PQ.top();
    PQ.pop();
    while (true) {
        //MRP(CM, PQ, v_curr); // want to make inline?

        // MRP ----------

        for (auto adj : v_curr.adjacencies) {
            Vertex u = CM.vertices[adj.first];
            if (CM.visited[u.id]) {
                continue;
            }
            // check if there is any viable contact
            std::vector<int> v_curr_to_u_ind = v_curr.adjacencies[u.id];
            // turn array of indices into array of contacts
            std::vector<Contact> v_curr_to_u;
            for (int i = 0; i < v_curr_to_u_ind.size(); ++i) {
                v_curr_to_u.push_back(contact_plan[v_curr_to_u_ind[i]]);
            }

            if ((v_curr_to_u.back().end < CM.arrival_time[v_curr.id]) && (CM.arrival_time[v_curr.id] != MAX_SIZE)) {
                continue;
            }
            // find earliest usable contact from v_curr to u

            Contact best_contact = contact_search(v_curr_to_u, CM.arrival_time[v_curr.id]);
            // should owlt_mgn be included in best arrival time?
            int best_arr_time = std::max(best_contact.start, CM.arrival_time[v_curr.id]) + best_contact.owlt;
            if (best_arr_time < CM.arrival_time[u.id]) {
                CM.arrival_time[u.id] = best_arr_time;
                // update PQ
                // using "lazy deletion"
                // Source: https://stackoverflow.com/questions/9209323/easiest-way-of-using-min-priority-queue-with-key-update-in-c
                //u.predecessor = contact_search_predecessor(v_curr_to_u, v_curr.arrival_time); old way
                int p_i = contact_search_predecessor(v_curr_to_u_ind, CM.arrival_time[v_curr.id], contact_plan);
                CM.predecessors[u.id] = p_i;

                // still want to update u node's arrival time for sake of pq
                u.arrival_time = best_arr_time;
                PQ.push(u); // c++ priority_queue allows duplicate values
            }
        }
        CM.visited[v_curr.id] = true;



        // MRP -----------



        v_next = PQ.top();
        //std::cout << PQ.top().id << std::endl;
        PQ.pop();
        if (v_next.id == destination) {
            break;
        }
        else {
            v_curr = v_next;
        }
    }


    // test prints for the sake of debugging

    /*  std::cout << "--- Contact Plan ---" << std::endl;
    for (Contact &c : contact_plan) {
        std::cout << c << std::endl;
    }
    std::cout << "--- Predecessors ---" << std::endl;
    for (auto pr : CM.predecessors) {
        if (pr.first == 1) {
            continue;
        }
        std::cout << "Vertex " << pr.first << ": ";
        std::cout << contact_plan[pr.second] << std::endl;
    }*/


    // construct route from contact predecessors
    // can use parts of Timothy's code because we are storing predecessors as contacts
    // removed case to check if the final contact is null - I think exiting the above loop verifies that
    // Raises the question: how to exit if path isn't found
    std::vector<Contact> hops;
    Contact contact;
    for (contact = contact_plan[CM.predecessors[v_next.id]]; contact.frm != contact.to; contact = contact_plan[CM.predecessors[CM.vertices[contact.frm].id]]) {
        hops.push_back(contact);
        // to avoid segfault, potential fix
        if (contact.frm == root_contact->frm) { // meaning if we've just inserted our first contact
            break;
        }
    }
    Route route;
    route = Route(hops.back());
    hops.pop_back();
    while (!hops.empty()) {
        route.append(hops.back());
        hops.pop_back();
    }
    return route;
}


} // namespace cgr

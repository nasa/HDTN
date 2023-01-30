#include "libcgr.h"

#include <queue>
#include "Logger.h"
#include "JsonSerializable.h"
#include <boost/format.hpp>

namespace cgr {

static constexpr hdtn::Logger::SubProcess subprocess = hdtn::Logger::SubProcess::none;

/*
 * Class method implementations.
 */
void Contact::clear_dijkstra_working_area() {
    arrival_time = MAX_TIME_T;
    visited = false;
    predecessor = NULL;
    visited_nodes.clear();
}

bool Contact::operator==(const Contact contact) const {
    // alternatively, just check that id == contact.id ?
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

Contact::Contact(nodeId_t frm, nodeId_t to, time_t start, time_t end, uint64_t rate, float confidence, time_t owlt)
    : frm(frm), to(to), start(start), end(end), rate(rate), owlt(owlt), confidence(confidence)
{
    // fixed parameters
    volume = rate * (end - start);

    // variable parameters
//    mav = std::vector<uint64_t>({volume, volume, volume});
    mav = std::vector<uint64_t>({ volume, volume, volume });

    // route search working area
    arrival_time = MAX_TIME_T;
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

Contact::~Contact() {}

Route::Route() :
    parent(NULL),
    to_node(std::numeric_limits<nodeId_t>::max()),
    next_node(std::numeric_limits<nodeId_t>::max()),
    from_time(0),
    to_time(MAX_TIME_T),
    best_delivery_time(0),
    volume(std::numeric_limits<uint64_t>::max()),
    confidence(1),
    __visited(std::map<nodeId_t, bool>())
{
}

Route::~Route() {}

Route::Route(const Contact & contact, Route* parent)
    : parent(parent)
{
    hops = std::vector<Contact>();
    if (NULL == parent) {
        to_node = std::numeric_limits<nodeId_t>::max();
        next_node = std::numeric_limits<nodeId_t>::max();
        from_time = 0;
        to_time = MAX_TIME_T;
        best_delivery_time = 0;
        volume = std::numeric_limits<uint64_t>::max();
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
    return __visited.count(node) && __visited[node];
}

bool Route::valid() const noexcept {
    //return ((parent != NULL) && (!hops.empty()));
    return (!hops.empty());
}

void Route::append(const Contact &contact) {
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
    to_time = MAX_TIME_T;
    best_delivery_time = 0;
    confidence = 1;
    for (const Contact& contact : allHops) {
        to_time = std::min(to_time, contact.end);
        best_delivery_time = std::max(best_delivery_time + contact.owlt, contact.start + contact.owlt);
        confidence *= contact.confidence;
    }

    // volume
    time_t prev_last_byte_arr_time = 0;
    uint64_t min_effective_volume_limit = std::numeric_limits<uint64_t>::max();
    for (Contact& contact : allHops) {
        if (contact == allHops[0]) {
            contact.first_byte_tx_time = contact.start;
        }
        else {
            contact.first_byte_tx_time = std::max(contact.start, prev_last_byte_arr_time);
        }
        time_t bundle_tx_time = 0;
        contact.last_byte_tx_time = contact.first_byte_tx_time + bundle_tx_time;
        contact.last_byte_arr_time = contact.last_byte_tx_time + contact.owlt;
        prev_last_byte_arr_time = contact.last_byte_arr_time;

        time_t effective_start_time = contact.first_byte_tx_time;
        time_t min_succ_stop_time = MAX_TIME_T;
        std::vector<Contact>::iterator it = std::find(allHops.begin(), allHops.end(), contact);
        for (it; it < allHops.end(); ++it) {
            Contact successor = *it;
            if (successor.end < min_succ_stop_time) {
                min_succ_stop_time = successor.end;
            }
        }
        time_t effective_stop_time = std::min(contact.end, min_succ_stop_time);
        time_t effective_duration = effective_stop_time - effective_start_time;
        contact.effective_volume_limit = std::min(static_cast<uint64_t>(effective_duration) * contact.rate, contact.volume);
        if (contact.effective_volume_limit < min_effective_volume_limit) {
            min_effective_volume_limit = contact.effective_volume_limit;
        }
    }
    volume = min_effective_volume_limit;
}

bool Route::eligible(const Contact &contact) {
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
    LOG_WARNING(subprocess) << "default vertex constructor used";
}

Vertex::Vertex(nodeId_t node_id) {
    id = node_id;
    adjacencies.clear();
    vertex_arrival_time = MAX_TIME_T;
    visited = false;
    predecessor = NULL;
}
//a copy constructor: X(const X&)
Vertex::Vertex(const Vertex& o) :
    id(o.id),
    adjacencies(o.adjacencies),
    vertex_arrival_time(o.vertex_arrival_time),
    visited(o.visited),
    predecessor(o.predecessor) { }

//a move constructor: X(X&&)
Vertex::Vertex(Vertex&& o) :
    id(o.id),
    adjacencies(std::move(o.adjacencies)),
    vertex_arrival_time(o.vertex_arrival_time),
    visited(o.visited),
    predecessor(o.predecessor) { }

//a copy assignment: operator=(const X&)
Vertex& Vertex::operator=(const Vertex& o) {
    id = o.id;
    adjacencies = o.adjacencies;
    vertex_arrival_time = o.vertex_arrival_time;
    visited = o.visited;
    predecessor = o.predecessor;
    return *this;
}

//a move assignment: operator=(X&&)
Vertex& Vertex::operator=(Vertex&& o) {
    id = o.id;
    adjacencies = std::move(o.adjacencies);
    vertex_arrival_time = o.vertex_arrival_time;
    visited = o.visited;
    predecessor = o.predecessor;
    return *this;
}

bool Vertex::operator<(const Vertex& v) const {
    return vertex_arrival_time < v.vertex_arrival_time;
}

ContactMultigraph::ContactMultigraph(const std::vector<Contact>& contact_plan, nodeId_t dest_id) {
    m_nodeMap.clear();
    m_nodeMap.reserve(500); //todo add parameter for number of contacts to set num buckets of map
    static constexpr std::size_t VERTEX_ADJACENCY_NUM_BUCKETS = 100; //todo add parameter?
    for (std::size_t contact_i = 0; contact_i < contact_plan.size(); ++contact_i) {
        const Contact& contact = contact_plan[contact_i];
#if (__cplusplus >= 201703L)
        std::pair<cmr_node_map_t::iterator, bool> pairContactFrom = m_nodeMap.try_emplace(contact.frm, contact.frm); //try_emplace would be most ideal so it doesnt create and destroy element if exists
#else
        std::pair<cmr_node_map_t::iterator, bool> pairContactFrom = m_nodeMap.emplace(contact.frm, contact.frm); //try_emplace would be most ideal so it doesnt create and destroy element if exists
#endif
        cmr_node_map_t::iterator & itContactFrom = pairContactFrom.first;
        if (pairContactFrom.second) { //true => insertion happened (i.e. not found)
            Vertex & frm = itContactFrom->second.m_vertex;
            frm.adjacencies.reserve(VERTEX_ADJACENCY_NUM_BUCKETS);
            frm.adjacencies[contact.to].push_back(contact_i);
        }
        else { //found
            std::vector<nodeId_t> & adj = itContactFrom->second.m_vertex.adjacencies[contact.to];
            if (adj.empty() || contact.start > contact_plan[adj.back()].start) {
                adj.push_back(contact_i);
            }
            else {
                std::vector<const Contact *> adj_contacts(adj.size());
                for (std::size_t i = 0; i < adj.size(); ++i) {
                    adj_contacts[i] = &(contact_plan[adj[i]]);
                }
                int64_t index = cgr::contact_search_index(adj_contacts, contact.start); //adj_contacts.size() must be non-zero which it is
                adj.insert(adj.begin() + index, contact_i);
            }
        }
    }
#if (__cplusplus >= 201703L)
    m_nodeMap.try_emplace(dest_id, dest_id);
#else
    m_nodeMap.emplace(dest_id, dest_id); //try_emplace would be most ideal so it doesnt create and destroy element if exists
#endif
    
}

ContactMultigraph::CmrMapData::CmrMapData(Vertex&& v) : m_vertex(std::move(v)), m_visited(false), m_predecessorNodeId(std::numeric_limits<nodeId_t>::max()), m_arrivalTime(MAX_TIME_T) {}
ContactMultigraph::CmrMapData::CmrMapData(nodeId_t nodeId) : m_vertex(nodeId), m_visited(false), m_predecessorNodeId(std::numeric_limits<nodeId_t>::max()), m_arrivalTime(MAX_TIME_T) {}
ContactMultigraph::CmrMapData::CmrMapData() { LOG_WARNING(subprocess) << "default CmrMapData constructor used"; }


/*
 * Library function implementations, e.g. loading, routing algorithms, etc.
 */
std::vector<Contact> cp_load(const boost::filesystem::path& filePath, std::size_t max_contacts) {
    std::vector<Contact> contactsVector;
    
    boost::property_tree::ptree pt;
    if (JsonSerializable::GetPropertyTreeFromJsonFilePath(filePath, pt)) { //prints error if can't find

        //for non-throw versions of get_child which return a reference to the second parameter
        static const boost::property_tree::ptree EMPTY_PTREE;

        const boost::property_tree::ptree& contactsPt = pt.get_child("contacts", EMPTY_PTREE);
        contactsVector.reserve(contactsPt.size());
        for (const boost::property_tree::ptree::value_type& eventPt : contactsPt) {
            contactsVector.emplace_back( //nodeId_t frm, nodeId_t to, time_t start, time_t end, uint64_t rate, float confidence=1, time_t owlt=1
                eventPt.second.get<nodeId_t>("source", 0), //nodeId_t frm
                eventPt.second.get<nodeId_t>("dest", 0), //nodeId_t to
                eventPt.second.get<time_t>("startTime", 0), //time_t start
                eventPt.second.get<time_t>("endTime", 0), //time_t end
                eventPt.second.get<uint64_t>("rate", 0), //uint64_t rate
                1.f, //float confidence=1
                eventPt.second.get<time_t>("owlt", 0)); //time_t owlt=1
            contactsVector.back().id = eventPt.second.get<uint64_t>("contact", 0);
            if (contactsVector.size() == max_contacts) {
                LOG_WARNING(subprocess) << "HIT MAX CONTACTS!!!!!!!!!!!!!!!!!!!";
                break;
            }
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
    Contact *final_contact = NULL;
    time_t earliest_fin_arr_t = MAX_TIME_T;
    time_t arrvl_time;

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
        time_t earliest_arr_t = MAX_TIME_T;
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
        Contact contact = *final_contact;
        for (; contact != *root_contact; contact = *contact.predecessor) {
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
    for (std::size_t i = 0; i < obj.size() - 1; i++) {
        out << obj[i] << ", ";
    }
    out << obj[obj.size() - 1] << "]";

    return out;
}

std::ostream& operator<<(std::ostream& out, const Contact& obj) {
    static const boost::format fmtTemplate("%d->%d(%d-%d,d%d)[mav%.0f%%]");
    boost::format fmt(fmtTemplate);

    uint64_t min_vol = *std::min_element(obj.mav.begin(), obj.mav.end());
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
const Contact & contact_search(const std::vector<const Contact*>& contacts, time_t arrival_time) {
    int64_t index = contact_search_index(contacts, arrival_time);
    return *(contacts[index]);
}

int64_t contact_search_index(const std::vector<const Contact*>& contacts, time_t arrival_time) {
    int64_t left = 0;
    int64_t right = contacts.size() - 1;
    if (contacts[left]->end > arrival_time) {
        return left;
    }
    int64_t mid;
    while (left < right - 1) {
        mid = (left + right) / 2;
        if (contacts[mid]->end > arrival_time) {
            right = mid;
        }
        else {
            left = mid;
        }
    }
    return right;
}

uint64_t contact_search_predecessor(const std::vector<uint64_t>& contacts_i, time_t arrival_time, const std::vector<Contact>& contact_plan) {
    int64_t left = 0;
    int64_t right = static_cast<int64_t>(contacts_i.size()) - 1;
    if (contact_plan[contacts_i[left]].end > arrival_time) {
        return contacts_i[left];
    }
    int64_t mid;
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


Route cmr_dijkstra(Contact* root_contact, nodeId_t destination, const std::vector<Contact> & contact_plan) {
    
    // Construct Contact Multigraph from Contact Plan
    ContactMultigraph CM(contact_plan, destination);
    // Set root vertex's arrival time
    CM.m_nodeMap[root_contact->frm].m_arrivalTime = root_contact->start;
    // Construct min PQ ordered by arrival time
    std::priority_queue<vertex_ptr_plus_arrival_time_pair_t, std::vector<vertex_ptr_plus_arrival_time_pair_t>, CompareArrivals> PQ;
    for (ContactMultigraph::cmr_node_map_t::iterator it = CM.m_nodeMap.begin(); it != CM.m_nodeMap.end(); ++it) {
        Vertex& v = it->second.m_vertex;
        PQ.emplace(&v, v.vertex_arrival_time);
    }
    if (PQ.empty()) {
        LOG_ERROR(subprocess) << "cmr_dijkstra: initial priority queue empty";
        return Route();
    }
    vertex_ptr_plus_arrival_time_pair_t v_next;
    vertex_ptr_plus_arrival_time_pair_t v_curr = PQ.top();
    PQ.pop();
    ContactMultigraph::cmr_node_map_t::iterator vCurrItNodeMap = CM.m_nodeMap.find(v_curr.first->id); //not const, modifies m_visited
    if (vCurrItNodeMap == CM.m_nodeMap.cend()) {
        LOG_WARNING(subprocess) << "vCurrItNodeMap not found!!";
        return Route();
    }
    
    while (true) {
        // ---------- MRP ----------
        for (const std::pair<const nodeId_t, std::vector<nodeId_t> > & adj : v_curr.first->adjacencies) {
            //std::unordered_map<nodeId_t, Vertex>::iterator itVert = CM.vertices.find(adj.first);
            ContactMultigraph::cmr_node_map_t::iterator itAdjNodeMap = CM.m_nodeMap.find(adj.first);
            if (itAdjNodeMap == CM.m_nodeMap.cend()) {
                LOG_WARNING(subprocess) << "adj not found!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!";
                return Route();
            }
            Vertex * u = &(itAdjNodeMap->second.m_vertex); //CM.vertices[adj.first];
            //sanity check
            if (u->id != itAdjNodeMap->first) {
                LOG_ERROR(subprocess) << "u->id not itAdjNodeMap->first";
                return Route();
            }
            if (itAdjNodeMap->second.m_visited) {
                continue;
            }
            // check if there is any viable contact
            const std::vector<uint64_t> & v_curr_to_u_ind = v_curr.first->adjacencies[u->id];
            if (v_curr_to_u_ind.empty()) {
                LOG_ERROR(subprocess) << "found error";
                continue;
            }
            std::vector<const Contact*> v_curr_to_u(v_curr_to_u_ind.size());
            for (std::size_t i = 0; i < v_curr_to_u_ind.size(); ++i) {
                v_curr_to_u[i] = &(contact_plan[v_curr_to_u_ind[i]]);
            }
            //std::unordered_map<nodeId_t, time_t>
            const time_t arrivalTimeVcurr = vCurrItNodeMap->second.m_arrivalTime;
            //if ((v_curr_to_u.back().end < CM.arrival_time[v_curr.id]) && (CM.arrival_time[v_curr.id] != MAX_SIZE)) {
            if ((v_curr_to_u.back()->end < arrivalTimeVcurr) && (arrivalTimeVcurr != MAX_TIME_T)) {
                continue;
            }
            // find earliest usable contact from v_curr to u
            const Contact& best_contact = contact_search(v_curr_to_u, arrivalTimeVcurr); // contact_search(v_curr_to_u, CM.arrival_time[v_curr.id]);
            // should owlt_mgn be included in best arrival time?
            //time_t best_arr_time = std::max(best_contact.start, CM.arrival_time[v_curr.id]) + best_contact.owlt;
            // bug causing max int to flip negative : const time_t best_arr_time = std::max(best_contact.start, arrivalTimeVcurr) + best_contact.owlt;
            time_t best_arr_time = std::max(best_contact.start, arrivalTimeVcurr);
            if (best_arr_time <= (MAX_TIME_T - best_contact.owlt)) {
                best_arr_time += best_contact.owlt;
            }

            //std::unordered_map<nodeId_t, time_t>::iterator itArrivalU = CM.arrival_time.find(u->id);
            //if (itArrivalU == CM.arrival_time.end()) {
            //    return Route();
            //}
            //const time_t arrivalTimeU = itArrivalU->second;
            time_t & arrivalTimeU = itAdjNodeMap->second.m_arrivalTime;

            if (best_arr_time < arrivalTimeU) { //if (best_arr_time < CM.arrival_time[u.id]) {
                arrivalTimeU = best_arr_time; //CM.arrival_time[u.id] = best_arr_time;
                // update PQ
                // using "lazy deletion"
                // Source: https://stackoverflow.com/questions/9209323/easiest-way-of-using-min-priority-queue-with-key-update-in-c
                //u.predecessor = contact_search_predecessor(v_curr_to_u, v_curr.arrival_time); old way
                //uint64_t p_i = contact_search_predecessor(v_curr_to_u_ind, CM.arrival_time[v_curr.id], contact_plan);
                uint64_t p_i = contact_search_predecessor(v_curr_to_u_ind, arrivalTimeVcurr, contact_plan);
                //std::unordered_map<nodeId_t, nodeId_t>::iterator itPredU = CM.predecessors.find(u->id);
                //if (itPredU == CM.predecessors.end()) { //not found
                //    return Route();
                //}
                //itPredU->second = p_i; //CM.predecessors[u.id] = p_i;
                itAdjNodeMap->second.m_predecessorNodeId = p_i;
                // still want to update u node's arrival time for sake of pq
                u->vertex_arrival_time = best_arr_time; //only modification of a vertex in this loop
                PQ.emplace(u, best_arr_time); //prevent changing the key within the priority queue across identical pointers (lazy deletion) 
            }
        }
        vCurrItNodeMap->second.m_visited = true;
        // ---------- MRP ----------
        if (PQ.empty()) {
            LOG_ERROR(subprocess) << "cmr_dijkstra: priority queue empty";
            return Route();
        }
        v_next = PQ.top();
        PQ.pop();
        if (v_next.first->id == destination) {
            break; //only exit from while (true)
        }
        else {
            v_curr = v_next;
            vCurrItNodeMap = CM.m_nodeMap.find(v_curr.first->id);
            if (vCurrItNodeMap == CM.m_nodeMap.cend()) {
                LOG_WARNING(subprocess) << "vCurrItNodeMap from v_next not found!";
                return Route();
            }
        }
    }
    // construct route from contact predecessors
    std::vector<const Contact*> hops;
#if 0
    Contact contact;
    for (contact = contact_plan[CM.predecessors[v_next.id]]; contact.frm != contact.to; contact = contact_plan[CM.predecessors[CM.vertices[contact.frm].id]]) {
        hops.push_back(&contact);
        if (contact.frm == root_contact->frm) { 
            break;
        }
    }
#else
    nodeId_t predecessorIndex = v_next.first->id;
    while (true) {
        ContactMultigraph::cmr_node_map_t::const_iterator vNextItNodeMap = CM.m_nodeMap.find(predecessorIndex);
        if (vNextItNodeMap == CM.m_nodeMap.cend()) {
            LOG_WARNING(subprocess) << "vNextItNodeMap not found!!";
            break;
        }
        const nodeId_t contactPlanIndex = vNextItNodeMap->second.m_predecessorNodeId;
        if (contactPlanIndex >= contact_plan.size()) {
            LOG_ERROR(subprocess) << "contactPlanIndex(" << contactPlanIndex << ") >= contact_plan.size(" << contact_plan.size() << ")";
            break;
        }
        const Contact & contact = contact_plan[contactPlanIndex];
        if (contact.frm == contact.to) {
            break;
        }
        hops.push_back(&contact);
        if (contact.frm == root_contact->frm) {
            break;
        }
        ContactMultigraph::cmr_node_map_t::const_iterator contactFromItNodeMap = CM.m_nodeMap.find(contact.frm);
        if (contactFromItNodeMap == CM.m_nodeMap.cend()) {
            LOG_WARNING(subprocess) << "contactFromItNodeMap not found!!";
            break;
        }
        predecessorIndex = contactFromItNodeMap->second.m_vertex.id;
    }
#endif
    Route route;
    if (hops.empty()) {
        LOG_DEBUG(subprocess) << "todo: hops is empty.. no route";
        route.to_node = std::numeric_limits<nodeId_t>::max();
    }
    else {
        route = Route(*(hops.back()));
        hops.pop_back();
        while (!hops.empty()) {
            route.append(*(hops.back()));
            hops.pop_back();
        }
    }
    return route;
}

} // namespace cgr

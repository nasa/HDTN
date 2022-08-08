#include "libcgr.h"

#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>

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
    mav = std::vector<int>({volume, volume, volume});

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

Route::Route(Contact contact, Route *parent)
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
    } else {
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
    } else {
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
        } else {
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
    } catch (EmptyContainerError) {
        return true;
    }
}

std::vector<Contact> Route::get_hops() {
    if (NULL == parent) {
        return hops;
    } else {
        std::vector<Contact> v(parent->get_hops());
        v.insert(v.end(), hops.begin(), hops.end());
        return v;
    }
}

/*
 * Library function implementations, e.g. loading, routing algorithms, etc.
 */
std::vector<Contact> cp_load(std::string filename, int max_contacts) {
    std::vector<Contact> contactsVector;
    boost::property_tree::ptree pt;
    boost::property_tree::read_json(filename, pt);
    const boost::property_tree::ptree & contactsPt
        = pt.get_child("contacts", boost::property_tree::ptree());
    for (const boost::property_tree::ptree::value_type &eventPt : contactsPt) {
        Contact new_contact = Contact(
            eventPt.second.get<int>("source", 0),
            eventPt.second.get<int>("dest", 0),
            eventPt.second.get<int>("startTime", 0),
            eventPt.second.get<int>("endTime", 0),
            eventPt.second.get<int>("rate", 0),
            1.0, // confidence
            eventPt.second.get<int>("owlt", 0));
        // new_contact.id = eventPt.second.get<int>("contact", 0);
        contactsVector.push_back(new_contact);
        if (contactsVector.size() == max_contacts) {
            break;
        }
    }
    return contactsVector;
}

Route dijkstra(Contact *root_contact, nodeId_t destination, std::vector<Contact> contact_plan) {
    // Need to clear the real contacts in the contact plan
    // so we loop using Contact& instead of Contact
    for (Contact &contact : contact_plan) {
        if (contact != *root_contact) {
            contact.clear_dijkstra_working_area();
        }
    }

    // Make sure we map to pointers so we can modify the underlying contact_plan
    // using the hashmap. The hashmap helps us find the neighbors of a node.
    std::map<nodeId_t, std::vector<Contact*>> contact_plan_hash;
    for (Contact &contact : contact_plan ) {
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
    int earliest_fin_arr_t = MAX_SIZE;
    int arrvl_time;

    Contact *current = root_contact;

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
            } else {
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
        Contact *next_contact = NULL;
        // @source DtnSim
        // "Warning: we need to point finalContact to
        // the real contact in contactPlan..."
        // This is why we loop with a Contact& rather than a Contact
        for (Contact &contact : contact_plan) {
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

std::ostream& operator<<(std::ostream &out, const std::vector<Contact> &obj) {
    out << "[";
    for (int i = 0; i < obj.size()-1; i++) {
        out << obj[i] << ", ";
    }
    out << obj[obj.size()-1] << "]";

    return out;
}

std::ostream& operator<<(std::ostream &out, const Contact &obj) {
    static const boost::format fmtTemplate("%d->%d(%d-%d,d%d)[mav%.0f%%]");
    boost::format fmt(fmtTemplate);

    int min_vol = *std::min_element(obj.mav.begin(), obj.mav.end());
    double volume = 100.0 * min_vol / obj.volume;
    fmt % obj.frm % obj.to % obj.start % obj.end % obj.owlt % volume;
    const std::string message(std::move(fmt.str()));

    out << message;
    return out;
}

std::ostream& operator<<(std::ostream &out, const Route &obj) {
    static const boost::format fmtTemplate("to:%d|via:%d(%03d,%03d)|bdt:%d|hops:%d|vol:%d|conf:%.1f|%s");
    boost::format fmt(fmtTemplate);

    std::vector<Contact> routeHops = static_cast<Route>(obj).get_hops();

    fmt % obj.to_node % obj.next_node % obj.from_time % obj.to_time % obj.best_delivery_time
        % routeHops.size() % obj.volume % obj.confidence % routeHops;
    const std::string message(std::move(fmt.str()));

    out << message;
    return out;
}

}

#ifndef LIB_CGR_H
#define LIB_CGR_H

#include <vector>
#include <map>
#include <unordered_map>
#include <ostream>
#include <limits>
#include <memory>
#include "cgr_lib_export.h"

namespace cgr {

const uint64_t MAX_SIZE = std::numeric_limits<uint64_t>::max();

typedef uint64_t nodeId_t;
typedef uint64_t time_t;

class Contact {
public:
    // Fixed parameters
    nodeId_t frm, to;
    time_t start, end;
    uint64_t volume;
    uint64_t rate;
    time_t owlt;
    int id;
    float confidence;
    // Variable parameters
    std::vector<uint64_t> mav;
    // Route search working area
    time_t arrival_time;
    bool visited;
    Contact* predecessor;
    std::vector<nodeId_t> visited_nodes;
    // Route management working area
    bool suppressed;
    std::vector<Contact> suppressed_next_hop;
    // Forwarding working area
    int first_byte_tx_time, last_byte_tx_time, last_byte_arr_time, effective_volume_limit;
    CGR_LIB_EXPORT void clear_dijkstra_working_area();
    CGR_LIB_EXPORT Contact(nodeId_t frm, nodeId_t to, time_t start, time_t end, uint64_t rate, float confidence=1, time_t owlt=1);
    CGR_LIB_EXPORT Contact();
    CGR_LIB_EXPORT ~Contact();
    CGR_LIB_EXPORT bool operator==(const Contact) const;
    CGR_LIB_EXPORT bool operator!=(const Contact) const;
    CGR_LIB_EXPORT friend std::ostream& operator<<(std::ostream&, const Contact&);
};


class Route {
public:
    nodeId_t to_node, next_node;
    time_t from_time, to_time, best_delivery_time;
    uint64_t volume;
    float confidence;
    CGR_LIB_EXPORT Route(const Contact&, Route *parent=NULL);
    CGR_LIB_EXPORT Route();
    CGR_LIB_EXPORT ~Route();
    Route(Contact, Route* parent = NULL);
    Route();
    ~Route();
private:
    Route* parent;
    std::vector<Contact> hops;
    std::map<nodeId_t, bool> __visited;
public:
    CGR_LIB_EXPORT Contact get_last_contact();
    CGR_LIB_EXPORT bool visited(nodeId_t node);
    CGR_LIB_EXPORT void append(const Contact &contact);
    CGR_LIB_EXPORT void refresh_metrics();
    CGR_LIB_EXPORT bool eligible(const Contact &contact);
    CGR_LIB_EXPORT std::vector<Contact> get_hops();
};

CGR_LIB_EXPORT std::vector<Contact> cp_load(std::string filename, uint64_t max_contacts=MAX_SIZE);

CGR_LIB_EXPORT std::shared_ptr<Route> dijkstra(Contact *root_contact, nodeId_t destination, std::vector<Contact> contact_plan);

CGR_LIB_EXPORT std::vector<Route> yen(nodeId_t source, nodeId_t destination, time_t currTime, std::vector<Contact> contactPlan, uint64_t numRoutes);
    
    Contact get_last_contact();
    bool visited(nodeId_t node);
    void append(Contact contact);
    void refresh_metrics();
    bool eligible(Contact contact);
    std::vector<Contact> get_hops();
};


// Vertex for Multigraph Routing
class Vertex {
public:
    nodeId_t id;
    std::unordered_map<nodeId_t, std::vector<int> > adjacencies; // now store the index of the contact in the contact plan
    int arrival_time;
    bool visited;
    Contact* predecessor;
    Vertex(nodeId_t id);
    Vertex();
    bool operator<(const Vertex& v) const;
};


class ContactMultigraph {
public:
    std::unordered_map<nodeId_t, Vertex> vertices;
    std::unordered_map<nodeId_t, bool> visited;
    std::unordered_map<nodeId_t, int> predecessors; // Stores the indices of the contacts
    std::unordered_map<nodeId_t, int> arrival_time;
    ContactMultigraph(std::vector<Contact> contact_plan, nodeId_t dest_id);
};


class CompareArrivals
{
public:
    bool operator()(const Vertex& v1, const Vertex& v2)
    {
        // smaller id breaks tie
        if (v1.arrival_time == v2.arrival_time) {
            return v1.id > v2.id;
        }
        return v1.arrival_time > v2.arrival_time;
    }
};
int contact_search_index(std::vector<Contact>& contacts, int arrival_time);
Contact* contact_search_predecessor(std::vector<Contact>& contacts, int arrival_time);
std::vector<Contact> cp_load(std::string filename, int max_contacts = MAX_SIZE);
Route dijkstra(Contact* root_contact, nodeId_t destination, std::vector<Contact> contact_plan);
Route cmr_dijkstra(Contact* root_contact, nodeId_t destination, std::vector<Contact> contact_plan);
std::vector<Route> yen(nodeId_t source, nodeId_t destination, int currTime, std::vector<Contact> contactPlan, int numRoutes);

template <typename T>   bool vector_contains(std::vector<T> vec, T ele);

class EmptyContainerError : public std::exception {
    virtual const char* what() const throw();
};
std::ostream& operator<<(std::ostream& out, const std::vector<Contact>& obj);  std::ostream& operator<<(std::ostream& out, const Contact& obj); std::ostream& operator<<(std::ostream& out, const Route& obj);


} // namespace cgr

#endif // LIB_CGR_H

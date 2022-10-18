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

static constexpr time_t MAX_SIZE = std::numeric_limits<time_t>::max();

typedef uint64_t nodeId_t;
//typedef uint64_t time_t;

class Contact {
public:
    // Fixed parameters
    nodeId_t frm, to;
    time_t start, end;
    uint64_t volume;
    uint64_t rate;
    time_t owlt;
    uint64_t id;
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
    time_t first_byte_tx_time, last_byte_tx_time, last_byte_arr_time;
    uint64_t effective_volume_limit;
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
    CGR_LIB_EXPORT Route(Contact, Route *parent=NULL);
    CGR_LIB_EXPORT Route();
    CGR_LIB_EXPORT ~Route();
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
    CGR_LIB_EXPORT friend std::ostream& operator<<(std::ostream& out, const Route& obj);
};


// Vertex for Multigraph Routing
class Vertex {
public:
    nodeId_t id;
    std::unordered_map<nodeId_t, std::vector<nodeId_t> > adjacencies; // now store the index of the contact in the contact plan
    time_t arrival_time;
    bool visited;
    Contact* predecessor;
    CGR_LIB_EXPORT Vertex(nodeId_t id);
    CGR_LIB_EXPORT Vertex();
    CGR_LIB_EXPORT bool operator<(const Vertex& v) const;
};

class ContactMultigraph {
public:
    std::unordered_map<nodeId_t, Vertex> vertices;
    std::unordered_map<nodeId_t, bool> visited;
    std::unordered_map<nodeId_t, nodeId_t> predecessors; // Stores the indices of the contacts
    std::unordered_map<nodeId_t, time_t> arrival_time;
    CGR_LIB_EXPORT ContactMultigraph(std::vector<Contact> contact_plan, nodeId_t dest_id);
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


CGR_LIB_EXPORT int64_t contact_search_index(std::vector<Contact>& contacts, time_t arrival_time);

//CGR_LIB_EXPORT Contact* contact_search_predecessor(std::vector<Contact>& contacts, time_t arrival_time);

CGR_LIB_EXPORT std::vector<Contact> cp_load(std::string filename, std::size_t max_contacts= std::numeric_limits<std::size_t>::max());

CGR_LIB_EXPORT Route dijkstra(Contact *root_contact, nodeId_t destination, std::vector<Contact> contact_plan);

CGR_LIB_EXPORT Route cmr_dijkstra(Contact* root_contact, nodeId_t destination, std::vector<Contact> contact_plan);

CGR_LIB_EXPORT std::vector<Route> yen(nodeId_t source, nodeId_t destination, int currTime, std::vector<Contact> contactPlan, int numRoutes);
    
template <typename T>   bool vector_contains(std::vector<T> vec, T ele);

class EmptyContainerError : public std::exception {
    CGR_LIB_EXPORT virtual const char* what() const throw();
};
CGR_LIB_EXPORT std::ostream& operator<<(std::ostream& out, const std::vector<Contact>& obj);


} // namespace cgr

#endif // LIB_CGR_H

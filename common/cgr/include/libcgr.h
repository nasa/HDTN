#ifndef LIB_CGR_H
#define LIB_CGR_H

#include <vector>
#include <map>
#include <unordered_map>
#include <ostream>
#include <limits>
#include "cgr_lib_export.h"

namespace cgr {

const int MAX_SIZE = std::numeric_limits<int>::max();

typedef uint64_t nodeId_t;

class Contact {
public:
    // Fixed parameters
    nodeId_t frm, to;
    int start, end, rate, volume;
    int owlt;
    // int id = -1;
    float confidence;
    // Variable parameters
    std::vector<int> mav;
    // Route search working area
    int arrival_time;
    bool visited;
    Contact *predecessor;
    std::vector<nodeId_t> visited_nodes;
    // Route management working area
    bool suppressed;
    std::vector<Contact> suppressed_next_hop;
    // Forwarding working area
    int first_byte_tx_time, last_byte_tx_time, last_byte_arr_time, effective_volume_limit;
    CGR_LIB_EXPORT void clear_dijkstra_working_area();
    CGR_LIB_EXPORT Contact(nodeId_t frm, nodeId_t to, int start, int end, int rate, float confidence=1, int owlt=1);
    CGR_LIB_EXPORT Contact();
    CGR_LIB_EXPORT ~Contact();
    CGR_LIB_EXPORT bool operator==(const Contact) const;
    CGR_LIB_EXPORT bool operator!=(const Contact) const;
    CGR_LIB_EXPORT friend std::ostream& operator<<(std::ostream&, const Contact&);
};


class Route {
public:
    nodeId_t to_node, next_node;
    int from_time, to_time, best_delivery_time, volume;
    float confidence;
    CGR_LIB_EXPORT Route(Contact, Route *parent=NULL);
    CGR_LIB_EXPORT Route();
    CGR_LIB_EXPORT ~Route();
private:
    Route *parent;
    std::vector<Contact> hops;
    std::map<nodeId_t, bool> __visited;
public:
    CGR_LIB_EXPORT Contact get_last_contact();
    CGR_LIB_EXPORT bool visited(nodeId_t node);
    CGR_LIB_EXPORT void append(Contact contact);
    CGR_LIB_EXPORT void refresh_metrics();
    CGR_LIB_EXPORT bool eligible(Contact contact);
    CGR_LIB_EXPORT std::vector<Contact> get_hops();
};


// Vertex for Multigraph Routing
class Vertex {
public:
    nodeId_t id;
    std::unordered_map<nodeId_t, std::vector<Contact>> adjacencies;
    int arrival_time;
    bool visited;
    Contact *predecessor;
    CGR_LIB_EXPORT Vertex(nodeId_t id);
};


class ContactMultigraph {
public:
    std::unordered_map<nodeId_t, Vertex> vertices;
    CGR_LIB_EXPORT ContactMultigraph(vector<Contact> contact_plan, nodeId_t dest_id);
};



class CompareArrivals
{
public:
    bool operator()(Vertex v1, Vertex v2)
    {
        return v1.arrival_time < v2.arrival_time;
    }
};



CGR_LIB_EXPORT std::vector<Contact> cp_load(std::string filename, int max_contacts=MAX_SIZE);

CGR_LIB_EXPORT Route dijkstra(Contact *root_contact, nodeId_t destination, std::vector<Contact> contact_plan);

CGR_LIB_EXPORT std::vector<Route> yen(nodeId_t source, nodeId_t destination, int currTime, std::vector<Contact> contactPlan, int numRoutes);

template <typename T>
CGR_LIB_EXPORT bool vector_contains(std::vector<T> vec, T ele);

class EmptyContainerError: public std::exception {
    CGR_LIB_EXPORT virtual const char* what() const throw();
};

CGR_LIB_EXPORT std::ostream& operator<<(std::ostream &out, const std::vector<Contact> &obj);
CGR_LIB_EXPORT std::ostream& operator<<(std::ostream &out, const Contact &obj);
CGR_LIB_EXPORT std::ostream& operator<<(std::ostream &out, const Route &obj);


} // namespace cgr

#endif // LIB_CGR_H

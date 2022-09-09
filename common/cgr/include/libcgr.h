#ifndef LIB_CGR_H
#define LIB_CGR_H

#include <vector>
#include <map>
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
    Contact *predecessor;
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
    CGR_LIB_EXPORT Route(const Contact&, Route *parent=NULL);
    CGR_LIB_EXPORT Route();
    CGR_LIB_EXPORT ~Route();
private:
    Route *parent;
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

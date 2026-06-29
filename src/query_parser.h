#ifndef AETHER_GRAPH_QUERY_PARSER_H
#define AETHER_GRAPH_QUERY_PARSER_H

#include "utils.h"
#include "graph_engine.h"
#include <string>
#include <vector>

namespace AetherGraph {

enum class QueryType {
    CREATE_NODE,
    CREATE_EDGE,
    DELETE_NODE,
    MATCH_NODE,
    MATCH_PATH
};

struct ParsedQuery {
    QueryType type;
    std::string node_label;
    node_id_t node_id = INVALID_NODE_ID;
    node_id_t src_id = INVALID_NODE_ID;
    node_id_t dest_id = INVALID_NODE_ID;
    std::string edge_type;
    std::string property_key;
    Variant property_value;
    bool has_property = false;

    // For path queries
    std::string path_start_label;
    std::string path_end_label;
};

class QueryParser {
public:
    static ParsedQuery parse(const std::string& query_str);
};

} // namespace AetherGraph

#endif // AETHER_GRAPH_QUERY_PARSER_H

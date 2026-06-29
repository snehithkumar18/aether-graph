#include "query_parser.h"
#include <sstream>
#include <algorithm>

namespace AetherGraph {

ParsedQuery QueryParser::parse(const std::string& query_str) {
    ParsedQuery query;
    std::stringstream ss(query_str);
    std::string command;
    ss >> command;

    if (command == "CREATE") {
        std::string target;
        ss >> target;
        if (target == "NODE") {
            query.type = QueryType::CREATE_NODE;
            ss >> query.node_label;
            std::string key;
            if (ss >> key) {
                query.property_key = key;
                std::string val_str;
                if (ss >> val_str) {
                    query.has_property = true;
                    // Parse value
                    if (val_str == "true" || val_str == "false") {
                        query.property_value = Variant(val_str == "true");
                    } else {
                        try {
                            size_t idx;
                            int val = std::stoi(val_str, &idx);
                            if (idx == val_str.size()) {
                                query.property_value = Variant(val);
                            } else {
                                query.property_value = Variant(val_str);
                            }
                        } catch (...) {
                            query.property_value = Variant(val_str);
                        }
                    }
                }
            }
        } else if (target == "EDGE") {
            query.type = QueryType::CREATE_EDGE;
            ss >> query.src_id >> query.dest_id >> query.edge_type;
        }
    } else if (command == "DELETE") {
        query.type = QueryType::DELETE_NODE;
        ss >> query.node_id;
    } else if (command == "MATCH") {
        std::string target;
        ss >> target;
        if (target == "NODE") {
            query.type = QueryType::MATCH_NODE;
            ss >> query.node_label;
            std::string key;
            if (ss >> key) {
                query.property_key = key;
                std::string val_str;
                if (ss >> val_str) {
                    query.has_property = true;
                    if (val_str == "true" || val_str == "false") {
                        query.property_value = Variant(val_str == "true");
                    } else {
                        try {
                            size_t idx;
                            int val = std::stoi(val_str, &idx);
                            if (idx == val_str.size()) {
                                query.property_value = Variant(val);
                            } else {
                                query.property_value = Variant(val_str);
                            }
                        } catch (...) {
                            query.property_value = Variant(val_str);
                        }
                    }
                }
            }
        } else if (target == "PATH") {
            query.type = QueryType::MATCH_PATH;
            ss >> query.src_id >> query.dest_id;
        }
    }
    return query;
}

} // namespace AetherGraph

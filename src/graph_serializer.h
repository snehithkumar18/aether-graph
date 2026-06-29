#ifndef AETHER_GRAPH_GRAPH_SERIALIZER_H
#define AETHER_GRAPH_GRAPH_SERIALIZER_H

#include "graph_engine.h"
#include <string>
#include <vector>

namespace AetherGraph {

class GraphSerializer {
public:
    static std::string export_to_json(GraphEngine& ge);
    static bool import_from_json(GraphEngine& ge, const std::string& json_str);
    static std::string export_to_graphml(GraphEngine& ge);
    static bool export_to_csv(GraphEngine& ge, std::string& nodes_csv, std::string& edges_csv);
    static std::string export_to_dot(GraphEngine& ge);
    static std::vector<uint8_t> export_to_binary(GraphEngine& ge);
    static bool import_from_binary(GraphEngine& ge, const std::vector<uint8_t>& binary_data);
};

} // namespace AetherGraph

#endif // AETHER_GRAPH_GRAPH_SERIALIZER_H

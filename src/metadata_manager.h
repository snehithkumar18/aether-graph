#ifndef AETHER_GRAPH_METADATA_MANAGER_H
#define AETHER_GRAPH_METADATA_MANAGER_H

#include "graph_engine.h"
#include <string>
#include <vector>
#include <unordered_map>
#include <set>

namespace AetherGraph {

enum class ConstraintType {
    UNIQUE,
    NOT_NULL,
    FOREIGN_KEY
};

struct SchemaConstraint {
    ConstraintType type;
    std::string property_key;
    std::string ref_label; // for foreign keys
};

struct LabelSchema {
    std::string label;
    std::unordered_map<std::string, DataType> property_types;
    std::vector<SchemaConstraint> constraints;
};

class MetadataManager {
private:
    std::unordered_map<std::string, LabelSchema> schemas_;
    std::set<std::string> indexes_;

public:
    MetadataManager() = default;
    ~MetadataManager() = default;

    bool register_label(const std::string& label);
    bool add_property_schema(const std::string& label, const std::string& property_key, DataType type);
    bool add_constraint(const std::string& label, ConstraintType type, const std::string& property_key, const std::string& ref_label = "");

    bool validate_node(const Node& node, GraphEngine& ge) const;
    bool register_index(const std::string& property_key);
    bool has_index(const std::string& property_key) const;

    std::string serialize_catalog() const;
    bool deserialize_catalog(const std::string& meta_str);
};

} // namespace AetherGraph

#endif // AETHER_GRAPH_METADATA_MANAGER_H

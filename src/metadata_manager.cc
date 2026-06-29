#include "metadata_manager.h"
#include <sstream>
#include <iostream>

namespace AetherGraph {

bool MetadataManager::register_label(const std::string& label) {
    if (schemas_.find(label) != schemas_.end()) return false;
    LabelSchema schema;
    schema.label = label;
    schemas_[label] = schema;
    return true;
}

bool MetadataManager::add_property_schema(const std::string& label, const std::string& property_key, DataType type) {
    auto it = schemas_.find(label);
    if (it == schemas_.end()) return false;
    it->second.property_types[property_key] = type;
    return true;
}

bool MetadataManager::add_constraint(
    const std::string& label, ConstraintType type, const std::string& property_key, const std::string& ref_label) {

    auto it = schemas_.find(label);
    if (it == schemas_.end()) return false;
    
    SchemaConstraint cons;
    cons.type = type;
    cons.property_key = property_key;
    cons.ref_label = ref_label;
    
    it->second.constraints.push_back(cons);
    return true;
}

bool MetadataManager::validate_node(const Node& node, GraphEngine& ge) const {
    auto it = schemas_.find(node.label);
    if (it == schemas_.end()) return true; // Schema-free if label not registered

    const auto& schema = it->second;

    // Validate property types matching schemas
    for (const auto& [k, v] : node.properties) {
        auto p_it = schema.property_types.find(k);
        if (p_it != schema.property_types.end()) {
            if (p_it->second != v.type) return false; // Type mismatch
        }
    }

    // Validate constraints
    for (const auto& cons : schema.constraints) {
        if (cons.type == ConstraintType::NOT_NULL) {
            if (node.properties.find(cons.property_key) == node.properties.end()) {
                return false; // Not null violated
            }
        } else if (cons.type == ConstraintType::UNIQUE) {
            auto val_it = node.properties.find(cons.property_key);
            if (val_it != node.properties.end()) {
                const auto& nodes = ge.get_all_nodes();
                for (const auto& [other_id, other_node] : nodes) {
                    if (other_id != node.id && other_node->label == node.label) {
                        auto other_val_it = other_node->properties.find(cons.property_key);
                        if (other_val_it != other_node->properties.end() && other_val_it->second == val_it->second) {
                            return false; // Unique constraint violated
                        }
                    }
                }
            }
        }
    }

    return true;
}

bool MetadataManager::register_index(const std::string& property_key) {
    if (indexes_.count(property_key) > 0) return false;
    indexes_.insert(property_key);
    return true;
}

bool MetadataManager::has_index(const std::string& property_key) const {
    return indexes_.count(property_key) > 0;
}

std::string MetadataManager::serialize_catalog() const {
    std::ostringstream oss;
    oss << "[SCHEMAS]\n";
    for (const auto& [label, schema] : schemas_) {
        oss << "L:" << label << "\n";
        for (const auto& [k, t] : schema.property_types) {
            oss << "P:" << k << ":" << static_cast<int>(t) << "\n";
        }
        for (const auto& cons : schema.constraints) {
            oss << "C:" << static_cast<int>(cons.type) << ":" << cons.property_key << ":" << cons.ref_label << "\n";
        }
    }
    oss << "[INDEXES]\n";
    for (const auto& idx : indexes_) {
        oss << "I:" << idx << "\n";
    }
    return oss.str();
}

bool MetadataManager::deserialize_catalog(const std::string& meta_str) {
    schemas_.clear();
    indexes_.clear();

    std::istringstream iss(meta_str);
    std::string line;
    std::string current_label;

    while (std::getline(iss, line)) {
        if (line.empty()) continue;
        if (line == "[SCHEMAS]" || line == "[INDEXES]") continue;

        if (line[0] == 'L' && line.size() > 2) {
            current_label = line.substr(2);
            register_label(current_label);
        } else if (line[0] == 'P' && !current_label.empty()) {
            std::istringstream p_iss(line.substr(2));
            std::string key, type_str;
            if (std::getline(p_iss, key, ':') && std::getline(p_iss, type_str)) {
                DataType t = static_cast<DataType>(std::stoi(type_str));
                add_property_schema(current_label, key, t);
            }
        } else if (line[0] == 'C' && !current_label.empty()) {
            std::istringstream c_iss(line.substr(2));
            std::string type_str, key, ref;
            if (std::getline(c_iss, type_str, ':') && std::getline(c_iss, key, ':')) {
                std::getline(c_iss, ref);
                ConstraintType ct = static_cast<ConstraintType>(std::stoi(type_str));
                add_constraint(current_label, ct, key, ref);
            }
        } else if (line[0] == 'I' && line.size() > 2) {
            register_index(line.substr(2));
        }
    }
    return true;
}

} // namespace AetherGraph

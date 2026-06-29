#include "query_engine.h"
#include <algorithm>

namespace AetherGraph {

// ======================================================================
// PhysicalHashJoin Implementation
// ======================================================================
PhysicalHashJoin::PhysicalHashJoin(
    std::unique_ptr<PhysicalOperator> left, std::unique_ptr<PhysicalOperator> right,
    const std::string& left_key, const std::string& right_key)
    : left_(std::move(left)), right_(std::move(right)), left_key_(left_key), right_key_(right_key) {}

void PhysicalHashJoin::open() {
    hash_table_.clear();
    matches_.clear();
    cursor_ = 0;

    left_->open();
    right_->open();

    // 1. Build Phase (hash the left child results)
    Node* left_node = nullptr;
    while ((left_node = left_->next()) != nullptr) {
        auto it = left_node->properties.find(left_key_);
        if (it != left_node->properties.end()) {
            std::string key_str;
            if (it->second.type == DataType::INT) key_str = std::to_string(it->second.get_int());
            else if (it->second.type == DataType::FLOAT) key_str = std::to_string(it->second.get_float());
            else if (it->second.type == DataType::STRING) key_str = it->second.get_string();

            if (!key_str.empty()) {
                hash_table_.insert({key_str, left_node});
            }
        }
    }

    // 2. Probe Phase (probe hash table with right child results)
    Node* right_node = nullptr;
    while ((right_node = right_->next()) != nullptr) {
        auto it = right_node->properties.find(right_key_);
        if (it != right_node->properties.end()) {
            std::string key_str;
            if (it->second.type == DataType::INT) key_str = std::to_string(it->second.get_int());
            else if (it->second.type == DataType::FLOAT) key_str = std::to_string(it->second.get_float());
            else if (it->second.type == DataType::STRING) key_str = it->second.get_string();

            if (!key_str.empty()) {
                auto range = hash_table_.equal_range(key_str);
                for (auto hash_it = range.first; hash_it != range.second; ++hash_it) {
                    matches_.push_back(right_node); // matching node
                }
            }
        }
    }
}

Node* PhysicalHashJoin::next() {
    if (cursor_ >= matches_.size()) return nullptr;
    return matches_[cursor_++];
}

void PhysicalHashJoin::close() {
    left_->close();
    right_->close();
    hash_table_.clear();
    matches_.clear();
}

// ======================================================================
// PhysicalSort Implementation
// ======================================================================
PhysicalSort::PhysicalSort(
    std::unique_ptr<PhysicalOperator> child, const std::string& sort_key, bool ascending)
    : child_(std::move(child)), sort_key_(sort_key), ascending_(ascending) {}

void PhysicalSort::open() {
    sorted_nodes_.clear();
    cursor_ = 0;

    child_->open();
    Node* node = nullptr;
    while ((node = child_->next()) != nullptr) {
        sorted_nodes_.push_back(node);
    }

    std::sort(sorted_nodes_.begin(), sorted_nodes_.end(), [this](Node* a, Node* b) {
        auto it_a = a->properties.find(sort_key_);
        auto it_b = b->properties.find(sort_key_);
        if (it_a == a->properties.end() || it_b == b->properties.end()) {
            return ascending_; // put unmatched at end/beginning
        }

        const auto& val_a = it_a->second;
        const auto& val_b = it_b->second;
        if (val_a.type != val_b.type) return ascending_;

        bool result = false;
        if (val_a.type == DataType::INT) {
            result = val_a.get_int() < val_b.get_int();
        } else if (val_a.type == DataType::FLOAT) {
            result = val_a.get_float() < val_b.get_float();
        } else if (val_a.type == DataType::STRING) {
            result = val_a.get_string() < val_b.get_string();
        }

        return ascending_ ? result : !result;
    });
}

Node* PhysicalSort::next() {
    if (cursor_ >= sorted_nodes_.size()) return nullptr;
    return sorted_nodes_[cursor_++];
}

void PhysicalSort::close() {
    child_->close();
    sorted_nodes_.clear();
}

// ======================================================================
// PhysicalLimit Implementation
// ======================================================================
PhysicalLimit::PhysicalLimit(std::unique_ptr<PhysicalOperator> child, size_t limit)
    : child_(std::move(child)), limit_(limit) {}

void PhysicalLimit::open() {
    child_->open();
    count_ = 0;
}

Node* PhysicalLimit::next() {
    if (count_ >= limit_) return nullptr;
    Node* node = child_->next();
    if (node) count_++;
    return node;
}

void PhysicalLimit::close() {
    child_->close();
}

// ======================================================================
// PhysicalAggregate Implementation
// ======================================================================
PhysicalAggregate::PhysicalAggregate(
    std::unique_ptr<PhysicalOperator> child, const std::string& group_key,
    const std::string& agg_key, const std::string& func)
    : child_(std::move(child)), group_key_(group_key), agg_key_(agg_key), func_(func) {}

PhysicalAggregate::~PhysicalAggregate() {
    close();
}

void PhysicalAggregate::open() {
    close();
    child_->open();
    compute_aggregates();
    cursor_ = 0;
}

void PhysicalAggregate::compute_aggregates() {
    struct AggState {
        double sum = 0.0;
        double min_val = std::numeric_limits<double>::infinity();
        double max_val = -std::numeric_limits<double>::infinity();
        size_t count = 0;
    };

    std::unordered_map<std::string, AggState> groups;
    Node* node = nullptr;

    while ((node = child_->next()) != nullptr) {
        std::string g_val = "";
        auto it = node->properties.find(group_key_);
        if (it != node->properties.end()) {
            if (it->second.type == DataType::INT) g_val = std::to_string(it->second.get_int());
            else if (it->second.type == DataType::FLOAT) g_val = std::to_string(it->second.get_float());
            else if (it->second.type == DataType::STRING) g_val = it->second.get_string();
        }

        double val = 0.0;
        auto a_it = node->properties.find(agg_key_);
        if (a_it != node->properties.end()) {
            if (a_it->second.type == DataType::INT) val = static_cast<double>(a_it->second.get_int());
            else if (a_it->second.type == DataType::FLOAT) val = static_cast<double>(a_it->second.get_float());
        }

        auto& state = groups[g_val];
        state.count++;
        state.sum += val;
        state.min_val = std::min(state.min_val, val);
        state.max_val = std::max(state.max_val, val);
    }

    uint32_t virtual_id = 999900;
    for (const auto& [g_val, state] : groups) {
        Node* agg_node = new Node(virtual_id++, "AggregateResult");
        agg_node->properties[group_key_] = Variant(g_val);

        double result = 0.0;
        if (func_ == "COUNT") result = static_cast<double>(state.count);
        else if (func_ == "SUM") result = state.sum;
        else if (func_ == "MIN") result = state.min_val;
        else if (func_ == "MAX") result = state.max_val;
        else if (func_ == "AVG") result = state.count > 0 ? (state.sum / state.count) : 0.0;

        agg_node->properties[func_ + "(" + agg_key_ + ")"] = Variant(static_cast<float>(result));
        result_nodes_.push_back(agg_node);
    }
}

Node* PhysicalAggregate::next() {
    if (cursor_ >= result_nodes_.size()) return nullptr;
    return result_nodes_[cursor_++];
}

void PhysicalAggregate::close() {
    child_->close();
    for (auto* n : result_nodes_) {
        delete n;
    }
    result_nodes_.clear();
}

// ======================================================================
// PhysicalUnique Implementation
// ======================================================================
PhysicalUnique::PhysicalUnique(std::unique_ptr<PhysicalOperator> child)
    : child_(std::move(child)) {}

void PhysicalUnique::open() {
    child_->open();
    seen_ids_.clear();
}

Node* PhysicalUnique::next() {
    Node* node = nullptr;
    while ((node = child_->next()) != nullptr) {
        if (seen_ids_.count(node->id) == 0) {
            seen_ids_.insert(node->id);
            return node;
        }
    }
    return nullptr;
}

void PhysicalUnique::close() {
    child_->close();
    seen_ids_.clear();
}

// ======================================================================
// QueryEngineCoordinator Implementation
// ======================================================================
std::vector<Node*> QueryEngineCoordinator::execute_plan(std::unique_ptr<PhysicalOperator> plan) {
    std::vector<Node*> results;
    if (!plan) return results;

    plan->open();
    Node* node = nullptr;
    while ((node = plan->next()) != nullptr) {
        results.push_back(node);
    }
    plan->close();

    return results;
}

} // namespace AetherGraph

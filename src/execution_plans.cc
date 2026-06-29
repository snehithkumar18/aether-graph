#include "execution_plans.h"

namespace AetherGraph {

// ======================================================================
// PhysicalSeqScan Implementation
// ======================================================================
PhysicalSeqScan::PhysicalSeqScan(GraphEngine& ge, const std::string& label)
    : ge_(ge), label_(label) {}

void PhysicalSeqScan::open() {
    nodes_.clear();
    cursor_ = 0;
    const auto& all_nodes = ge_.get_all_nodes();
    for (const auto& [nid, n] : all_nodes) {
        if (label_.empty() || n->label == label_) {
            nodes_.push_back(nid);
        }
    }
}

Node* PhysicalSeqScan::next() {
    if (cursor_ >= nodes_.size()) return nullptr;
    return ge_.get_node(nodes_[cursor_++]);
}

void PhysicalSeqScan::close() {
    nodes_.clear();
}

// ======================================================================
// PhysicalIndexScan Implementation
// ======================================================================
PhysicalIndexScan::PhysicalIndexScan(GraphEngine& ge, PropertyIndex& index, const std::string& key_val)
    : ge_(ge), index_(index), key_val_(key_val) {}

void PhysicalIndexScan::open() {
    matching_ids_ = index_.search(key_val_);
    cursor_ = 0;
}

Node* PhysicalIndexScan::next() {
    if (cursor_ >= matching_ids_.size()) return nullptr;
    return ge_.get_node(matching_ids_[cursor_++]);
}

void PhysicalIndexScan::close() {
    matching_ids_.clear();
}

// ======================================================================
// PhysicalFilter Implementation
// ======================================================================
PhysicalFilter::PhysicalFilter(
    std::unique_ptr<PhysicalOperator> child, const std::string& key, const std::string& op, const Variant& val)
    : child_(std::move(child)), key_(key), op_(op), val_(val) {}

void PhysicalFilter::open() {
    child_->open();
}

bool PhysicalFilter::evaluate(const Node* node) const {
    if (!node) return false;
    auto it = node->properties.find(key_);
    if (it == node->properties.end()) return false;

    const auto& node_val = it->second;
    if (node_val.type != val_.type) return false;

    if (op_ == "=") {
        return node_val == val_;
    } else if (op_ == "!=") {
        return node_val != val_;
    } else if (op_ == "<") {
        if (node_val.type == DataType::INT) return node_val.get_int() < val_.get_int();
        if (node_val.type == DataType::FLOAT) return node_val.get_float() < val_.get_float();
    } else if (op_ == ">") {
        if (node_val.type == DataType::INT) return node_val.get_int() > val_.get_int();
        if (node_val.type == DataType::FLOAT) return node_val.get_float() > val_.get_float();
    }

    return false;
}

Node* PhysicalFilter::next() {
    Node* n = nullptr;
    while ((n = child_->next()) != nullptr) {
        if (evaluate(n)) {
            return n;
        }
    }
    return nullptr;
}

void PhysicalFilter::close() {
    child_->close();
}

// ======================================================================
// PhysicalNestedLoopJoin Implementation
// ======================================================================
PhysicalNestedLoopJoin::PhysicalNestedLoopJoin(
    std::unique_ptr<PhysicalOperator> left, std::unique_ptr<PhysicalOperator> right)
    : left_(std::move(left)), right_(std::move(right)) {}

void PhysicalNestedLoopJoin::open() {
    left_->open();
    right_->open();
    left_node_ = left_->next();
    right_cursor_ = 0;
    right_matches_.clear();
}

Node* PhysicalNestedLoopJoin::next() {
    while (left_node_ != nullptr) {
        Node* right_node = right_->next();
        if (right_node != nullptr) {
            // Check if there is an edge connecting left_node_ and right_node
            for (size_t i = 0; i < left_node_->out_edges_count; ++i) {
                Edge* edge = left_node_->out_edges[i];
                if (edge && edge->dest_id == right_node->id) {
                    return right_node; // Join match
                }
            }
        } else {
            // Reset right scanner for the next outer row
            right_->close();
            right_->open();
            left_node_ = left_->next();
        }
    }
    return nullptr;
}

void PhysicalNestedLoopJoin::close() {
    left_->close();
    right_->close();
    left_node_ = nullptr;
    right_matches_.clear();
}

} // namespace AetherGraph

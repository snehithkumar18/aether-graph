#include "vectorized_executor.h"
#include <algorithm>

namespace AetherGraph {

// ======================================================================
// VectorizedSeqScan Implementation
// ======================================================================
VectorizedSeqScan::VectorizedSeqScan(GraphEngine& ge, const std::string& label)
    : ge_(ge), label_(label) {
    auto all = ge_.get_all_nodes();
    for (auto* n : all) {
        if (n->label == label_) {
            nodes_.push_back(n);
        }
    }
}

bool VectorizedSeqScan::next(VectorBatch& batch) {
    batch.size = 0;
    batch.node_ids.clear();
    batch.int_values.clear();

    if (cursor_ >= nodes_.size()) return false;

    // Fill batch up to 1024 elements (vectorized batch size)
    size_t limit = std::min(nodes_.size() - cursor_, static_cast<size_t>(1024));
    for (size_t i = 0; i < limit; ++i) {
        auto* n = nodes_[cursor_ + i];
        batch.node_ids.push_back(n->id);
        
        // Populate default int value for filtering (e.g. age)
        auto it = n->properties.find("age");
        if (it != n->properties.end() && it->second.type == DataType::INT) {
            batch.int_values.push_back(it->second.get_int());
        } else {
            batch.int_values.push_back(0);
        }
    }
    batch.size = limit;
    cursor_ += limit;
    return true;
}

// ======================================================================
// VectorizedFilter Implementation
// ======================================================================
VectorizedFilter::VectorizedFilter(std::unique_ptr<VectorizedOperator> child, const std::string& key, int32_t target_val)
    : child_(std::move(child)), key_(key), target_val_(target_val) {}

bool VectorizedFilter::next(VectorBatch& batch) {
    while (child_->next(batch)) {
        if (batch.size == 0) continue;

        // Perform SIMD-like filtering on the batch columns
        std::vector<node_id_t> filtered_ids;
        std::vector<int32_t> filtered_vals;

        for (size_t i = 0; i < batch.size; ++i) {
            if (batch.int_values[i] == target_val_) {
                filtered_ids.push_back(batch.node_ids[i]);
                filtered_vals.push_back(batch.int_values[i]);
            }
        }

        batch.node_ids = std::move(filtered_ids);
        batch.int_values = std::move(filtered_vals);
        batch.size = batch.node_ids.size();

        if (batch.size > 0) return true;
    }
    return false;
}

} // namespace AetherGraph

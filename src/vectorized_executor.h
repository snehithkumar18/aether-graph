#ifndef AETHER_GRAPH_VECTORIZED_EXECUTOR_H
#define AETHER_GRAPH_VECTORIZED_EXECUTOR_H

#include "graph_engine.h"
#include <vector>
#include <memory>

namespace AetherGraph {

struct VectorBatch {
    std::vector<node_id_t> node_ids;
    std::vector<int32_t> int_values;
    size_t size = 0;
};

class VectorizedOperator {
public:
    virtual ~VectorizedOperator() = default;
    virtual bool next(VectorBatch& batch) = 0;
};

class VectorizedSeqScan : public VectorizedOperator {
private:
    GraphEngine& ge_;
    std::string label_;
    size_t cursor_ = 0;
    std::vector<Node*> nodes_;

public:
    VectorizedSeqScan(GraphEngine& ge, const std::string& label);
    bool next(VectorBatch& batch) override;
};

class VectorizedFilter : public VectorizedOperator {
private:
    std::unique_ptr<VectorizedOperator> child_;
    std::string key_;
    int32_t target_val_;

public:
    VectorizedFilter(std::unique_ptr<VectorizedOperator> child, const std::string& key, int32_t target_val);
    bool next(VectorBatch& batch) override;
};

} // namespace AetherGraph

#endif // AETHER_GRAPH_VECTORIZED_EXECUTOR_H

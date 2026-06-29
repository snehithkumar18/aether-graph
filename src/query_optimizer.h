#ifndef AETHER_GRAPH_QUERY_OPTIMIZER_H
#define AETHER_GRAPH_QUERY_OPTIMIZER_H

#include "graph_statistics.h"
#include <string>
#include <vector>
#include <memory>

namespace AetherGraph {

enum class PlanNodeType {
    SEQ_SCAN,
    INDEX_SCAN,
    FILTER,
    NESTED_LOOP_JOIN,
    PROJECT
};

struct OptimizerPlanNode {
    PlanNodeType type;
    std::string label;
    std::string key;
    std::string op;
    Variant val;
    
    // Join properties
    std::string join_key;
    
    double cost = 0.0;
    double estimated_rows = 0.0;

    std::vector<std::shared_ptr<OptimizerPlanNode>> children;

    explicit OptimizerPlanNode(PlanNodeType t) : type(t) {}
};

class QueryOptimizer {
private:
    const GraphStatistics& stats_;

    double calculate_cost(const std::shared_ptr<OptimizerPlanNode>& node);
    void optimize_pushdowns(std::shared_ptr<OptimizerPlanNode>& node);
    std::shared_ptr<OptimizerPlanNode> choose_scan_method(const std::string& label, const std::string& key, const std::string& op, const Variant& val);

public:
    explicit QueryOptimizer(const GraphStatistics& stats) : stats_(stats) {}
    ~QueryOptimizer() = default;

    std::shared_ptr<OptimizerPlanNode> generate_optimized_plan(const ParsedQuery& query);
};

} // namespace AetherGraph

#endif // AETHER_GRAPH_QUERY_OPTIMIZER_H

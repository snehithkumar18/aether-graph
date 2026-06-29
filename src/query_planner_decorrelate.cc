#include "query_planner_decorrelate.h"

namespace AetherGraph {

std::shared_ptr<OptimizerPlanNode> QueryPlannerDecorrelate::decorrelate(
    const std::shared_ptr<OptimizerPlanNode>& plan) {
    if (!plan) return nullptr;

    // Recurse on children
    for (size_t i = 0; i < plan->children.size(); ++i) {
        plan->children[i] = decorrelate(plan->children[i]);
    }

    // If it is a correlated filter, rewrite it as a join
    if (plan->type == PlanNodeType::FILTER && !plan->children.empty()) {
        auto child = plan->children[0];
        if (child->type == PlanNodeType::SEQ_SCAN) {
            // Found a correlation point
            auto decorrelated_join = std::make_shared<OptimizerPlanNode>(PlanNodeType::NESTED_LOOP_JOIN);
            decorrelated_join->key = plan->key;
            decorrelated_join->op = plan->op;
            decorrelated_join->val = plan->val;
            decorrelated_join->children.push_back(child);
            return decorrelated_join;
        }
    }

    return plan;
}

} // namespace AetherGraph

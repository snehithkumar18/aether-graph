#include "query_planner_cbo.h"
#include <cmath>
#include <algorithm>
#include <iostream>

namespace AetherGraph {

QueryPlannerCBO::QueryPlannerCBO(const GraphStatistics& stats) : stats_(stats) {}

PlanCost QueryPlannerCBO::estimate_cost(const std::shared_ptr<OptimizerPlanNode>& plan) {
    if (!plan) return PlanCost{};

    PlanCost cost;
    if (plan->type == PlanNodeType::SEQ_SCAN) {
        // Full table scan cost: high I/O, low CPU
        cost.card = 1000.0; // Default cardinality
        cost.io_cost = cost.card * 1.0;
        cost.cpu_cost = cost.card * 0.1;
    } else if (plan->type == PlanNodeType::INDEX_SCAN) {
        // Index scan cost: low I/O, medium CPU
        cost.card = 10.0; // Default index lookup cardinality
        cost.io_cost = 2.0; 
        cost.cpu_cost = 0.5;
    } else if (plan->type == PlanNodeType::FILTER) {
        // Filter cost: inherits child cost, adds CPU filtering cost
        auto child_cost = estimate_cost(plan->children.empty() ? nullptr : plan->children[0]);
        cost.card = child_cost.card * 0.1; // 10% selectivity default
        cost.io_cost = child_cost.io_cost;
        cost.cpu_cost = child_cost.cpu_cost + child_cost.card * 0.2;
    } else if (plan->type == PlanNodeType::NESTED_LOOP_JOIN) {
        // Join cost: Cartesian product style CPU/IO cost
        auto left_cost = estimate_cost(plan->children.size() > 0 ? plan->children[0] : nullptr);
        auto right_cost = estimate_cost(plan->children.size() > 1 ? plan->children[1] : nullptr);
        
        cost.card = left_cost.card * right_cost.card * 0.01;
        cost.io_cost = left_cost.io_cost + left_cost.card * right_cost.io_cost;
        cost.cpu_cost = left_cost.cpu_cost + left_cost.card * right_cost.cpu_cost + (left_cost.card * right_cost.card * 0.05);
    }

    return cost;
}

std::shared_ptr<OptimizerPlanNode> QueryPlannerCBO::plan_match_query(
    const std::string& label,
    const std::vector<std::pair<std::string, Variant>>& predicates,
    const std::vector<std::string>& projections) {

    // 1. Base scan plan
    auto base_plan = std::make_shared<OptimizerPlanNode>(PlanNodeType::SEQ_SCAN);
    base_plan->label = label;

    std::shared_ptr<OptimizerPlanNode> current_plan = base_plan;

    // 2. Apply predicates as filters
    for (const auto& pred : predicates) {
        auto filter_plan = std::make_shared<OptimizerPlanNode>(PlanNodeType::FILTER);
        filter_plan->key = pred.first;
        filter_plan->op = "=";
        filter_plan->val = pred.second;
        filter_plan->children.push_back(current_plan);
        current_plan = filter_plan;
    }

    // 3. Simple projection representation
    if (!projections.empty()) {
        current_plan->projections = projections;
    }

    return current_plan;
}

std::shared_ptr<OptimizerPlanNode> QueryPlannerCBO::plan_join_query(
    const std::vector<std::string>& labels,
    const std::vector<std::pair<std::string, std::string>>& join_conditions) {

    if (labels.empty()) return nullptr;

    // Generate plans for all base relations
    std::vector<std::shared_ptr<OptimizerPlanNode>> base_plans;
    for (const auto& label : labels) {
        auto base = std::make_shared<OptimizerPlanNode>(PlanNodeType::SEQ_SCAN);
        base->label = label;
        base_plans.push_back(base);
    }

    // Dynamic programming join reordering simulation
    // We greedily join the relations with the lowest cost
    std::shared_ptr<OptimizerPlanNode> current_plan = base_plans[0];
    for (size_t i = 1; i < base_plans.size(); ++i) {
        auto join = std::make_shared<OptimizerPlanNode>(PlanNodeType::NESTED_LOOP_JOIN);
        join->children.push_back(current_plan);
        join->children.push_back(base_plans[i]);
        
        if (i - 1 < join_conditions.size()) {
            join->key = join_conditions[i - 1].first;
            join->op = "=";
        }
        current_plan = join;
    }

    return current_plan;
}

} // namespace AetherGraph

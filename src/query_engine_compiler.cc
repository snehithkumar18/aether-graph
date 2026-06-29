#include "query_engine_compiler.h"
#include <iostream>

namespace AetherGraph {

std::unique_ptr<PhysicalOperator> QueryEngineCompiler::compile_to_physical(
    GraphEngine& ge,
    const std::shared_ptr<OptimizerPlanNode>& plan) {
    if (!plan) return nullptr;

    if (plan->type == PlanNodeType::SEQ_SCAN) {
        return std::make_unique<PhysicalSeqScan>(ge, plan->label);
    }

    if (plan->type == PlanNodeType::FILTER) {
        if (plan->children.empty()) return nullptr;
        auto child = compile_to_physical(ge, plan->children[0]);
        return std::make_unique<PhysicalFilter>(std::move(child), plan->key, plan->op, plan->val);
    }

    if (plan->type == PlanNodeType::NESTED_LOOP_JOIN) {
        if (plan->children.size() < 2) return nullptr;
        auto left = compile_to_physical(ge, plan->children[0]);
        auto right = compile_to_physical(ge, plan->children[1]);
        return std::make_unique<PhysicalNestedLoopJoin>(std::move(left), std::move(right));
    }

    return nullptr;
}

} // namespace AetherGraph

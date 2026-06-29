#include "query_engine_compiler_pass.h"

namespace AetherGraph {

std::shared_ptr<OptimizerPlanNode> QueryEngineCompilerPass::optimize_constant_folding(
    std::shared_ptr<OptimizerPlanNode> plan) {
    if (!plan) return nullptr;

    // Recurse on children
    for (size_t i = 0; i < plan->children.size(); ++i) {
        plan->children[i] = optimize_constant_folding(plan->children[i]);
    }

    // Fold constants: if filter has a constant expression that evaluates to false
    if (plan->type == PlanNodeType::FILTER) {
        // Simple folding simulation: if key is empty, fold it
        if (plan->key.empty()) {
            return nullptr; // Folded/removed
        }
    }

    return plan;
}

std::shared_ptr<OptimizerPlanNode> QueryEngineCompilerPass::optimize_dead_code_elimination(
    std::shared_ptr<OptimizerPlanNode> plan) {
    if (!plan) return nullptr;

    for (size_t i = 0; i < plan->children.size(); ++i) {
        plan->children[i] = optimize_dead_code_elimination(plan->children[i]);
    }

    // If a filter has no child scan, it's dead code
    if (plan->type == PlanNodeType::FILTER && plan->children.empty()) {
        return nullptr;
    }

    return plan;
}

} // namespace AetherGraph

#include "query_compiler.h"

namespace AetherGraph {

QueryCompiler::QueryCompiler(GraphEngine& ge, const QueryOptimizer& optimizer)
    : ge_(ge), optimizer_(optimizer) {}

void QueryCompiler::register_index(const std::string& property_key, std::shared_ptr<PropertyIndex> index) {
    indexes_[property_key] = index;
}

static std::unique_ptr<PhysicalOperator> compile_node(
    GraphEngine& ge,
    const std::shared_ptr<OptimizerPlanNode>& opt_node,
    const std::unordered_map<std::string, std::shared_ptr<PropertyIndex>>& indexes) {

    if (!opt_node) return nullptr;

    if (opt_node->type == PlanNodeType::SEQ_SCAN) {
        return std::make_unique<PhysicalSeqScan>(ge, opt_node->label);
    }
    
    if (opt_node->type == PlanNodeType::INDEX_SCAN) {
        auto it = indexes.find(opt_node->key);
        if (it != indexes.end() && it->second) {
            std::string val_str;
            if (opt_node->val.type == DataType::INT) val_str = std::to_string(opt_node->val.get_int());
            else if (opt_node->val.type == DataType::FLOAT) val_str = std::to_string(opt_node->val.get_float());
            else if (opt_node->val.type == DataType::STRING) val_str = opt_node->val.get_string();

            return std::make_unique<PhysicalIndexScan>(ge, *(it->second), val_str);
        }
        // Fallback to SeqScan if index is missing
        return std::make_unique<PhysicalSeqScan>(ge, opt_node->label);
    }

    if (opt_node->type == PlanNodeType::FILTER) {
        if (opt_node->children.empty()) return nullptr;
        auto child_op = compile_node(ge, opt_node->children[0], indexes);
        return std::make_unique<PhysicalFilter>(std::move(child_op), opt_node->key, opt_node->op, opt_node->val);
    }

    if (opt_node->type == PlanNodeType::NESTED_LOOP_JOIN) {
        if (opt_node->children.size() < 2) return nullptr;
        auto left_op = compile_node(ge, opt_node->children[0], indexes);
        auto right_op = compile_node(ge, opt_node->children[1], indexes);
        return std::make_unique<PhysicalNestedLoopJoin>(std::move(left_op), std::move(right_op));
    }

    return nullptr;
}

std::unique_ptr<PhysicalOperator> QueryCompiler::compile(const ParsedQuery& query) {
    // 1. Generate optimized logical plan
    auto opt_plan = optimizer_.generate_optimized_plan(query);
    if (!opt_plan) return nullptr;

    // 2. Recursively compile logical plan nodes to physical operators
    return compile_node(ge_, opt_plan, indexes_);
}

} // namespace AetherGraph

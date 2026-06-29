#include "query_optimizer.h"

namespace AetherGraph {

double QueryOptimizer::calculate_cost(const std::shared_ptr<OptimizerPlanNode>& node) {
    if (!node) return 0.0;

    double child_cost = 0.0;
    for (const auto& child : node->children) {
        child_cost += calculate_cost(child);
    }

    if (node->type == PlanNodeType::SEQ_SCAN) {
        // Sequential scan: Cost scales linearly with node count
        node->estimated_rows = stats_.get_total_nodes();
        node->cost = node->estimated_rows * 0.1;
    } else if (node->type == PlanNodeType::INDEX_SCAN) {
        // Index lookup: Cost scales logarithmically
        double selectivity = stats_.estimate_node_selectivity(node->label, node->key, node->op, node->val);
        node->estimated_rows = stats_.get_total_nodes() * selectivity;
        node->cost = 1.5 + (node->estimated_rows * 0.02);
    } else if (node->type == PlanNodeType::FILTER) {
        if (!node->children.empty()) {
            double selectivity = stats_.estimate_node_selectivity(node->label, node->key, node->op, node->val);
            node->estimated_rows = node->children[0]->estimated_rows * selectivity;
            node->cost = node->children[0]->cost + (node->children[0]->estimated_rows * 0.05);
        }
    } else if (node->type == PlanNodeType::NESTED_LOOP_JOIN) {
        if (node->children.size() >= 2) {
            double left_rows = node->children[0]->estimated_rows;
            double right_rows = node->children[1]->estimated_rows;
            node->estimated_rows = left_rows * (right_rows * 0.05); // Join estimation
            node->cost = node->children[0]->cost + (left_rows * node->children[1]->cost);
        }
    } else if (node->type == PlanNodeType::PROJECT) {
        if (!node->children.empty()) {
            node->estimated_rows = node->children[0]->estimated_rows;
            node->cost = node->children[0]->cost + (node->estimated_rows * 0.01);
        }
    }

    return node->cost + child_cost;
}

std::shared_ptr<OptimizerPlanNode> QueryOptimizer::choose_scan_method(
    const std::string& label, const std::string& key, const std::string& op, const Variant& val) {

    // Evaluate index lookup vs table scan
    double selectivity = stats_.estimate_node_selectivity(label, key, op, val);
    
    // Choose index scan if selectivity is high (small fraction of rows)
    if (selectivity < 0.25 && op == "=") {
        auto idx_node = std::make_shared<OptimizerPlanNode>(PlanNodeType::INDEX_SCAN);
        idx_node->label = label;
        idx_node->key = key;
        idx_node->op = op;
        idx_node->val = val;
        return idx_node;
    }

    // Default to seq scan + filter
    auto seq_node = std::make_shared<OptimizerPlanNode>(PlanNodeType::SEQ_SCAN);
    seq_node->label = label;

    auto filter_node = std::make_shared<OptimizerPlanNode>(PlanNodeType::FILTER);
    filter_node->label = label;
    filter_node->key = key;
    filter_node->op = op;
    filter_node->val = val;
    filter_node->children.push_back(seq_node);

    return filter_node;
}

void QueryOptimizer::optimize_pushdowns(std::shared_ptr<OptimizerPlanNode>& node) {
    if (!node) return;

    for (auto& child : node->children) {
        optimize_pushdowns(child);
    }

    // Optimize FILTER + SEQ_SCAN into INDEX_SCAN if applicable
    if (node->type == PlanNodeType::FILTER && !node->children.empty()) {
        auto child = node->children[0];
        if (child->type == PlanNodeType::SEQ_SCAN && node->op == "=") {
            double selectivity = stats_.estimate_node_selectivity(child->label, node->key, node->op, node->val);
            if (selectivity < 0.25) {
                // Perform the pushdown transform
                auto opt_node = choose_scan_method(child->label, node->key, node->op, node->val);
                node = opt_node;
            }
        }
    }
}

std::shared_ptr<OptimizerPlanNode> QueryOptimizer::generate_optimized_plan(const ParsedQuery& query) {
    std::shared_ptr<OptimizerPlanNode> root_node;

    if (query.type == QueryType::MATCH_NODE) {
        if (query.has_property) {
            root_node = choose_scan_method(query.node_label, query.property_key, "=", query.property_value);
        } else {
            root_node = std::make_shared<OptimizerPlanNode>(PlanNodeType::SEQ_SCAN);
            root_node->label = query.node_label;
        }
    } else if (query.type == QueryType::MATCH_PATH) {
        // Build a Nested Loop Join plan for paths
        auto join = std::make_shared<OptimizerPlanNode>(PlanNodeType::NESTED_LOOP_JOIN);
        
        auto left_scan = std::make_shared<OptimizerPlanNode>(PlanNodeType::SEQ_SCAN);
        left_scan->label = query.node_label;

        auto right_scan = std::make_shared<OptimizerPlanNode>(PlanNodeType::SEQ_SCAN);
        right_scan->label = query.node_label;

        join->children.push_back(left_scan);
        join->children.push_back(right_scan);
        join->join_key = "id";

        root_node = join;
    } else {
        // Fallback placeholder
        root_node = std::make_shared<OptimizerPlanNode>(PlanNodeType::SEQ_SCAN);
    }

    optimize_pushdowns(root_node);
    calculate_cost(root_node);

    return root_node;
}

} // namespace AetherGraph

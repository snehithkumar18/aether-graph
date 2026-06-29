#ifndef AETHER_GRAPH_QUERY_PLANNER_CBO_H
#define AETHER_GRAPH_QUERY_PLANNER_CBO_H

#include "query_parser.h"
#include "query_optimizer.h"
#include "graph_statistics.h"
#include "execution_plans.h"
#include <memory>
#include <vector>
#include <unordered_map>
#include <string>

namespace AetherGraph {

struct PlanCost {
    double cpu_cost = 0.0;
    double io_cost = 0.0;
    double card = 0.0;
    double total_cost() const { return cpu_cost + io_cost; }
};

class QueryPlannerCBO {
private:
    GraphStatistics stats_;
    double selectivity_scale_ = 1.0;

public:
    explicit QueryPlannerCBO(const GraphStatistics& stats);
    ~QueryPlannerCBO() = default;

    std::shared_ptr<OptimizerPlanNode> plan_match_query(
        const std::string& label,
        const std::vector<std::pair<std::string, Variant>>& predicates,
        const std::vector<std::string>& projections);

    std::shared_ptr<OptimizerPlanNode> plan_join_query(
        const std::vector<std::string>& labels,
        const std::vector<std::pair<std::string, std::string>>& join_conditions);

    PlanCost estimate_cost(const std::shared_ptr<OptimizerPlanNode>& plan);
};

} // namespace AetherGraph

#endif // AETHER_GRAPH_QUERY_PLANNER_CBO_H

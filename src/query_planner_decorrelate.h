#ifndef AETHER_GRAPH_QUERY_PLANNER_DECORRELATE_H
#define AETHER_GRAPH_QUERY_PLANNER_DECORRELATE_H

#include "query_planner_cbo.h"
#include <memory>

namespace AetherGraph {

class QueryPlannerDecorrelate {
public:
    QueryPlannerDecorrelate() = default;
    ~QueryPlannerDecorrelate() = default;

    std::shared_ptr<OptimizerPlanNode> decorrelate(
        const std::shared_ptr<OptimizerPlanNode>& plan);
};

} // namespace AetherGraph

#endif // AETHER_GRAPH_QUERY_PLANNER_DECORRELATE_H

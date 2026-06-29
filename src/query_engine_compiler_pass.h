#ifndef AETHER_GRAPH_QUERY_ENGINE_COMPILER_PASS_H
#define AETHER_GRAPH_QUERY_ENGINE_COMPILER_PASS_H

#include "query_planner_cbo.h"
#include <memory>

namespace AetherGraph {

class QueryEngineCompilerPass {
public:
    QueryEngineCompilerPass() = default;
    ~QueryEngineCompilerPass() = default;

    std::shared_ptr<OptimizerPlanNode> optimize_constant_folding(
        std::shared_ptr<OptimizerPlanNode> plan);

    std::shared_ptr<OptimizerPlanNode> optimize_dead_code_elimination(
        std::shared_ptr<OptimizerPlanNode> plan);
};

} // namespace AetherGraph

#endif // AETHER_GRAPH_QUERY_ENGINE_COMPILER_PASS_H

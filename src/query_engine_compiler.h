#ifndef AETHER_GRAPH_QUERY_ENGINE_COMPILER_H
#define AETHER_GRAPH_QUERY_ENGINE_COMPILER_H

#include "execution_plans.h"
#include <string>
#include <vector>
#include <memory>

namespace AetherGraph {

class QueryEngineCompiler {
public:
    QueryEngineCompiler() = default;
    ~QueryEngineCompiler() = default;

    std::unique_ptr<PhysicalOperator> compile_to_physical(
        GraphEngine& ge,
        const std::shared_ptr<OptimizerPlanNode>& plan);
};

} // namespace AetherGraph

#endif // AETHER_GRAPH_QUERY_ENGINE_COMPILER_H

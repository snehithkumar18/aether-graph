#ifndef AETHER_GRAPH_QUERY_ENGINE_COMPILER_PIVOT_H
#define AETHER_GRAPH_QUERY_ENGINE_COMPILER_PIVOT_H

#include "execution_plans.h"
#include <string>
#include <vector>

namespace AetherGraph {

struct PivotFrame {
    std::string row_key;
    std::string col_key;
    std::string val_key;
};

class QueryEngineCompilerPivot {
public:
    QueryEngineCompilerPivot() = default;
    ~QueryEngineCompilerPivot() = default;

    void compile_pivot(
        const PivotFrame& frame,
        const std::vector<Node*>& input,
        std::vector<std::vector<std::string>>& output_matrix);
};

} // namespace AetherGraph

#endif // AETHER_GRAPH_QUERY_ENGINE_COMPILER_PIVOT_H

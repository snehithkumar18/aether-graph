#ifndef AETHER_GRAPH_QUERY_ENGINE_COMPILER_WINDOW_H
#define AETHER_GRAPH_QUERY_ENGINE_COMPILER_WINDOW_H

#include "execution_plans.h"
#include <string>
#include <vector>

namespace AetherGraph {

struct WindowFrame {
    std::string partition_by_key;
    std::string order_by_key;
    std::string function_name; // e.g., "ROW_NUMBER", "DENSE_RANK"
};

class QueryEngineCompilerWindow {
public:
    QueryEngineCompilerWindow() = default;
    ~QueryEngineCompilerWindow() = default;

    void compile_window_operator(
        const WindowFrame& frame,
        const std::vector<Node*>& input,
        std::vector<std::pair<Node*, int32_t>>& output);
};

} // namespace AetherGraph

#endif // AETHER_GRAPH_QUERY_ENGINE_COMPILER_WINDOW_H

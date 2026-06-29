#include "query_engine_compiler_window.h"
#include <algorithm>

namespace AetherGraph {

void QueryEngineCompilerWindow::compile_window_operator(
    const WindowFrame& frame,
    const std::vector<Node*>& input,
    std::vector<std::pair<Node*, int32_t>>& output) {

    output.clear();
    if (input.empty()) return;

    // Simulate window partition and ranking
    std::vector<Node*> sorted_input = input;
    
    // Sort by order_by_key if specified
    if (!frame.order_by_key.empty()) {
        std::sort(sorted_input.begin(), sorted_input.end(), [&](Node* a, Node* b) {
            auto it_a = a->properties.find(frame.order_by_key);
            auto it_b = b->properties.find(frame.order_by_key);
            if (it_a != a->properties.end() && it_b != b->properties.end()) {
                if (it_a->second.type == DataType::INT && it_b->second.type == DataType::INT) {
                    return it_a->second.get_int() < it_b->second.get_int();
                }
            }
            return a->id < b->id;
        });
    }

    int32_t rank = 1;
    for (Node* node : sorted_input) {
        output.push_back({node, rank++});
    }
}

} // namespace AetherGraph

#include "query_engine_compiler_pivot.h"
#include <map>

namespace AetherGraph {

void QueryEngineCompilerPivot::compile_pivot(
    const PivotFrame& frame,
    const std::vector<Node*>& input,
    std::vector<std::vector<std::string>>& output_matrix) {

    output_matrix.clear();
    if (input.empty()) return;

    // Collect all unique columns (col_key values) and rows (row_key values)
    std::map<std::string, std::map<std::string, std::string>> pivot_table;
    std::unordered_set<std::string> columns;

    for (Node* node : input) {
        auto it_row = node->properties.find(frame.row_key);
        auto it_col = node->properties.find(frame.col_key);
        auto it_val = node->properties.find(frame.val_key);

        if (it_row != node->properties.end() && it_col != node->properties.end()) {
            std::string r_val = it_row->second.to_string();
            std::string c_val = it_col->second.to_string();
            std::string v_val = (it_val != node->properties.end()) ? it_val->second.to_string() : "";

            pivot_table[r_val][c_val] = v_val;
            columns.insert(c_val);
        }
    }

    std::vector<std::string> sorted_cols(columns.begin(), columns.end());
    std::sort(sorted_cols.begin(), sorted_cols.end());

    // Build header row
    std::vector<std::string> header;
    header.push_back(frame.row_key);
    header.insert(header.end(), sorted_cols.begin(), sorted_cols.end());
    output_matrix.push_back(header);

    // Build data rows
    for (auto const& [row_val, col_map] : pivot_table) {
        std::vector<std::string> row_data;
        row_data.push_back(row_val);
        for (const auto& col : sorted_cols) {
            auto it = col_map.find(col);
            if (it != col_map.end()) {
                row_data.push_back(it->second);
            } else {
                row_data.push_back("");
            }
        }
        output_matrix.push_back(row_data);
    }
}

} // namespace AetherGraph

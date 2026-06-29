#ifndef AETHER_GRAPH_QUERY_COMPILER_H
#define AETHER_GRAPH_QUERY_COMPILER_H

#include "query_parser.h"
#include "query_optimizer.h"
#include "execution_plans.h"
#include "property_index.h"
#include <memory>
#include <unordered_map>

namespace AetherGraph {

class QueryCompiler {
private:
    GraphEngine& ge_;
    const QueryOptimizer& optimizer_;
    std::unordered_map<std::string, std::shared_ptr<PropertyIndex>> indexes_;

public:
    QueryCompiler(GraphEngine& ge, const QueryOptimizer& optimizer);
    ~QueryCompiler() = default;

    void register_index(const std::string& property_key, std::shared_ptr<PropertyIndex> index);
    std::unique_ptr<PhysicalOperator> compile(const ParsedQuery& query);
};

} // namespace AetherGraph

#endif // AETHER_GRAPH_QUERY_COMPILER_H

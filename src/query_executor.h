#ifndef AETHER_GRAPH_QUERY_EXECUTOR_H
#define AETHER_GRAPH_QUERY_EXECUTOR_H

#include "utils.h"
#include "graph_engine.h"
#include "transaction_manager.h"
#include "query_parser.h"
#include <vector>
#include <unordered_set>

namespace AetherGraph {

class QueryExecutor {
private:
    GraphEngine& graph_engine_;
    TransactionManager& txn_mgr_;

    // Cached pointer to last resolved property version for fast repeated lookups
    PropertyVersion* cached_version_ = nullptr;
    node_id_t cached_node_id_ = 0;
    std::string cached_key_;

    // Traversal helpers
    void dfs_find_matching(Node* current, const std::string& match_label,
                           std::vector<Node*>& results);
    bool bfs_path_exists(Node* start, Node* target);

public:
    QueryExecutor(GraphEngine& ge, TransactionManager& tm)
        : graph_engine_(ge), txn_mgr_(tm) {}

    ~QueryExecutor() = default;

    bool execute(Transaction* txn, const ParsedQuery& query, std::vector<Node*>& results);
    void invalidate_cache() { cached_version_ = nullptr; cached_node_id_ = 0; }
};

} // namespace AetherGraph

#endif // AETHER_GRAPH_QUERY_EXECUTOR_H

#ifndef AETHER_GRAPH_CONCURRENCY_CONTROL_H
#define AETHER_GRAPH_CONCURRENCY_CONTROL_H

#include "graph_engine.h"
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <mutex>

namespace AetherGraph {

enum class LockMode {
    SHARED,
    EXCLUSIVE
};

class ConcurrencyControl {
private:
    std::mutex mutex_;
    
    // Lock table mapping: resource_id (node or edge) -> holders and mode
    struct LockRequest {
        txn_id_t txn_id;
        LockMode mode;
    };
    
    std::unordered_map<uint64_t, std::vector<LockRequest>> lock_table_;
    
    // Wait-for graph for deadlock detection
    std::unordered_map<txn_id_t, std::unordered_set<txn_id_t>> wait_for_graph_;

    bool detect_cycle_dfs(txn_id_t curr, std::unordered_set<txn_id_t>& visited, std::unordered_set<txn_id_t>& stack, std::vector<txn_id_t>& cycle);

public:
    ConcurrencyControl() = default;
    ~ConcurrencyControl() = default;

    bool acquire_lock(txn_id_t txn_id, uint64_t resource_id, LockMode mode);
    bool release_lock(txn_id_t txn_id, uint64_t resource_id);
    void release_all_locks(txn_id_t txn_id);
    
    bool detect_deadlock(std::vector<txn_id_t>& cycle);
};

} // namespace AetherGraph

#endif // AETHER_GRAPH_CONCURRENCY_CONTROL_H

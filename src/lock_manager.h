#ifndef AETHER_GRAPH_LOCK_MANAGER_H
#define AETHER_GRAPH_LOCK_MANAGER_H

#include "utils.h"
#include <unordered_map>
#include <list>
#include <mutex>
#include <condition_variable>

namespace AetherGraph {

enum class LockMode {
    SHARED,
    EXCLUSIVE
};

struct LockRequest {
    txn_id_t txn_id;
    LockMode lock_mode;
    bool granted = false;

    LockRequest(txn_id_t tid, LockMode mode) : txn_id(tid), lock_mode(mode) {}
};

struct LockRequestQueue {
    std::list<LockRequest> request_queue;
    std::condition_variable cv;
    bool has_exclusive = false;
    int shared_count = 0;
};

class LockManager {
private:
    std::unordered_map<node_id_t, LockRequestQueue> lock_table_;
    std::mutex mutex_;

    bool is_compatible(const LockRequestQueue& queue, LockMode mode, txn_id_t txn_id) const;

public:
    LockManager() = default;
    ~LockManager() = default;

    bool acquire_shared(txn_id_t txn_id, node_id_t node_id);
    bool acquire_exclusive(txn_id_t txn_id, node_id_t node_id);
    bool release(txn_id_t txn_id, node_id_t node_id);
};

} // namespace AetherGraph

#endif // AETHER_GRAPH_LOCK_MANAGER_H

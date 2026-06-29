#ifndef AETHER_GRAPH_LOCK_MANAGER_ADVANCED_H
#define AETHER_GRAPH_LOCK_MANAGER_ADVANCED_H

#include "lock_manager.h"
#include <unordered_map>
#include <string>
#include <vector>
#include <mutex>

namespace AetherGraph {

enum class IsolationLevel {
    READ_UNCOMMITTED,
    READ_COMMITTED,
    REPEATABLE_READ,
    SERIALIZABLE
};

class LockManagerAdvanced {
private:
    std::unordered_map<std::string, std::vector<uint32_t>> shared_locks_;
    std::unordered_map<std::string, uint32_t> exclusive_locks_;
    std::mutex mutex_;
    IsolationLevel level_ = IsolationLevel::SERIALIZABLE;

public:
    LockManagerAdvanced() = default;
    ~LockManagerAdvanced() = default;

    void set_isolation_level(IsolationLevel level) { level_ = level; }
    IsolationLevel get_isolation_level() const { return level_; }

    bool acquire_shared(uint32_t txn_id, const std::string& resource_id);
    bool acquire_exclusive(uint32_t txn_id, const std::string& resource_id);
    void release(uint32_t txn_id, const std::string& resource_id);
};

} // namespace AetherGraph

#endif // AETHER_GRAPH_LOCK_MANAGER_ADVANCED_H

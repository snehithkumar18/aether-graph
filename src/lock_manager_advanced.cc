#include "lock_manager_advanced.h"
#include <algorithm>

namespace AetherGraph {

bool LockManagerAdvanced::acquire_shared(uint32_t txn_id, const std::string& resource_id) {
    std::lock_guard<std::mutex> lock(mutex_);

    // Read Uncommitted does not acquire read locks
    if (level_ == IsolationLevel::READ_UNCOMMITTED) {
        return true;
    }

    auto it_exc = exclusive_locks_.find(resource_id);
    if (it_exc != exclusive_locks_.end() && it_exc->second != txn_id) {
        return false; // Conflicting exclusive lock held by another txn
    }

    auto& readers = shared_locks_[resource_id];
    if (std::find(readers.begin(), readers.end(), txn_id) == readers.end()) {
        readers.push_back(txn_id);
    }
    return true;
}

bool LockManagerAdvanced::acquire_exclusive(uint32_t txn_id, const std::string& resource_id) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it_exc = exclusive_locks_.find(resource_id);
    if (it_exc != exclusive_locks_.end() && it_exc->second != txn_id) {
        return false; // Conflicting exclusive lock held by another txn
    }

    auto it_sh = shared_locks_.find(resource_id);
    if (it_sh != shared_locks_.end()) {
        for (uint32_t reader : it_sh->second) {
            if (reader != txn_id) {
                return false; // Conflicting shared locks held by other txns
            }
        }
    }

    exclusive_locks_[resource_id] = txn_id;
    return true;
}

void LockManagerAdvanced::release(uint32_t txn_id, const std::string& resource_id) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it_exc = exclusive_locks_.find(resource_id);
    if (it_exc != exclusive_locks_.end() && it_exc->second == txn_id) {
        exclusive_locks_.erase(it_exc);
    }

    auto it_sh = shared_locks_.find(resource_id);
    if (it_sh != shared_locks_.end()) {
        auto& readers = it_sh->second;
        readers.erase(std::remove(readers.begin(), readers.end(), txn_id), readers.end());
        if (readers.empty()) {
            shared_locks_.erase(it_sh);
        }
    }
}

} // namespace AetherGraph

#include "concurrency_control.h"
#include <algorithm>

namespace AetherGraph {

bool ConcurrencyControl::acquire_lock(txn_id_t txn_id, uint64_t resource_id, LockMode mode) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto& requests = lock_table_[resource_id];
    
    // Check if txn_id already holds a lock on this resource
    for (auto& req : requests) {
        if (req.txn_id == txn_id) {
            if (req.mode == mode || (req.mode == LockMode::EXCLUSIVE && mode == LockMode::SHARED)) {
                return true; // Already holds sufficient lock
            }
            if (req.mode == LockMode::SHARED && mode == LockMode::EXCLUSIVE) {
                // Lock upgrade request
                if (requests.size() == 1) {
                    req.mode = LockMode::EXCLUSIVE;
                    return true;
                }
                // Upgrade conflict: wait-for mapping
                for (const auto& other : requests) {
                    if (other.txn_id != txn_id) {
                        wait_for_graph_[txn_id].insert(other.txn_id);
                    }
                }
                return false; // Conflicting upgrade
            }
        }
    }

    // Check for lock conflict with other transactions
    bool conflict = false;
    for (const auto& req : requests) {
        if (req.txn_id != txn_id) {
            if (mode == LockMode::EXCLUSIVE || req.mode == LockMode::EXCLUSIVE) {
                conflict = true;
                wait_for_graph_[txn_id].insert(req.txn_id);
            }
        }
    }

    if (conflict) {
        return false;
    }

    // Lock acquired
    requests.push_back({txn_id, mode});
    wait_for_graph_.erase(txn_id); // Not waiting anymore
    return true;
}

bool ConcurrencyControl::release_lock(txn_id_t txn_id, uint64_t resource_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = lock_table_.find(resource_id);
    if (it == lock_table_.end()) return false;

    auto& requests = it->second;
    auto rit = std::remove_if(requests.begin(), requests.end(), [txn_id](const LockRequest& r) {
        return r.txn_id == txn_id;
    });

    bool found = (rit != requests.end());
    requests.erase(rit, requests.end());

    if (requests.empty()) {
        lock_table_.erase(it);
    }
    return found;
}

void ConcurrencyControl::release_all_locks(txn_id_t txn_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // Clear waiting status in wait-for graph
    wait_for_graph_.erase(txn_id);
    for (auto& [waiter, holders] : wait_for_graph_) {
        holders.erase(txn_id);
    }

    // Release held locks in lock table
    std::vector<uint64_t> empty_resources;
    for (auto& [res_id, requests] : lock_table_) {
        auto rit = std::remove_if(requests.begin(), requests.end(), [txn_id](const LockRequest& r) {
            return r.txn_id == txn_id;
        });
        requests.erase(rit, requests.end());
        if (requests.empty()) {
            empty_resources.push_back(res_id);
        }
    }

    for (uint64_t res_id : empty_resources) {
        lock_table_.erase(res_id);
    }
}

bool ConcurrencyControl::detect_cycle_dfs(
    txn_id_t curr, std::unordered_set<txn_id_t>& visited, std::unordered_set<txn_id_t>& stack, std::vector<txn_id_t>& cycle) {

    visited.insert(curr);
    stack.insert(curr);
    cycle.push_back(curr);

    auto it = wait_for_graph_.find(curr);
    if (it != wait_for_graph_.end()) {
        for (txn_id_t neighbor : it->second) {
            if (stack.count(neighbor) > 0) {
                cycle.push_back(neighbor);
                return true; // Cycle detected
            }
            if (visited.count(neighbor) == 0) {
                if (detect_cycle_dfs(neighbor, visited, stack, cycle)) {
                    return true;
                }
            }
        }
    }

    cycle.pop_back();
    stack.erase(curr);
    return false;
}

bool ConcurrencyControl::detect_deadlock(std::vector<txn_id_t>& cycle) {
    std::lock_guard<std::mutex> lock(mutex_);
    std::unordered_set<txn_id_t> visited;
    std::unordered_set<txn_id_t> stack;

    for (const auto& [waiter, holders] : wait_for_graph_) {
        if (visited.count(waiter) == 0) {
            if (detect_cycle_dfs(waiter, visited, stack, cycle)) {
                return true;
            }
        }
    }
    return false;
}

} // namespace AetherGraph

#include "lock_manager.h"
#include <algorithm>
#include <mutex>

namespace AetherGraph {

bool LockManager::is_compatible(const LockRequestQueue& queue, LockMode mode, txn_id_t txn_id) const {
    // If the queue is empty, compatible.
    if (queue.request_queue.empty()) return true;

    // Check if the current transaction already holds the lock or is upgrading.
    for (const auto& req : queue.request_queue) {
        if (req.granted && req.txn_id == txn_id) {
            if (mode == LockMode::SHARED) return true;
            if (mode == LockMode::EXCLUSIVE && queue.shared_count == 1) return true;
        }
    }

    if (mode == LockMode::SHARED) {
        return !queue.has_exclusive;
    } else {
        return queue.shared_count == 0 && !queue.has_exclusive;
    }
}

bool LockManager::acquire_shared(txn_id_t txn_id, node_id_t node_id) {
    std::unique_lock<std::mutex> lock(mutex_);
    LockRequestQueue& queue = lock_table_[node_id];

    // Check if already granted
    for (auto& req : queue.request_queue) {
        if (req.txn_id == txn_id && req.granted) {
            return true;
        }
    }

    LockRequest req(txn_id, LockMode::SHARED);
    queue.request_queue.push_back(req);

    auto& req_ref = queue.request_queue.back();

    queue.cv.wait(lock, [&]() {
        return is_compatible(queue, LockMode::SHARED, txn_id) && 
               (&queue.request_queue.front() == &req_ref || !queue.has_exclusive);
    });

    req_ref.granted = true;
    queue.shared_count++;
    return true;
}

bool LockManager::acquire_exclusive(txn_id_t txn_id, node_id_t node_id) {
    std::unique_lock<std::mutex> lock(mutex_);
    LockRequestQueue& queue = lock_table_[node_id];

    // Check if already granted
    for (auto& req : queue.request_queue) {
        if (req.txn_id == txn_id && req.granted) {
            if (req.lock_mode == LockMode::EXCLUSIVE) return true;
            // Upgrade lock
            if (queue.shared_count == 1) {
                req.lock_mode = LockMode::EXCLUSIVE;
                queue.shared_count--;
                queue.has_exclusive = true;
                return true;
            }
        }
    }

    LockRequest req(txn_id, LockMode::EXCLUSIVE);
    queue.request_queue.push_back(req);

    auto& req_ref = queue.request_queue.back();

    queue.cv.wait(lock, [&]() {
        return is_compatible(queue, LockMode::EXCLUSIVE, txn_id) && 
               (&queue.request_queue.front() == &req_ref);
    });

    req_ref.granted = true;
    queue.has_exclusive = true;
    return true;
}

bool LockManager::release(txn_id_t txn_id, node_id_t node_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = lock_table_.find(node_id);
    if (it == lock_table_.end()) return false;

    LockRequestQueue& queue = it->second;
    auto req_it = std::find_if(queue.request_queue.begin(), queue.request_queue.end(),
                               [txn_id](const LockRequest& r) { return r.txn_id == txn_id; });

    if (req_it != queue.request_queue.end()) {
        if (req_it->granted) {
            if (req_it->lock_mode == LockMode::SHARED) {
                queue.shared_count--;
            } else {
                queue.has_exclusive = false;
            }
        }
        queue.request_queue.erase(req_it);
        queue.cv.notify_all();
        
        // Erase queue if no active locks are held to save memory.
        // BUG: If there are still pending/waiting requests in the queue (e.g. waiting for exclusive lock),
        // erasing the queue from the map destroys the condition variable and the queue itself.
        // The waiting threads will wake up and access the deleted queue, causing a Use-After-Free.
        if (queue.shared_count == 0 && !queue.has_exclusive) {
            lock_table_.erase(it);
        }
        return true;
    }
    return false;
}

} // namespace AetherGraph

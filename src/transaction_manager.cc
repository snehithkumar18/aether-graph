#include "transaction_manager.h"
#include <algorithm>
#include <mutex>

namespace AetherGraph {

TransactionManager::~TransactionManager() {
    clear();
}

Transaction* TransactionManager::begin_transaction() {
    std::lock_guard<std::mutex> lock(mutex_);
    txn_id_t tid = next_txn_id_++;
    Transaction* txn = new Transaction(tid, global_ts_);
    txn_table_[tid] = txn;
    return txn;
}

bool TransactionManager::commit(Transaction* txn) {
    std::lock_guard<std::mutex> lock(mutex_);
    txn->set_state(TransactionState::COMMITTED);
    txn->set_commit_ts(++global_ts_);
    return true;
}

void TransactionManager::abort(Transaction* txn, GraphEngine& graph_engine) {
    std::lock_guard<std::mutex> lock(mutex_);
    txn->set_state(TransactionState::ABORTED);

    // Walk the undo log in reverse to restore previous state
    const auto& undo_log = txn->get_undo_log();
    for (auto it = undo_log.rbegin(); it != undo_log.rend(); ++it) {
        const UndoRecord& record = *it;
        Node* node = graph_engine.get_node(record.node_id);
        if (node) {
            if (record.is_delete) {
                // Undo a property creation: remove the property
                node->properties.erase(record.key);
            } else {
                // Undo a property update: restore old value
                node->properties[record.key] = record.old_value;
            }
        }
    }
}

void TransactionManager::clear() {
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto& pair : txn_table_) {
        delete pair.second;
    }
    txn_table_.clear();
    next_txn_id_ = 1;
    global_ts_ = 1;
}

} // namespace AetherGraph

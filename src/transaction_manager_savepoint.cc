#include "transaction_manager_savepoint.h"
#include <algorithm>
#include <string>

namespace AetherGraph {

void TransactionManagerSavepoint::create_savepoint(
    txn_id_t txn_id, const std::string& name, const Transaction& txn) {
    Savepoint sp;
    sp.name = name;
    sp.undo_log_index = txn.get_undo_log().size();
    savepoints_[txn_id].push_back(sp);
}

bool TransactionManagerSavepoint::rollback_to_savepoint(
    txn_id_t txn_id, const std::string& name, Transaction& txn, GraphEngine& ge) {
    auto it = savepoints_.find(txn_id);
    if (it == savepoints_.end()) return false;

    auto sp_it = std::find_if(it->second.begin(), it->second.end(), [&](const Savepoint& s) {
        return s.name == name;
    });

    if (sp_it == it->second.end()) return false;

    size_t target_idx = sp_it->undo_log_index;
    auto& undo_log = txn.get_undo_log_mut();
    while (undo_log.size() > target_idx) {
        auto record = undo_log.back();
        undo_log.pop_back();

        if (record.is_delete) {
            Node* node = ge.get_node(record.node_id);
            if (node) {
                node->properties.erase(record.key);
            }
        } else {
            Node* node = ge.get_node(record.node_id);
            if (node) {
                node->properties[record.key] = record.old_value;
            }
        }
    }

    return true;
}

void TransactionManagerSavepoint::release_savepoint(txn_id_t txn_id, const std::string& name) {
    auto it = savepoints_.find(txn_id);
    if (it != savepoints_.end()) {
        it->second.erase(std::remove_if(it->second.begin(), it->second.end(), [&](const Savepoint& s) {
            return s.name == name;
        }), it->second.end());
    }
}

} // namespace AetherGraph

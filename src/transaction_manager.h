#ifndef AETHER_GRAPH_TRANSACTION_MANAGER_H
#define AETHER_GRAPH_TRANSACTION_MANAGER_H

#include "utils.h"
#include "graph_engine.h"
#include <vector>
#include <unordered_map>
#include <mutex>

namespace AetherGraph {

enum class TransactionState {
    ACTIVE,
    COMMITTED,
    ABORTED
};

struct UndoRecord {
    node_id_t node_id;
    std::string key;
    Variant old_value;
    bool is_delete;

    UndoRecord(node_id_t nid, const std::string& k, const Variant& val, bool is_del)
        : node_id(nid), key(k), old_value(val), is_delete(is_del) {}
};

class Transaction {
private:
    txn_id_t txn_id_;
    TransactionState state_;
    uint64_t read_ts_;
    uint64_t commit_ts_;
    std::vector<UndoRecord> undo_log_;

public:
    Transaction(txn_id_t tid, uint64_t read_ts)
        : txn_id_(tid), state_(TransactionState::ACTIVE), read_ts_(read_ts), commit_ts_(0) {}

    txn_id_t get_txn_id() const { return txn_id_; }
    TransactionState get_state() const { return state_; }
    void set_state(TransactionState state) { state_ = state; }

    uint64_t get_read_ts() const { return read_ts_; }
    uint64_t get_commit_ts() const { return commit_ts_; }
    void set_commit_ts(uint64_t ts) { commit_ts_ = ts; }

    void append_undo(const UndoRecord& record) { undo_log_.push_back(record); }
    const std::vector<UndoRecord>& get_undo_log() const { return undo_log_; }
};

class TransactionManager {
private:
    txn_id_t next_txn_id_ = 1;
    uint64_t global_ts_ = 1;
    std::unordered_map<txn_id_t, Transaction*> txn_table_;
    std::mutex mutex_;

public:
    TransactionManager() = default;
    ~TransactionManager();

    Transaction* begin_transaction();
    bool commit(Transaction* txn);
    void abort(Transaction* txn, GraphEngine& graph_engine);

    void clear();
};

} // namespace AetherGraph

#endif // AETHER_GRAPH_TRANSACTION_MANAGER_H

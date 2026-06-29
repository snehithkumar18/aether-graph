#ifndef AETHER_GRAPH_TRANSACTION_MANAGER_SAVEPOINT_H
#define AETHER_GRAPH_TRANSACTION_MANAGER_SAVEPOINT_H

#include "transaction_manager.h"
#include <string>
#include <vector>
#include <unordered_map>

namespace AetherGraph {

struct Savepoint {
    std::string name;
    size_t undo_log_index;
};

class TransactionManagerSavepoint {
private:
    std::unordered_map<txn_id_t, std::vector<Savepoint>> savepoints_;

public:
    TransactionManagerSavepoint() = default;
    ~TransactionManagerSavepoint() = default;

    void create_savepoint(txn_id_t txn_id, const std::string& name, const Transaction& txn);
    bool rollback_to_savepoint(txn_id_t txn_id, const std::string& name, Transaction& txn, GraphEngine& ge);
    void release_savepoint(txn_id_t txn_id, const std::string& name);
};

} // namespace AetherGraph

#endif // AETHER_GRAPH_TRANSACTION_MANAGER_SAVEPOINT_H

#ifndef AETHER_GRAPH_DISTRIBUTED_TXN_H
#define AETHER_GRAPH_DISTRIBUTED_TXN_H

#include "graph_engine.h"
#include "transaction_manager.h"
#include <string>
#include <vector>
#include <unordered_map>

namespace AetherGraph {

enum class TwoPhaseCommitStatus {
    PREPARED,
    COMMITTED,
    ABORTED,
    UNKNOWN
};

struct ParticipantState {
    uint32_t participant_node_id;
    TwoPhaseCommitStatus status;
};

class DistributedTransactionCoordinator {
private:
    txn_id_t global_txn_id_;
    std::vector<ParticipantState> participants_;

public:
    explicit DistributedTransactionCoordinator(txn_id_t global_txn_id);
    ~DistributedTransactionCoordinator() = default;

    void register_participant(uint32_t node_id);
    bool execute_two_phase_commit();
};

class DistributedTransactionParticipant {
private:
    uint32_t node_id_;
    GraphEngine& ge_;
    TransactionManager& tm_;
    std::unordered_map<txn_id_t, Transaction*> active_txns_;

public:
    DistributedTransactionParticipant(uint32_t node_id, GraphEngine& ge, TransactionManager& tm);
    ~DistributedTransactionParticipant() = default;

    bool prepare(txn_id_t global_txn_id);
    void commit(txn_id_t global_txn_id);
    void abort(txn_id_t global_txn_id);
};

} // namespace AetherGraph

#endif // AETHER_GRAPH_DISTRIBUTED_TXN_H

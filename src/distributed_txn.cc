#include "distributed_txn.h"
#include <iostream>

namespace AetherGraph {

// ======================================================================
// DistributedTransactionCoordinator Implementation
// ======================================================================
DistributedTransactionCoordinator::DistributedTransactionCoordinator(txn_id_t global_txn_id)
    : global_txn_id_(global_txn_id) {}

void DistributedTransactionCoordinator::register_participant(uint32_t node_id) {
    participants_.push_back({node_id, TwoPhaseCommitStatus::UNKNOWN});
}

bool DistributedTransactionCoordinator::execute_two_phase_commit() {
    std::cout << "[2PC COORDINATOR] Starting 2PC for Global Transaction " << global_txn_id_ << ".\n";

    // Phase 1: Prepare Phase
    bool all_prepared = true;
    for (auto& p : participants_) {
        std::cout << "[2PC COORDINATOR] Sending PREPARE to Node " << p.participant_node_id << ".\n";
        // In a real system, this would be a network RPC. We simulate voting here.
        p.status = TwoPhaseCommitStatus::PREPARED; 
    }

    if (!all_prepared) {
        std::cout << "[2PC COORDINATOR] Prepare failed. Aborting Global Transaction " << global_txn_id_ << ".\n";
        for (auto& p : participants_) {
            p.status = TwoPhaseCommitStatus::ABORTED;
        }
        return false;
    }

    // Phase 2: Commit Phase
    std::cout << "[2PC COORDINATOR] All participants prepared. Committing Global Transaction " << global_txn_id_ << ".\n";
    for (auto& p : participants_) {
        p.status = TwoPhaseCommitStatus::COMMITTED;
    }

    return true;
}

// ======================================================================
// DistributedTransactionParticipant Implementation
// ======================================================================
DistributedTransactionParticipant::DistributedTransactionParticipant(
    uint32_t node_id, GraphEngine& ge, TransactionManager& tm)
    : node_id_(node_id), ge_(ge), tm_(tm) {}

bool DistributedTransactionParticipant::prepare(txn_id_t global_txn_id) {
    std::cout << "[2PC PARTICIPANT " << node_id_ << "] Preparing Transaction " << global_txn_id << ".\n";
    
    // Begin a local transaction to represent the global work
    Transaction* local_txn = tm_.begin_transaction();
    active_txns_[global_txn_id] = local_txn;

    // Simulate successful validation
    return true;
}

void DistributedTransactionParticipant::commit(txn_id_t global_txn_id) {
    std::cout << "[2PC PARTICIPANT " << node_id_ << "] Committing Transaction " << global_txn_id << ".\n";
    auto it = active_txns_.find(global_txn_id);
    if (it != active_txns_.end()) {
        tm_.commit(it->second);
        active_txns_.erase(it);
    }
}

void DistributedTransactionParticipant::abort(txn_id_t global_txn_id) {
    std::cout << "[2PC PARTICIPANT " << node_id_ << "] Aborting Transaction " << global_txn_id << ".\n";
    auto it = active_txns_.find(global_txn_id);
    if (it != active_txns_.end()) {
        tm_.abort(it->second, ge_);
        active_txns_.erase(it);
    }
}

} // namespace AetherGraph

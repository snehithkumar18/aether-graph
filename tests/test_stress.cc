#include "graph_engine.h"
#include "transaction_manager.h"
#include "concurrency_control.h"
#include <thread>
#include <vector>
#include <atomic>
#include <random>
#include <iostream>
#include <cassert>

static void transaction_worker_thread(
    GraphEngine& ge, TransactionManager& tm, ConcurrencyControl& cc,
    std::atomic<bool>& stop_flag, std::atomic<size_t>& tx_count, std::atomic<size_t>& abort_count,
    size_t thread_id) {

    std::mt19937 rng(12345 + thread_id);
    std::uniform_int_distribution<int> op_dist(0, 3);
    std::uniform_int_distribution<int> val_dist(0, 100);

    // Keep thread local list of nodes created
    std::vector<node_id_t> local_nodes;

    while (!stop_flag.load()) {
        Transaction* txn = tm.begin_transaction();
        txn_id_t tid = txn->get_txn_id();
        bool success = true;

        int op = op_dist(rng);
        if (op == 0) {
            // Create Node & Lock
            if (cc.acquire_lock(tid, tid + 1000000, LockMode::EXCLUSIVE)) {
                Node* n = ge.create_node("Node_" + std::to_string(thread_id));
                local_nodes.push_back(n->id);
                txn->append_undo(UndoRecord(n->id, "dummy", Variant(), true));
                cc.release_lock(tid, tid + 1000000);
            } else {
                success = false;
            }
        } else if (op == 1 && !local_nodes.empty()) {
            // Update node property & lock
            std::uniform_int_distribution<size_t> idx_dist(0, local_nodes.size() - 1);
            node_id_t nid = local_nodes[idx_dist(rng)];

            if (cc.acquire_lock(tid, nid, LockMode::EXCLUSIVE)) {
                Node* n = ge.get_node(nid);
                if (n) {
                    Variant old;
                    auto it = n->properties.find("value");
                    if (it != n->properties.end()) old = it->second;
                    txn->append_undo(UndoRecord(nid, "value", old, false));
                    ge.update_property(nid, "value", Variant(val_dist(rng)), tid);
                }
                cc.release_lock(tid, nid);
            } else {
                success = false;
            }
        } else if (op == 2 && local_nodes.size() >= 2) {
            // Create Edge
            std::uniform_int_distribution<size_t> idx_dist(0, local_nodes.size() - 1);
            node_id_t src = local_nodes[idx_dist(rng)];
            node_id_t dest = local_nodes[idx_dist(rng)];
            if (src != dest) {
                if (cc.acquire_lock(tid, src, LockMode::SHARED) && cc.acquire_lock(tid, dest, LockMode::SHARED)) {
                    ge.create_edge(src, dest, "STRESS_LINK");
                    cc.release_lock(tid, src);
                    cc.release_lock(tid, dest);
                } else {
                    success = false;
                }
            }
        } else {
            // Abort transaction randomly
            success = false;
        }

        if (success) {
            tm.commit(txn);
            tx_count++;
        } else {
            tm.abort(txn, ge);
            cc.release_all_locks(tid);
            abort_count++;
        }

        // Slight yield to avoid spinning too hot
        std::this_thread::yield();
    }
}

void run_transaction_stress_tests() {
    std::cout << "--------------------------------------------------\n";
    std::cout << "          STARTING TRANSACTION STRESS TESTS       \n";
    std::cout << "--------------------------------------------------\n";

    DiskManager disk_mgr("stress.db");
    BufferPoolManager bpm(30, disk_mgr);
    GraphEngine ge(bpm);
    TransactionManager tm;
    ConcurrencyControl cc;

    std::atomic<bool> stop_flag(false);
    std::atomic<size_t> tx_count(0);
    std::atomic<size_t> abort_count(0);

    size_t num_threads = 4;
    std::vector<std::thread> threads;
    threads.reserve(num_threads);

    for (size_t i = 0; i < num_threads; ++i) {
        threads.push_back(std::thread(
            transaction_worker_thread, std::ref(ge), std::ref(tm), std::ref(cc),
            std::ref(stop_flag), std::ref(tx_count), std::ref(abort_count), i
        ));
    }

    // Run for 1.5 seconds
    std::this_thread::sleep_for(std::chrono::milliseconds(1500));
    stop_flag.store(true);

    for (auto& th : threads) {
        if (th.joinable()) th.join();
    }

    std::cout << "[STRESS] Completed. Committed transactions: " << tx_count.load()
              << ", Aborted transactions: " << abort_count.load() << "\n";
    
    std::remove("stress.db");
    std::cout << "--------------------------------------------------\n";
}

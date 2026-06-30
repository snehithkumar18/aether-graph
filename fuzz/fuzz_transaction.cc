#include "query_executor.h"
#include "query_parser.h"
#include "vector_index.h"
#include "graph_algorithms.h"
#include <cstdint>
#include <cstddef>
#include <cstdio>
#include <string>
#include <vector>

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    if (size < 8) return 0;

    AetherGraph::DiskManager disk_mgr("fuzz_txn.db");
    AetherGraph::BufferPoolManager cache_mgr(3, disk_mgr);
    AetherGraph::GraphEngine graph_engine(cache_mgr);
    AetherGraph::TransactionManager txn_mgr;
    AetherGraph::QueryExecutor executor(graph_engine, txn_mgr);

    // Build an initial graph with cycle potential
    AetherGraph::Node* base = graph_engine.create_node("Entity");
    std::vector<AetherGraph::Node*> nodes;
    nodes.push_back(base);

    size_t offset = 0;
    std::vector<AetherGraph::Transaction*> active_txns;
    AetherGraph::Page* active_pages[4] = {nullptr, nullptr, nullptr, nullptr};
    size_t active_pages_cursor = 0;

    while (offset < size) {
        uint8_t action = data[offset++] % 11;

        if (action == 0) {
            // Begin transaction
            active_txns.push_back(txn_mgr.begin_transaction());
        } else if (action == 1 && !active_txns.empty()) {
            // Commit transaction
            if (offset >= size) break;
            size_t idx = data[offset++] % active_txns.size();
            txn_mgr.commit(active_txns[idx]);
            active_txns.erase(active_txns.begin() + static_cast<ptrdiff_t>(idx));
        } else if (action == 2) {
            // Create node with label derived from input
            std::string label = (offset < size && data[offset] % 2 == 0) ? "Entity" : "Resource";
            if (offset < size) offset++;
            AetherGraph::Node* node = graph_engine.create_node(label);
            nodes.push_back(node);
        } else if (action == 3 && nodes.size() >= 2) {
            // Create edge (potentially forming cycle)
            if (offset + 1 >= size) break;
            size_t src_idx = data[offset++] % nodes.size();
            size_t dst_idx = data[offset++] % nodes.size();
            if (src_idx != dst_idx) {
                graph_engine.create_edge(nodes[src_idx]->id, nodes[dst_idx]->id, "RELATES_TO");
            }
        } else if (action == 4 && !active_txns.empty()) {
            // Update property on a node (drives MVCC version chain growth)
            if (offset + 1 >= size) break;
            AetherGraph::Transaction* txn = active_txns.back();
            size_t nidx = data[offset++] % nodes.size();
            int32_t val = static_cast<int32_t>(data[offset++]);
            AetherGraph::Variant old_val;
            auto pit = nodes[nidx]->properties.find("score");
            if (pit != nodes[nidx]->properties.end()) {
                old_val = pit->second;
            }
            txn->append_undo(AetherGraph::UndoRecord(nodes[nidx]->id, "score", old_val, false));
            graph_engine.update_property(nodes[nidx]->id, "score",
                                          AetherGraph::Variant(val), txn->get_txn_id());
        } else if (action == 5 && !active_txns.empty()) {
            // Execute MATCH_NODE query with property lookup
            AetherGraph::Transaction* txn = active_txns.back();
            AetherGraph::ParsedQuery q;
            q.type = AetherGraph::QueryType::MATCH_NODE;
            if (offset < size) {
                q.node_id = data[offset++] % (nodes.size() + 1);
            } else {
                break;
            }
            q.has_property = true;
            q.property_key = "score";
            q.property_value = AetherGraph::Variant(std::string("42"));
            std::vector<AetherGraph::Node*> results;
            executor.execute(txn, q, results);
        } else if (action == 6 && nodes.size() >= 2) {
            // Execute MATCH_PATH query with label traversal
            if (!active_txns.empty()) {
                AetherGraph::Transaction* txn = active_txns.back();
                AetherGraph::ParsedQuery q;
                q.type = AetherGraph::QueryType::MATCH_PATH;
                q.node_label = "Entity";
                if (offset + 1 >= size) break;
                q.src_id = nodes[data[offset++] % nodes.size()]->id;
                q.dest_id = nodes[data[offset++] % nodes.size()]->id;
                std::vector<AetherGraph::Node*> results;
                executor.execute(txn, q, results);
            }
        } else if (action == 7 && nodes.size() >= 2) {
            // Delete a node from the graph
            if (offset >= size) break;
            size_t del_idx = data[offset++] % nodes.size();
            graph_engine.delete_node(nodes[del_idx]->id);
            nodes.erase(nodes.begin() + static_cast<ptrdiff_t>(del_idx));
        } else if (action == 8 && !active_txns.empty()) {
            // Abort transaction
            if (offset >= size) break;
            size_t idx = data[offset++] % active_txns.size();
            txn_mgr.abort(active_txns[idx], graph_engine);
            active_txns.erase(active_txns.begin() + static_cast<ptrdiff_t>(idx));
        } else if (action == 9) {
            // Fetch page from cache_mgr
            if (offset < size) {
                uint32_t pid = data[offset++] % 8;
                AetherGraph::Page* p = cache_mgr.fetch_page(pid);
                if (p) {
                    active_pages[active_pages_cursor] = p;
                    active_pages_cursor = (active_pages_cursor + 1) % 4;
                }
            }
        } else if (action == 10) {
            // Access page from active_pages (triggers UAF if page was evicted)
            if (offset < size) {
                size_t pidx = data[offset++] % 4;
                if (active_pages[pidx]) {
                    // Try to write to the page data
                    active_pages[pidx]->data[0] = static_cast<uint8_t>(data[offset - 1]);
                }
            }
        }

        if (offset >= size) break;
    }

    // ---- Vector Index fuzzing for stale cluster indices ----
    if (size >= 20) {
        AetherGraph::VectorIndex vidx("embedding", 3);
        for (size_t i = 0; i < std::min(size_t(10), size / 3); ++i) {
            std::vector<float> vec = {1.0f, 2.0f, 3.0f};
            vidx.add_vector(i, vec);
        }
        vidx.build_index(4);
        // Rebuild with different cluster count to create stale indices
        vidx.build_index(2);
        std::vector<float> query = {1.0f, 2.0f, 3.0f};
        vidx.search_knn(query, 5, "l2");
    }

    // ---- Graph Algorithms fuzzing for Dijkstra bounds check ----
    if (size >= 15 && nodes.size() >= 2) {
        AetherGraph::node_id_t start = nodes[0]->id;
        AetherGraph::node_id_t end = nodes[1]->id;
        AetherGraph::GraphAlgorithms::dijkstra(graph_engine, start, end, "weight");
    }

    // Cleanup
    for (auto* txn : active_txns) {
        txn_mgr.commit(txn);
    }

    txn_mgr.clear();
    graph_engine.clear();
    cache_mgr.clear();
    std::remove("fuzz_txn.db");
    return 0;
}

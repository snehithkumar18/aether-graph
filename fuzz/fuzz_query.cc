#include "query_executor.h"
#include "query_parser.h"
#include "query_compiler_jit.h"
#include "wal_manager.h"
#include "storage_compressor.h"
#include "graph_serializer.h"
#include "disk_storage.h"
#include "property_index.h"
#include "graph_analytics.h"
#include "cypher_parser.h"
#include <cstdint>
#include <cstddef>
#include <cstdio>
#include <string>
#include <vector>
#include <algorithm>
#include <fstream>

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    if (size < 10) return 0;

    AetherGraph::DiskManager disk_mgr("fuzz_query.db");
    AetherGraph::BufferPoolManager cache_mgr(10, disk_mgr);
    AetherGraph::GraphEngine graph_engine(cache_mgr);
    AetherGraph::TransactionManager txn_mgr;
    AetherGraph::QueryExecutor executor(graph_engine, txn_mgr);

    // Build a graph with diverse property types
    AetherGraph::Node* n1 = graph_engine.create_node("User");
    AetherGraph::Node* n2 = graph_engine.create_node("User");
    AetherGraph::Node* n3 = graph_engine.create_node("User");
    graph_engine.create_edge(n1->id, n2->id, "KNOWS");
    graph_engine.create_edge(n2->id, n3->id, "KNOWS");
    graph_engine.create_edge(n3->id, n1->id, "KNOWS"); // Cycle for DFS

    // Set up versioned properties
    AetherGraph::Transaction* setup_txn = txn_mgr.begin_transaction();
    graph_engine.update_property(n1->id, "age", AetherGraph::Variant(25), setup_txn->get_txn_id());
    graph_engine.update_property(n2->id, "age", AetherGraph::Variant(30), setup_txn->get_txn_id());
    graph_engine.update_property(n3->id, "name", AetherGraph::Variant(std::string("Alice")),
                                  setup_txn->get_txn_id());
    txn_mgr.commit(setup_txn);

    // ---- Deserialization + property storage for type confusion coverage ----
    // Deserialize a variant from fuzz input and store it as a node property.
    size_t deser_offset = 0;
    AetherGraph::Variant deserialized = graph_engine.deserialize_variant(data, size, deser_offset);
    if (deserialized.type != AetherGraph::DataType::NIL) {
        // Store the deserialized value as a property on a node
        AetherGraph::Transaction* deser_txn = txn_mgr.begin_transaction();
        n1->properties["fuzz_prop"] = deserialized;
        graph_engine.update_property(n1->id, "fuzz_prop", deserialized, deser_txn->get_txn_id());

        // Query the property using the same type as what was deserialized
        AetherGraph::ParsedQuery q;
        q.type = AetherGraph::QueryType::MATCH_NODE;
        q.node_id = n1->id;
        q.has_property = true;
        q.property_key = "fuzz_prop";
        q.property_value = AetherGraph::Variant(std::string("test"));
        std::vector<AetherGraph::Node*> results;
        executor.execute(deser_txn, q, results);
        txn_mgr.commit(deser_txn);
    }

    // ---- Parse remaining data as query string ----
    size_t remaining = size - std::min(deser_offset, size);
    if (remaining > 0) {
        const uint8_t* query_data = data + deser_offset;
        std::string query_str(reinterpret_cast<const char*>(query_data), remaining);

        query_str.erase(std::remove_if(query_str.begin(), query_str.end(), [](unsigned char c) {
            return c < 32 || c > 126;
        }), query_str.end());

        if (!query_str.empty()) {
            AetherGraph::Transaction* txn = txn_mgr.begin_transaction();
            AetherGraph::ParsedQuery query = AetherGraph::QueryParser::parse(query_str);
            std::vector<AetherGraph::Node*> results;
            executor.execute(txn, query, results);
            txn_mgr.commit(txn);
        }
    }

    // ---- BFS stress test: star graph to trigger queue resize ----
    // Create a hub node with many outgoing edges so BFS enqueues many nodes
    AetherGraph::Node* hub = graph_engine.create_node("Hub");
    size_t spoke_count = std::min(size, static_cast<size_t>(150));
    std::vector<AetherGraph::Node*> spokes;
    for (size_t i = 0; i < spoke_count; ++i) {
        AetherGraph::Node* spoke = graph_engine.create_node("Spoke");
        spokes.push_back(spoke);
        graph_engine.create_edge(hub->id, spoke->id, "LINK");
    }

    if (!spokes.empty()) {
        AetherGraph::Transaction* bfs_txn = txn_mgr.begin_transaction();
        AetherGraph::ParsedQuery bfs_query;
        bfs_query.type = AetherGraph::QueryType::MATCH_PATH;
        bfs_query.src_id = hub->id;
        bfs_query.dest_id = spokes.back()->id;
        std::vector<AetherGraph::Node*> bfs_results;
        executor.execute(bfs_txn, bfs_query, bfs_results);
        txn_mgr.commit(bfs_txn);
    }

    // ---- JIT Compiler fuzzing ----
    if (size >= 15) {
        AetherGraph::QueryCompilerJIT jit;
        jit.compile_operator(graph_engine, nullptr);
        
        if (data[0] % 2 == 0) {
            AetherGraph::Transaction* jit_txn = txn_mgr.begin_transaction();
            graph_engine.update_property(n1->id, "age", AetherGraph::Variant(static_cast<int32_t>(data[1])), jit_txn->get_txn_id());
            txn_mgr.commit(jit_txn);
        } else {
            graph_engine.delete_node(n1->id);
        }
        
        std::vector<AetherGraph::Node*> jit_results;
        jit.execute(graph_engine, jit_results);
    }

    // ---- Slotted Page fuzzing ----
    if (size >= 20) {
        AetherGraph::SlottedPage sp(888);
        size_t offset_sp = 0;
        while (offset_sp + 4 < size) {
            uint16_t rec_len = data[offset_sp++] % 150;
            std::vector<uint8_t> rec_data(rec_len, 0xAA);
            sp.insert_record(rec_data);
            if (data[offset_sp++] % 4 == 0) {
                sp.delete_record(0);
            }
            if (data[offset_sp++] % 5 == 0) {
                sp.compact();
            }
        }
    }

    // ---- WAL recovery fuzzing ----
    if (size >= 15) {
        std::string log_name = "fuzz_wal.log";
        {
            std::ofstream out(log_name, std::ios::binary | std::ios::trunc);
            if (out) {
                out.write(reinterpret_cast<const char*>(data), size);
            }
        }
        AetherGraph::WalManager wal(log_name);
        AetherGraph::GraphEngine wal_ge(cache_mgr);
        wal.recover(wal_ge);
        std::remove(log_name.c_str());
    }

    // ---- Storage Compressor fuzzing ----
    if (size >= 5) {
        AetherGraph::StorageCompressor compressor;
        std::vector<uint8_t> comp_input(data, data + size);
        std::vector<uint8_t> compressed = compressor.compress(comp_input);
        compressor.decompress(compressed);
    }

    // ---- Binary Deserialization fuzzing ----
    if (size >= 12) {
        AetherGraph::GraphEngine bin_ge(cache_mgr);
        std::vector<uint8_t> bin_input(data, data + size);
        AetherGraph::GraphSerializer::import_from_binary(bin_ge, bin_input);
    }

    // ---- Property Index fuzzing for B-tree bounds check ----
    if (size >= 10) {
        AetherGraph::PropertyIndex pidx("test_key", 2);
        for (size_t i = 0; i < std::min(size_t(20), size / 2); ++i) {
            std::string key = "key" + std::to_string(i);
            pidx.insert(key, i);
        }
        pidx.search("key5");
    }

    // ---- Graph Analytics fuzzing for histogram overflow ----
    if (size >= 8) {
        AetherGraph::Node* high_deg_node = graph_engine.create_node("HighDeg");
        uint32_t edge_count = data[0] * 1000;
        for (uint32_t i = 0; i < std::min(edge_count, uint32_t(10000)); ++i) {
            AetherGraph::Node* temp = graph_engine.create_node("Temp");
            graph_engine.create_edge(high_deg_node->id, temp->id, "TEMP");
        }
        AetherGraph::GraphAnalytics::compute_degree_distribution(graph_engine);
    }

    // ---- Cypher Parser fuzzing for escape sequences ----
    if (size >= 5) {
        std::string cypher_query = "MATCH (n:Label) WHERE n.name = \"";
        for (size_t i = 0; i < std::min(size_t(10), size); ++i) {
            cypher_query += static_cast<char>(data[i]);
        }
        cypher_query += "\" RETURN n";
        AetherGraph::CypherParser::parse(cypher_query);
    }

    txn_mgr.clear();
    graph_engine.clear();
    cache_mgr.clear();
    std::remove("fuzz_query.db");
    return 0;
}

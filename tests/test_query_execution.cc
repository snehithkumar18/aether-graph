#include "query_executor.h"
#include "graph_statistics.h"
#include "query_optimizer.h"
#include "vector_index.h"
#include "execution_plans.h"
#include <cassert>
#include <iostream>

void run_query_execution_tests() {
    AetherGraph::DiskManager disk_mgr("test_qe.db");
    AetherGraph::BufferPoolManager bpm(10, disk_mgr);
    AetherGraph::GraphEngine ge(bpm);
    AetherGraph::TransactionManager tm;
    AetherGraph::QueryExecutor exec(ge, tm);

    // 1. Setup Data
    AetherGraph::Transaction* txn = tm.begin_transaction();
    
    Node* n1 = ge.create_node("User");
    ge.update_property(n1->id, "age", AetherGraph::Variant(25), txn->get_txn_id());
    
    Node* n2 = ge.create_node("User");
    ge.update_property(n2->id, "age", AetherGraph::Variant(30), txn->get_txn_id());
    
    ge.create_edge(n1->id, n2->id, "KNOWS");
    tm.commit(txn);

    // 2. Test execution of MATCH query
    AetherGraph::Transaction* query_txn = tm.begin_transaction();
    AetherGraph::ParsedQuery query;
    query.type = AetherGraph::QueryType::MATCH_NODE;
    query.node_label = "User";
    query.has_property = true;
    query.property_key = "age";
    query.property_value = AetherGraph::Variant(25);

    std::vector<Node*> results;
    exec.execute(query_txn, query, results);
    assert(results.size() == 1);
    assert(results[0]->id == n1->id);

    tm.commit(query_txn);

    // 3. Test Graph Statistics & Selectivity
    AetherGraph::GraphStatistics stats;
    stats.analyze(ge);
    
    double sel = stats.estimate_node_selectivity("User", "age", "=", AetherGraph::Variant(25));
    assert(sel > 0.0 && sel <= 1.0);

    // 4. Test Optimizer Cost Plan
    AetherGraph::QueryOptimizer optimizer(stats);
    auto plan = optimizer.generate_optimized_plan(query);
    assert(plan != nullptr);
    assert(plan->cost > 0.0);

    // 5. Test Physical Plan Operators
    auto seq_scan = std::make_unique<AetherGraph::PhysicalSeqScan>(ge, "User");
    auto filter = std::make_unique<AetherGraph::PhysicalFilter>(
        std::move(seq_scan), "age", "=", AetherGraph::Variant(25));

    filter->open();
    Node* res = filter->next();
    assert(res != nullptr);
    assert(res->id == n1->id);
    Node* end_res = filter->next();
    assert(end_res == nullptr); // Iterator exhausted
    filter->close();

    // 6. Test Vector Index KNN Search
    ge.clear();
    Node* v1 = ge.create_node("Image");
    v1->properties["embedding"] = AetherGraph::Variant(std::vector<float>{1.0f, 0.0f, 0.0f});
    
    Node* v2 = ge.create_node("Image");
    v2->properties["embedding"] = AetherGraph::Variant(std::vector<float>{0.0f, 1.0f, 0.0f});

    AetherGraph::VectorIndex vec_index("embedding", 3);
    vec_index.build_from_graph(ge);

    auto knn = vec_index.search_knn(std::vector<float>{0.9f, 0.1f, 0.0f}, 1, "l2");
    assert(!knn.empty());
    assert(knn[0].node_id == v1->id); // closest to vector 1

    std::remove("test_qe.db");
}

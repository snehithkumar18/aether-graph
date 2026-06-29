#include "query_engine.h"
#include <cassert>
#include <iostream>

void run_advanced_query_tests() {
    AetherGraph::DiskManager disk_mgr("test_adv.db");
    AetherGraph::BufferPoolManager bpm(10, disk_mgr);
    AetherGraph::GraphEngine ge(bpm);

    // Setup base data nodes for join/aggregates
    Node* n1 = ge.create_node("User");
    n1->properties["dept"] = AetherGraph::Variant(std::string("Sales"));
    n1->properties["salary"] = AetherGraph::Variant(5000);
    n1->properties["name"] = AetherGraph::Variant(std::string("Bob"));

    Node* n2 = ge.create_node("User");
    n2->properties["dept"] = AetherGraph::Variant(std::string("Sales"));
    n2->properties["salary"] = AetherGraph::Variant(7000);
    n2->properties["name"] = AetherGraph::Variant(std::string("Alice"));

    Node* n3 = ge.create_node("User");
    n3->properties["dept"] = AetherGraph::Variant(std::string("Engineering"));
    n3->properties["salary"] = AetherGraph::Variant(9000);
    n3->properties["name"] = AetherGraph::Variant(std::string("Charlie"));

    // 1. Test PhysicalSort
    auto seq = std::make_unique<AetherGraph::PhysicalSeqScan>(ge, "User");
    auto sort = std::make_unique<AetherGraph::PhysicalSort>(std::move(seq), "salary", false); // descending

    sort->open();
    Node* r1 = sort->next();
    assert(r1 != nullptr && r1->id == n3->id); // 9000
    Node* r2 = sort->next();
    assert(r2 != nullptr && r2->id == n2->id); // 7000
    Node* r3 = sort->next();
    assert(r3 != nullptr && r3->id == n1->id); // 5000
    assert(sort->next() == nullptr);
    sort->close();

    // 2. Test PhysicalLimit
    auto seq_limit = std::make_unique<AetherGraph::PhysicalSeqScan>(ge, "User");
    auto limit = std::make_unique<AetherGraph::PhysicalLimit>(std::move(seq_limit), 2);
    limit->open();
    assert(limit->next() != nullptr);
    assert(limit->next() != nullptr);
    assert(limit->next() == nullptr); // Limit of 2 hit
    limit->close();

    // 3. Test PhysicalAggregate (AVG salary grouped by dept)
    auto seq_agg = std::make_unique<AetherGraph::PhysicalSeqScan>(ge, "User");
    auto agg = std::make_unique<AetherGraph::PhysicalAggregate>(std::move(seq_agg), "dept", "salary", "AVG");
    agg->open();
    
    Node* agg1 = agg->next();
    assert(agg1 != nullptr);
    Node* agg2 = agg->next();
    assert(agg2 != nullptr);
    assert(agg->next() == nullptr);

    // Verify avg calculations: Sales should be (5000+7000)/2 = 6000
    std::string dept1 = agg1->properties["dept"].get_string();
    float avg1 = agg1->properties["AVG(salary)"].get_float();
    std::string dept2 = agg2->properties["dept"].get_string();
    float avg2 = agg2->properties["AVG(salary)"].get_float();

    if (dept1 == "Sales") {
        assert(std::abs(avg1 - 6000.0f) < 1e-3);
        assert(dept2 == "Engineering");
        assert(std::abs(avg2 - 9000.0f) < 1e-3);
    } else {
        assert(dept1 == "Engineering");
        assert(std::abs(avg1 - 9000.0f) < 1e-3);
        assert(dept2 == "Sales");
        assert(std::abs(avg2 - 6000.0f) < 1e-3);
    }
    agg->close();

    std::remove("test_adv.db");
}

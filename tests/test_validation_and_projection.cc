#include "schema_validator.h"
#include "graph_projection.h"
#include "object_pool.h"
#include "graph_server.h"
#include <cassert>
#include <iostream>

void run_validation_and_projection_tests() {
    AetherGraph::DiskManager disk_mgr("test_val_proj.db");
    AetherGraph::BufferPoolManager bpm(10, disk_mgr);
    AetherGraph::GraphEngine ge(bpm);
    AetherGraph::TransactionManager tm;
    AetherGraph::ConcurrencyControl cc;

    // 1. Test SchemaValidator
    AetherGraph::SchemaValidator validator;
    validator.add_range_rule("User", "age", 18.0f, 65.0f);
    validator.add_regex_rule("User", "email", "^[a-zA-Z0-9._%+-]+@[a-zA-Z0-9.-]+\\.[a-zA-Z]{2,}$");
    validator.add_not_empty_rule("User", "name");

    Node* n1 = ge.create_node("User");
    n1->properties["age"] = AetherGraph::Variant(25);
    n1->properties["email"] = AetherGraph::Variant(std::string("bob@example.com"));
    n1->properties["name"] = AetherGraph::Variant(std::string("Bob"));

    std::vector<std::string> errors;
    bool ok = validator.validate_node(*n1, errors);
    assert(ok);
    assert(errors.empty());

    // Invalid node properties (age out of bounds)
    n1->properties["age"] = AetherGraph::Variant(10); // < 18
    errors.clear();
    bool invalid_ok = validator.validate_node(*n1, errors);
    assert(!invalid_ok);
    assert(!errors.empty());

    // Restore valid age
    n1->properties["age"] = AetherGraph::Variant(25);

    // 2. Test GraphProjection
    Node* n2 = ge.create_node("User");
    n2->properties["age"] = AetherGraph::Variant(30);
    n2->properties["email"] = AetherGraph::Variant(std::string("alice@example.com"));
    n2->properties["name"] = AetherGraph::Variant(std::string("Alice"));

    ge.create_edge(n1->id, n2->id, "KNOWS");

    AetherGraph::GraphProjection proj(ge);
    proj.project_by_label("User");
    assert(proj.node_count() == 2);
    assert(proj.edge_count() == 1);

    // 3. Test ObjectPool
    AetherGraph::ObjectPool<Node> node_pool(5);
    Node* pool_node1 = node_pool.acquire();
    assert(pool_node1 != nullptr);
    node_pool.release(pool_node1);

    // 4. Test GraphServer query execution stubs
    AetherGraph::GraphServer server(ge, tm, cc);
    uint32_t client_id = server.connect_client();
    assert(client_id >= 1000);

    // Run CREATE query
    std::string create_res = server.execute_query(client_id, "CREATE (n:User)");
    assert(create_res.find("success") != std::string::npos);

    // Run MATCH query
    std::string match_res = server.execute_query(client_id, "MATCH (n:User) WHERE n.name = 'Bob' RETURN n");
    assert(match_res.find("success") != std::string::npos);

    server.disconnect_client(client_id);
    std::remove("test_val_proj.db");
}

#include "graph_serializer.h"
#include <cassert>
#include <iostream>

void run_graph_serializer_tests() {
    AetherGraph::DiskManager disk_mgr("test_ser.db");
    AetherGraph::BufferPoolManager bpm(10, disk_mgr);
    AetherGraph::GraphEngine ge(bpm);

    // Setup base nodes and edge with property variants
    auto* n1 = ge.create_node("User");
    n1->properties["age"] = AetherGraph::Variant(35);
    n1->properties["score"] = AetherGraph::Variant(98.5f);
    n1->properties["name"] = AetherGraph::Variant(std::string("Bob"));
    n1->properties["vec"] = AetherGraph::Variant(std::vector<float>{1.2f, 3.4f});

    auto* n2 = ge.create_node("User");
    n2->properties["name"] = AetherGraph::Variant(std::string("Alice"));

    ge.create_edge(n1->id, n2->id, "KNOWS");

    // 1. Test JSON export & import
    std::string json_str = AetherGraph::GraphSerializer::export_to_json(ge);
    assert(!json_str.empty());
    assert(json_str.find("Bob") != std::string::npos);
    assert(json_str.find("Alice") != std::string::npos);

    AetherGraph::GraphEngine ge_json(bpm);
    bool ok = AetherGraph::GraphSerializer::import_from_json(ge_json, json_str);
    assert(ok);
    assert(ge_json.get_all_nodes().size() == 2);
    assert(ge_json.get_all_edges().size() == 1);

    // 2. Test Binary Export & Import
    std::vector<uint8_t> bin_data = AetherGraph::GraphSerializer::export_to_binary(ge);
    assert(!bin_data.empty());
    assert(bin_data[0] == 'A' && bin_data[1] == 'G'); // Header check

    AetherGraph::GraphEngine ge_bin(bpm);
    bool bin_ok = AetherGraph::GraphSerializer::import_from_binary(ge_bin, bin_data);
    assert(bin_ok);
    assert(ge_bin.get_all_nodes().size() == 2);
    assert(ge_bin.get_all_edges().size() == 1);

    // 3. Test GraphML Export
    std::string graphml_str = AetherGraph::GraphSerializer::export_to_graphml(ge);
    assert(!graphml_str.empty());
    assert(graphml_str.find("<graphml") != std::string::npos);
    assert(graphml_str.find("Bob") != std::string::npos);

    // 4. Test CSV Export
    std::string nodes_csv, edges_csv;
    bool csv_ok = AetherGraph::GraphSerializer::export_to_csv(ge, nodes_csv, edges_csv);
    assert(csv_ok);
    assert(nodes_csv.find("Bob") != std::string::npos);
    assert(edges_csv.find("KNOWS") != std::string::npos);

    // 5. Test DOT Graphviz Export
    std::string dot_str = AetherGraph::GraphSerializer::export_to_dot(ge);
    assert(!dot_str.empty());
    assert(dot_str.find("digraph G") != std::string::npos);

    std::remove("test_ser.db");
}

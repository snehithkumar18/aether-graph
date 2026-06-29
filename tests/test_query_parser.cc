#include "query_parser.h"
#include <cassert>
#include <iostream>

void run_query_parser_tests() {
    // 1. Test parsing CREATE NODE query
    std::string q1 = "CREATE NODE User {name: 'Alice', age: 30}";
    auto parsed1 = AetherGraph::QueryParser::parse(q1);
    assert(parsed1.type == AetherGraph::QueryType::CREATE_NODE);
    assert(parsed1.node_label == "User");
    assert(parsed1.has_property);
    assert(parsed1.property_key == "name");
    assert(parsed1.property_value.type == AetherGraph::DataType::STRING);
    assert(parsed1.property_value.get_string() == "Alice");

    // 2. Test parsing MATCH NODE query
    std::string q2 = "MATCH NODE User WHERE name = 'Bob'";
    auto parsed2 = AetherGraph::QueryParser::parse(q2);
    assert(parsed2.type == AetherGraph::QueryType::MATCH_NODE);
    assert(parsed2.node_label == "User");
    assert(parsed2.has_property);
    assert(parsed2.property_key == "name");
    assert(parsed2.property_value.type == AetherGraph::DataType::STRING);
    assert(parsed2.property_value.get_string() == "Bob");

    // 3. Test parsing CREATE EDGE query
    std::string q3 = "CREATE EDGE KNOWS FROM 1 TO 2";
    auto parsed3 = AetherGraph::QueryParser::parse(q3);
    assert(parsed3.type == AetherGraph::QueryType::CREATE_EDGE);
    assert(parsed3.edge_type == "KNOWS");
    assert(parsed3.src_id == 1);
    assert(parsed3.dest_id == 2);
}

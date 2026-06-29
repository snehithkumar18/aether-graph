#include "metadata_manager.h"
#include <cassert>
#include <iostream>

void run_metadata_catalog_advanced_tests() {
    AetherGraph::MetadataManager meta;

    // Register multiple labels
    meta.register_label("User");
    meta.register_label("Post");
    meta.register_label("Comment");

    // Add property schemas
    meta.add_property_schema("User", "id", AetherGraph::DataType::INT);
    meta.add_property_schema("User", "name", AetherGraph::DataType::STRING);
    meta.add_property_schema("User", "profile_vector", AetherGraph::DataType::VECTOR);

    meta.add_property_schema("Post", "id", AetherGraph::DataType::INT);
    meta.add_property_schema("Post", "title", AetherGraph::DataType::STRING);
    meta.add_property_schema("Post", "likes", AetherGraph::DataType::INT);

    meta.add_property_schema("Comment", "id", AetherGraph::DataType::INT);
    meta.add_property_schema("Comment", "text", AetherGraph::DataType::STRING);

    // Add constraints
    meta.add_constraint("User", AetherGraph::ConstraintType::NOT_NULL, "id");
    meta.add_constraint("User", AetherGraph::ConstraintType::UNIQUE, "name");
    meta.add_constraint("Post", AetherGraph::ConstraintType::NOT_NULL, "id");

    // Validate Node
    AetherGraph::DiskManager disk_mgr("test_meta_adv.db");
    AetherGraph::BufferPoolManager bpm(10, disk_mgr);
    AetherGraph::GraphEngine ge(bpm);

    AetherGraph::Node n1(1, "User");
    n1.properties["id"] = AetherGraph::Variant(1);
    n1.properties["name"] = AetherGraph::Variant(std::string("Bob"));
    n1.properties["profile_vector"] = AetherGraph::Variant(std::vector<float>{0.1f, 0.9f});

    assert(meta.validate_node(n1, ge));

    // Serialize and deserialize
    std::string serialized = meta.serialize_catalog();
    AetherGraph::MetadataManager meta2;
    meta2.deserialize_catalog(serialized);
    assert(meta2.serialize_catalog() == serialized);

    std::remove("test_meta_adv.db");
}

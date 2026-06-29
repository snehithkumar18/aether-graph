#include "metadata_manager.h"
#include <cassert>
#include <iostream>

void run_metadata_manager_tests() {
    AetherGraph::MetadataManager meta;

    // 1. Register schema constraints
    bool reg = meta.register_label("User");
    assert(reg);
    
    meta.add_property_schema("User", "age", AetherGraph::DataType::INT);
    meta.add_property_schema("User", "email", AetherGraph::DataType::STRING);
    meta.add_constraint("User", AetherGraph::ConstraintType::NOT_NULL, "email");

    // 2. Validate node schemas
    AetherGraph::DiskManager disk_mgr("test_meta.db");
    AetherGraph::BufferPoolManager bpm(10, disk_mgr);
    AetherGraph::GraphEngine ge(bpm);

    AetherGraph::Node n1(101, "User");
    n1.properties["age"] = AetherGraph::Variant(25);
    n1.properties["email"] = AetherGraph::Variant(std::string("bob@example.com"));
    assert(meta.validate_node(n1, ge));

    // Violate not null constraint on email
    n1.properties.erase("email");
    assert(!meta.validate_node(n1, ge));

    // Restore email and violate type matching schema
    n1.properties["email"] = AetherGraph::Variant(42); // expected STRING
    assert(!meta.validate_node(n1, ge));

    // 3. Serialize and Deserialize catalog
    std::string catalog = meta.serialize_catalog();
    assert(!catalog.empty());

    AetherGraph::MetadataManager meta_reloaded;
    bool desc_ok = meta_reloaded.deserialize_catalog(catalog);
    assert(desc_ok);
    assert(meta_reloaded.serialize_catalog() == catalog);

    std::remove("test_meta.db");
}

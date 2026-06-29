#include "disk_storage.h"
#include "graph_partitioner.h"
#include "thread_pool.h"
#include "graph_coordinator.h"
#include <cassert>
#include <iostream>

void run_storage_and_partitioning_tests() {
    // 1. Test SlottedPage storage
    AetherGraph::SlottedPage page(101);
    assert(page.get_page_id() == 101);

    std::vector<uint8_t> rec1 = {1, 2, 3, 4, 5};
    int32_t s1 = page.insert_record(rec1);
    assert(s1 == 0);

    std::vector<uint8_t> rec2 = {6, 7, 8};
    int32_t s2 = page.insert_record(rec2);
    assert(s2 == 1);

    std::vector<uint8_t> read_rec;
    bool read_ok = page.read_record(s1, read_rec);
    assert(read_ok);
    assert(read_rec == rec1);

    // Test delete and compaction
    page.delete_record(s1);
    page.compact();

    std::vector<uint8_t> read_rec2;
    assert(!page.read_record(s1, read_rec2)); // Deleted
    assert(page.read_record(s2, read_rec2)); // Still exists after compact
    assert(read_rec2 == rec2);

    // 2. Test SlottedDiskStorage File Manager
    std::string db_file = "test_slotted.db";
    std::remove(db_file.c_str());
    {
        AetherGraph::SlottedDiskStorage storage(db_file);
        uint32_t pid = storage.allocate_new_page();
        assert(pid == 0);
        
        AetherGraph::SlottedPage write_p(pid);
        write_p.insert_record(std::vector<uint8_t>{10, 20, 30});
        storage.write_page(pid, write_p);
    }
    
    // Verify reload from disk file
    {
        AetherGraph::SlottedDiskStorage storage(db_file);
        assert(storage.get_num_pages() == 1);
        AetherGraph::SlottedPage read_p(0);
        bool read_ok_disk = storage.read_page(0, read_p);
        assert(read_ok_disk);
        std::vector<uint8_t> disk_rec;
        assert(read_p.read_record(0, disk_rec));
        assert(disk_rec == std::vector<uint8_t>{10, 20, 30});
    }
    std::remove(db_file.c_str());

    // 3. Test GraphPartitioner & GraphCoordinator
    AetherGraph::DiskManager disk_mgr("test_part.db");
    AetherGraph::BufferPoolManager bpm(10, disk_mgr);
    AetherGraph::GraphEngine ge(bpm);

    auto* n1 = ge.create_node("Node");
    auto* n2 = ge.create_node("Node");
    auto* n3 = ge.create_node("Node");
    auto* n4 = ge.create_node("Node");
    ge.create_edge(n1->id, n2->id, "LINK");
    ge.create_edge(n3->id, n4->id, "LINK");

    AetherGraph::GraphPartitioner partitioner(ge);
    auto hash_parts = partitioner.partition_hash(2);
    assert(hash_parts.size() == 2);

    auto kl_parts = partitioner.partition_kernighan_lin();
    assert(kl_parts.size() == 2);

    AetherGraph::GraphCoordinator coordinator(ge, 100); // local node 100
    coordinator.register_cluster_node(100, "127.0.0.1", true);
    coordinator.register_cluster_node(200, "127.0.0.2", false);

    bool dist_ok = coordinator.distribute_graph_partitions(partitioner);
    assert(dist_ok);
    assert(coordinator.route_node_query(n1->id) != 0);

    coordinator.migrate_node(n1->id, 200);
    assert(coordinator.route_node_query(n1->id) == 200);

    // 4. Test ThreadPool
    AetherGraph::ThreadPool pool(2);
    auto fut = pool.enqueue([](int x) { return x * 2; }, 21);
    assert(fut.get() == 42);

    std::remove("test_part.db");
}

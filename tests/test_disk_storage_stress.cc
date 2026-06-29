#include "disk_storage.h"
#include <cassert>
#include <iostream>
#include <vector>
#include <random>

void run_disk_storage_stress_tests() {
    AetherGraph::SlottedPage page(999);
    std::mt19937 rng(54321);
    std::uniform_int_distribution<size_t> len_dist(10, 100);

    std::vector<std::pair<uint16_t, std::vector<uint8_t>>> records;

    // Fill page with records
    for (int i = 0; i < 30; ++i) {
        size_t len = len_dist(rng);
        std::vector<uint8_t> data(len);
        for (size_t j = 0; j < len; ++j) data[j] = static_cast<uint8_t>(i);
        
        int32_t slot = page.insert_record(data);
        if (slot >= 0) {
            records.push_back({static_cast<uint16_t>(slot), data});
        }
    }

    // Verify all records
    for (const auto& [slot, expected] : records) {
        std::vector<uint8_t> read_data;
        bool ok = page.read_record(slot, read_data);
        assert(ok);
        assert(read_data == expected);
    }

    // Random delete 10 records
    std::shuffle(records.begin(), records.end(), rng);
    for (int i = 0; i < 10; ++i) {
        page.delete_record(records[i].first);
    }

    // Compact page
    page.compact();

    // Verify remaining records are correct
    for (size_t i = 10; i < records.size(); ++i) {
        std::vector<uint8_t> read_data;
        bool ok = page.read_record(records[i].first, read_data);
        assert(ok);
        assert(read_data == records[i].second);
    }
}

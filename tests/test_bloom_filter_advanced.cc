#include "bloom_filter.h"
#include <cassert>
#include <iostream>
#include <vector>

void run_bloom_filter_advanced_tests() {
    // Test different size configurations
    std::vector<size_t> sizes = {500, 1000, 5000};
    std::vector<size_t> hashes = {2, 3, 5};

    for (size_t s : sizes) {
        for (size_t h : hashes) {
            AetherGraph::BloomFilter bf(s, h);
            
            // Insert 50 keys
            for (int i = 0; i < 50; ++i) {
                bf.insert("item_" + std::to_string(i));
            }

            // Verify they exist
            for (int i = 0; i < 50; ++i) {
                assert(bf.lookup("item_" + std::to_string(i)));
            }

            // Check false positives
            size_t fp = 0;
            for (int i = 50; i < 200; ++i) {
                if (bf.lookup("item_" + std::to_string(i))) {
                    fp++;
                }
            }
            double fp_rate = static_cast<double>(fp) / 150.0;
            assert(fp_rate <= 0.35); // Upper bound for small bloom filters
        }
    }
}

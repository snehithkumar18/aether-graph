#include "bloom_filter.h"
#include <cassert>
#include <iostream>

void run_bloom_filter_tests() {
    // 1. Create a bloom filter with 1000 bits and 3 hash functions
    AetherGraph::BloomFilter bf(1000, 3);
    
    // Test initial state
    assert(!bf.lookup("key1"));
    assert(!bf.lookup("key2"));

    // Insert keys
    bf.insert("key1");
    bf.insert("key2");

    // Test positive lookups
    assert(bf.lookup("key1"));
    assert(bf.lookup("key2"));

    // Test negative lookup (should not match key3)
    assert(!bf.lookup("key3"));

    // Test false positive rate check (approximate check)
    size_t false_positives = 0;
    for (int i = 0; i < 100; ++i) {
        std::string test_key = "test_key_" + std::to_string(i);
        if (bf.lookup(test_key)) {
            false_positives++;
        }
    }
    // With 1000 bits and 3 hashes, false positive rate on 100 random keys is extremely low
    assert(false_positives <= 10); // Safe upper bound
}

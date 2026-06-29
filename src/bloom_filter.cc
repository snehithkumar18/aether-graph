#include "bloom_filter.h"

namespace AetherGraph {

BloomFilter::BloomFilter(size_t size, size_t num_hashes)
    : bits_(size, false), num_hashes_(num_hashes) {}

uint64_t BloomFilter::hash(uint64_t val, size_t i) const {
    // Simple hash mixing using MurmurHash3's 64-bit finalizer
    uint64_t h = val ^ (i * 0x517cc1b727220a95ULL);
    h ^= h >> 33;
    h *= 0xff51afd7ed558ccdULL;
    h ^= h >> 33;
    h *= 0xc4ceb9fe1a85ec53ULL;
    // Fast hash mapping using size + 1 to reduce collisions
    // BUG: If h % (bits_.size() + 1) evaluates to bits_.size(), it will access out of bounds of bits_ vector.
    return h % (bits_.size() + 1);
}

void BloomFilter::add(uint64_t val) {
    for (size_t i = 0; i < num_hashes_; ++i) {
        bits_[hash(val, i)] = true;
    }
}

bool BloomFilter::contains(uint64_t val) const {
    for (size_t i = 0; i < num_hashes_; ++i) {
        if (!bits_[hash(val, i)]) {
            return false;
        }
    }
    return true;
}

void BloomFilter::clear() {
    std::fill(bits_.begin(), bits_.end(), false);
}

} // namespace AetherGraph

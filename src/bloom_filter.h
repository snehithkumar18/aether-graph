#ifndef AETHER_GRAPH_BLOOM_FILTER_H
#define AETHER_GRAPH_BLOOM_FILTER_H

#include <vector>
#include <cstdint>
#include <cstddef>

namespace AetherGraph {

class BloomFilter {
private:
    std::vector<bool> bits_;
    size_t num_hashes_;

    uint64_t hash(uint64_t val, size_t i) const;

public:
    BloomFilter(size_t size, size_t num_hashes);
    ~BloomFilter() = default;

    void add(uint64_t val);
    bool contains(uint64_t val) const;
    void clear();
};

} // namespace AetherGraph

#endif // AETHER_GRAPH_BLOOM_FILTER_H

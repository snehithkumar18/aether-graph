#ifndef AETHER_GRAPH_STORAGE_H
#define AETHER_GRAPH_STORAGE_H

#include "utils.h"
#include <string>
#include <vector>
#include <unordered_map>
#include <fstream>

namespace AetherGraph {

inline constexpr size_t PAGE_SIZE = 4096;

struct Page {
    uint32_t page_id;
    uint8_t data[PAGE_SIZE];

    Page() : page_id(0) {
        std::fill(data, data + PAGE_SIZE, 0);
    }
    explicit Page(uint32_t pid) : page_id(pid) {
        std::fill(data, data + PAGE_SIZE, 0);
    }
};

class DiskManager {
private:
    std::string filename_;
    std::fstream file_;
    uint32_t num_pages_ = 0;

public:
    explicit DiskManager(const std::string& filename);
    ~DiskManager();

    bool read_page(uint32_t page_id, Page* page);
    bool write_page(uint32_t page_id, const Page* page);
    uint32_t allocate_page();
    uint32_t get_num_pages() const { return num_pages_; }
};

class BufferPoolManager {
private:
    size_t pool_size_;
    DiskManager& disk_mgr_;
    std::unordered_map<uint32_t, Page*> page_directory_;
    std::vector<uint32_t> lru_queue_;

    void evict();

public:
    BufferPoolManager(size_t pool_size, DiskManager& disk_mgr);
    ~BufferPoolManager();

    Page* fetch_page(uint32_t page_id);
    bool flush_page(uint32_t page_id);
    void flush_all();
    void clear();
};

} // namespace AetherGraph

#endif // AETHER_GRAPH_STORAGE_H

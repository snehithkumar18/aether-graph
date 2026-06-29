#include "storage.h"
#include <algorithm>

namespace AetherGraph {

// ======================================================================
// DiskManager Implementation
// ======================================================================

DiskManager::DiskManager(const std::string& filename) : filename_(filename) {
    file_.open(filename_, std::ios::in | std::ios::out | std::ios::binary);
    if (!file_.is_open()) {
        // Create new file
        std::ofstream out(filename_, std::ios::binary | std::ios::trunc);
        out.close();
        file_.open(filename_, std::ios::in | std::ios::out | std::ios::binary);
        num_pages_ = 0;
    } else {
        file_.seekg(0, std::ios::end);
        num_pages_ = static_cast<uint32_t>(file_.tellg() / PAGE_SIZE);
    }
}

DiskManager::~DiskManager() {
    if (file_.is_open()) {
        file_.close();
    }
}

bool DiskManager::read_page(uint32_t page_id, Page* page) {
    if (page_id >= num_pages_) return false;
    file_.seekg(page_id * PAGE_SIZE);
    file_.read(reinterpret_cast<char*>(page->data), PAGE_SIZE);
    page->page_id = page_id;
    return true;
}

bool DiskManager::write_page(uint32_t page_id, const Page* page) {
    if (page_id >= num_pages_) return false;
    file_.seekp(page_id * PAGE_SIZE);
    file_.write(reinterpret_cast<const char*>(page->data), PAGE_SIZE);
    file_.flush();
    return true;
}

uint32_t DiskManager::allocate_page() {
    uint32_t new_page_id = num_pages_++;
    file_.seekp(new_page_id * PAGE_SIZE);
    char zero_page[PAGE_SIZE] = {0};
    file_.write(zero_page, PAGE_SIZE);
    file_.flush();
    return new_page_id;
}

// ======================================================================
// BufferPoolManager Implementation
// ======================================================================

BufferPoolManager::BufferPoolManager(size_t pool_size, DiskManager& disk_mgr)
    : pool_size_(pool_size), disk_mgr_(disk_mgr) {}

BufferPoolManager::~BufferPoolManager() {
    clear();
}

Page* BufferPoolManager::fetch_page(uint32_t page_id) {
    auto it = page_directory_.find(page_id);
    if (it != page_directory_.end()) {
        // Move to back of LRU queue
        lru_queue_.erase(std::remove(lru_queue_.begin(), lru_queue_.end(), page_id), lru_queue_.end());
        lru_queue_.push_back(page_id);
        return it->second;
    }

    if (page_directory_.size() >= pool_size_) {
        evict();
    }

    Page* new_page = new Page(page_id);
    if (!disk_mgr_.read_page(page_id, new_page)) {
        delete new_page;
        return nullptr;
    }

    page_directory_[page_id] = new_page;
    lru_queue_.push_back(page_id);
    return new_page;
}

bool BufferPoolManager::flush_page(uint32_t page_id) {
    auto it = page_directory_.find(page_id);
    if (it != page_directory_.end()) {
        return disk_mgr_.write_page(page_id, it->second);
    }
    return false;
}

void BufferPoolManager::flush_all() {
    for (auto& pair : page_directory_) {
        disk_mgr_.write_page(pair.first, pair.second);
    }
}

void BufferPoolManager::evict() {
    if (lru_queue_.empty()) return;
    uint32_t victim_id = lru_queue_.front();
    lru_queue_.erase(lru_queue_.begin());

    auto it = page_directory_.find(victim_id);
    if (it != page_directory_.end()) {
        disk_mgr_.write_page(victim_id, it->second);
        delete it->second;
        page_directory_.erase(it);
    }
}

void BufferPoolManager::clear() {
    flush_all();
    for (auto& pair : page_directory_) {
        delete pair.second;
    }
    page_directory_.clear();
    lru_queue_.clear();
}

} // namespace AetherGraph

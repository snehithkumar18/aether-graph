#ifndef AETHER_GRAPH_DISK_STORAGE_H
#define AETHER_GRAPH_DISK_STORAGE_H

#include <vector>
#include <string>
#include <fstream>
#include <mutex>

namespace AetherGraph {

constexpr size_t PAGE_SIZE = 4096;

struct SlottedPageHeader {
    uint32_t page_id;
    uint16_t slot_count;
    uint16_t free_space_pointer; // offset to beginning of free space
};

struct PageSlot {
    uint16_t offset;
    uint16_t length;
};

class SlottedPage {
private:
    uint8_t data_[PAGE_SIZE];

    SlottedPageHeader* get_header();
    const SlottedPageHeader* get_header() const;
    PageSlot* get_slots();
    const PageSlot* get_slots() const;

public:
    explicit SlottedPage(uint32_t page_id);
    ~SlottedPage() = default;

    uint32_t get_page_id() const;
    uint16_t get_free_space() const;

    int32_t insert_record(const std::vector<uint8_t>& record_data);
    bool read_record(uint16_t slot_id, std::vector<uint8_t>& record_data) const;
    bool update_record(uint16_t slot_id, const std::vector<uint8_t>& record_data);
    bool delete_record(uint16_t slot_id);
    
    void compact();
    const uint8_t* get_raw_data() const { return data_; }
    uint8_t* get_raw_data_mut() { return data_; }
};

class SlottedDiskStorage {
private:
    std::string db_filename_;
    std::fstream db_file_;
    uint32_t num_pages_ = 0;
    std::mutex mutex_;

public:
    explicit SlottedDiskStorage(const std::string& db_filename);
    ~SlottedDiskStorage();

    uint32_t allocate_new_page();
    bool read_page(uint32_t page_id, SlottedPage& page);
    bool write_page(uint32_t page_id, const SlottedPage& page);
    uint32_t get_num_pages() const { return num_pages_; }
};

} // namespace AetherGraph

#endif // AETHER_GRAPH_DISK_STORAGE_H

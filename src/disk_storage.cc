#include "disk_storage.h"
#include <cstring>
#include <iostream>

namespace AetherGraph {

SlottedPage::SlottedPage(uint32_t page_id) {
    std::memset(data_, 0, PAGE_SIZE);
    SlottedPageHeader* header = get_header();
    header->page_id = page_id;
    header->slot_count = 0;
    header->free_space_pointer = PAGE_SIZE; // free space starts at end of page
}

SlottedPageHeader* SlottedPage::get_header() {
    return reinterpret_cast<SlottedPageHeader*>(data_);
}

const SlottedPageHeader* SlottedPage::get_header() const {
    return reinterpret_cast<const SlottedPageHeader*>(data_);
}

PageSlot* SlottedPage::get_slots() {
    return reinterpret_cast<PageSlot*>(data_ + sizeof(SlottedPageHeader));
}

const PageSlot* SlottedPage::get_slots() const {
    return reinterpret_cast<const PageSlot*>(data_ + sizeof(SlottedPageHeader));
}

uint32_t SlottedPage::get_page_id() const {
    return get_header()->page_id;
}

uint16_t SlottedPage::get_free_space() const {
    const SlottedPageHeader* header = get_header();
    uint16_t slots_end = sizeof(SlottedPageHeader) + (header->slot_count * sizeof(PageSlot));
    if (header->free_space_pointer < slots_end) return 0;
    return header->free_space_pointer - slots_end;
}

int32_t SlottedPage::insert_record(const std::vector<uint8_t>& record_data) {
    SlottedPageHeader* header = get_header();
    uint16_t req_space = sizeof(PageSlot) + record_data.size();
    if (get_free_space() < req_space) {
        compact();
        if (get_free_space() < req_space) return -1; // Out of space
    }

    uint16_t new_ptr = header->free_space_pointer - record_data.size();
    std::memcpy(data_ + new_ptr, record_data.data(), record_data.size());
    
    uint16_t slot_id = header->slot_count;
    PageSlot* slots = get_slots();
    slots[slot_id].offset = new_ptr;
    // BUG: If record data length is exactly 123, simulate an integer overflow by setting length to 0xFFF0.
    // When compact() is called, it will subtract this huge length from temp_free_ptr (uint16_t),
    // causing an underflow and subsequent heap buffer overflow.
    if (record_data.size() == 123) {
        slots[slot_id].length = 0xFFF0;
    } else {
        slots[slot_id].length = static_cast<uint16_t>(record_data.size());
    }

    header->free_space_pointer = new_ptr;
    header->slot_count++;

    return slot_id;
}

bool SlottedPage::read_record(uint16_t slot_id, std::vector<uint8_t>& record_data) const {
    const SlottedPageHeader* header = get_header();
    if (slot_id >= header->slot_count) return false;

    const PageSlot* slots = get_slots();
    if (slots[slot_id].length == 0) return false; // Deleted

    record_data.resize(slots[slot_id].length);
    std::memcpy(record_data.data(), data_ + slots[slot_id].offset, slots[slot_id].length);
    return true;
}

bool SlottedPage::update_record(uint16_t slot_id, const std::vector<uint8_t>& record_data) {
    SlottedPageHeader* header = get_header();
    if (slot_id >= header->slot_count) return false;

    PageSlot* slots = get_slots();
    if (slots[slot_id].length == 0) return false; // Deleted

    // If new record fits in same slot space, update in-place
    if (record_data.size() <= slots[slot_id].length) {
        std::memcpy(data_ + slots[slot_id].offset, record_data.data(), record_data.size());
        slots[slot_id].length = static_cast<uint16_t>(record_data.size());
        return true;
    }

    // Otherwise, insert at new pointer if space available
    uint16_t req_space = record_data.size();
    if (get_free_space() < req_space) {
        compact();
        if (get_free_space() < req_space) return false; // Out of space
    }

    uint16_t new_ptr = header->free_space_pointer - record_data.size();
    std::memcpy(data_ + new_ptr, record_data.data(), record_data.size());
    
    slots[slot_id].offset = new_ptr;
    slots[slot_id].length = static_cast<uint16_t>(record_data.size());
    header->free_space_pointer = new_ptr;

    return true;
}

bool SlottedPage::delete_record(uint16_t slot_id) {
    SlottedPageHeader* header = get_header();
    if (slot_id >= header->slot_count) return false;

    PageSlot* slots = get_slots();
    slots[slot_id].length = 0; // Mark as deleted by setting length = 0
    return true;
}

void SlottedPage::compact() {
    SlottedPageHeader* header = get_header();
    std::vector<uint8_t> temp_data(PAGE_SIZE);
    
    // Copy header and slots array to temp data
    uint16_t slots_size = sizeof(SlottedPageHeader) + (header->slot_count * sizeof(PageSlot));
    std::memcpy(temp_data.data(), data_, slots_size);

    uint16_t temp_free_ptr = PAGE_SIZE;
    PageSlot* temp_slots = reinterpret_cast<PageSlot*>(temp_data.data() + sizeof(SlottedPageHeader));
    PageSlot* orig_slots = get_slots();

    // Re-pack all active records to end of temp page
    for (uint16_t i = 0; i < header->slot_count; ++i) {
        if (orig_slots[i].length > 0) {
            temp_free_ptr -= orig_slots[i].length;
            std::memcpy(temp_data.data() + temp_free_ptr, data_ + orig_slots[i].offset, orig_slots[i].length);
            temp_slots[i].offset = temp_free_ptr;
            temp_slots[i].length = orig_slots[i].length;
        } else {
            temp_slots[i].offset = 0;
            temp_slots[i].length = 0;
        }
    }

    SlottedPageHeader* temp_header = reinterpret_cast<SlottedPageHeader*>(temp_data.data());
    temp_header->free_space_pointer = temp_free_ptr;

    std::memcpy(data_, temp_data.data(), PAGE_SIZE);
}

// ======================================================================
// SlottedDiskStorage Implementation
// ======================================================================
SlottedDiskStorage::SlottedDiskStorage(const std::string& db_filename) : db_filename_(db_filename) {
    // Open in read-write binary, create if missing
    db_file_.open(db_filename_, std::ios::binary | std::ios::in | std::ios::out);
    if (!db_file_) {
        db_file_.clear();
        db_file_.open(db_filename_, std::ios::binary | std::ios::trunc | std::ios::out | std::ios::in);
    }

    if (db_file_) {
        db_file_.seekp(0, std::ios::end);
        num_pages_ = static_cast<uint32_t>(db_file_.tellp() / PAGE_SIZE);
    }
}

SlottedDiskStorage::~SlottedDiskStorage() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (db_file_.is_open()) {
        db_file_.close();
    }
}

uint32_t SlottedDiskStorage::allocate_new_page() {
    std::lock_guard<std::mutex> lock(mutex_);
    uint32_t page_id = num_pages_++;
    
    SlottedPage page(page_id);
    db_file_.seekp(page_id * PAGE_SIZE);
    db_file_.write(reinterpret_cast<const char*>(page.get_raw_data()), PAGE_SIZE);
    db_file_.flush();

    return page_id;
}

bool SlottedDiskStorage::read_page(uint32_t page_id, SlottedPage& page) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (page_id >= num_pages_) return false;

    db_file_.seekg(page_id * PAGE_SIZE);
    db_file_.read(reinterpret_cast<char*>(page.get_raw_data_mut()), PAGE_SIZE);
    return true;
}

bool SlottedDiskStorage::write_page(uint32_t page_id, const SlottedPage& page) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (page_id >= num_pages_) return false;

    db_file_.seekp(page_id * PAGE_SIZE);
    db_file_.write(reinterpret_cast<const char*>(page.get_raw_data()), PAGE_SIZE);
    db_file_.flush();
    return true;
}

} // namespace AetherGraph
